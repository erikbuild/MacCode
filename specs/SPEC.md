# MacCode — Claude Code for the Macintosh SE

**Status:** Draft for review
**Date:** 2026-06-06
**Author:** Erik & Claude
**Target:** Macintosh SE, System 7.0.1, 68000, 1-bit 512×342 display, via Retro68 + MacTCP. Primary dev target: Basilisk II (MacTCP-configured); secondary: real Mac SE hardware.

---

## 1. Overview

A native classic-Macintosh application that reproduces the look and feel of the Claude Code terminal interface on a Macintosh SE, backed by a real Claude Code session running on a modern Mac.

The SE app is a thin client. It renders a Mac-native re-creation of the Claude Code experience — a scrolling transcript, a `✻` status/verb line, tool-call activity, and a prompt input box at the bottom — and talks to a small proxy daemon over a plain TCP socket using a tiny, fixed, typed record protocol. The proxy drives an actual Claude Code session via the Claude Agent SDK, holds the Anthropic credentials, performs all TLS, and translates the SDK's rich event stream to and from the record protocol.

The design principle throughout: **keep the SE dumb.** Everything hard — TLS, JSON, the agent loop, UTF-8 — lives on the modern Mac.

This project is built inside the **VibeRetro68** template and reuses its toolchain and emulator tooling (`add_application` CMake macro, `scripts/run-basiliskii.sh`, the global Retro68 toolchain).

---

## 2. Goals

- Reproduce the *experience* of Claude Code on a Mac SE: type a prompt at the bottom, watch Claude's reply stream into a transcript with the `✻` verb line and tool-call bullets above the input.
- Interactive back-and-forth REPL against a real Claude Code session operating in one project directory on the modern Mac.
- Tool-use permission prompts surfaced on the SE (Allow / Deny).
- Interrupt a running turn from the SE (`esc`).
- Scroll back through the conversation transcript.
- Start a new conversation or resume the most recent one in the project.
- Run cleanly under Basilisk II with MacTCP; be portable to real SE hardware.

## 3. Non-Goals (v1)

- No raw-terminal/VT100 emulation. We re-create Claude Code natively; we do **not** mirror its ANSI TUI byte-for-byte.
- No multiple concurrent conversations, no project picker from the SE, no full session history browser. (Sessions are limited to "new" + "resume most recent," one fixed project per proxy launch.)
- No TLS, no Anthropic credentials, and no JSON parsing on the SE.
- No file editing/diff viewing UI on the SE beyond textual tool-activity lines. The SE observes what Claude does; it does not render diffs or open files.
- No syntax highlighting, color (the SE is 1-bit), or images.
- No authentication between the SE and the proxy beyond network reachability (LAN-trust model — see §9).

---

## 4. Architecture

Three layers:

```
① Mac SE (System 7.0.1)        ② Modern Mac (proxy)            ③ Claude Code
   MacCode.app             maccode-relay (TS/Node)          real session
   - Mac window UI         <TCP>  - listens on TCP :PORT   <SDK>  - your project dir
   - transcript / ✻ verb          - record ⇄ event xlate          - Read/Edit/Bash/…
   - Allow/Deny dialog            - UTF-8 ⇄ Mac Roman              - its own tools/loop
   - MacTCP stream client         - holds auth, does TLS    <TLS> → Anthropic API
   (no JSON, no TLS, no keys)     - one project (configurable)
```

- **SE ⇄ proxy:** a tiny typed-record protocol over a single TCP stream (plaintext, LAN). See §7.
- **proxy ⇄ Claude Code:** the Claude Agent SDK (`@anthropic-ai/claude-agent-sdk`), which provides the structured event stream, a `canUseTool` permission callback, interrupt, resume, and a configurable working directory.
- **proxy ⇄ Anthropic:** standard TLS, performed by Claude Code / the SDK using the modern Mac's existing Claude Code authentication.

---

## 5. Components

### 5.1 SE client app — MacCode

Language: C (Retro68), classic Mac Toolbox. Single application, one document window.

**Window layout** (full screen, 512×342, minus the 20px menu bar):

```
┌───────────────────────────────────────────────┐  ← title bar: "MacCode"
│ transcript (scrollable)                      ▲ │
│   > fix the parser bug                       ▒ │  ← scrollbar (right)
│   I found the off-by-one in scan()…          ▒ │
│   ● Read main.c                              ▼ │
├───────────────────────────────────────────────┤
│ ✻ Forging…                       (esc to stop) │  ← verb / status line
├───────────────────────────────────────────────┤
│ > ▌                                            │  ← input box
└───────────────────────────────────────────────┘
```

**Sub-components:**

- **Transcript view** — a custom scrollback buffer with manual drawing, *not* TextEdit. Rationale: TextEdit has a hard 32 KB total limit and is awkward for append-and-autoscroll; a custom bounded line buffer gives predictable memory use and full control of scrolling on limited RAM. Holds a bounded ring of rendered lines (target budget ~32–64 KB of text; oldest lines dropped). Right-edge scrollbar (standard `VScroll` control). Distinguishes message kinds visually with simple prefixes/typography: user prompts (bold `>`), assistant text (plain), tool activity (`●` bullet).
- **Verb / status line** — single line above the input showing the `✻` Claude glyph plus the current gerund verb ("Forging…", "Cogitating…") while a turn runs, and "(esc to stop)". Idle when no turn is active. The `✻` glyph is drawn by the SE (small hand-drawn/`PICT` icon), not transmitted as a character.
- **Input box** — a TextEdit field for composing the prompt. Return sends (Shift-Return inserts a newline). Disabled/greyed while a turn is running or while a permission prompt is open.
- **Permission dialog** — modal (or in-window) Allow / Deny prompt showing the tool name and a one-line description of the requested action, rendered from an `ASK` record.
- **Menus** — Apple (About), File (New Conversation, Resume Last, Quit), Edit (standard, for the input field).

**State model** (drives UI enable/disable and what records are accepted):

```
Disconnected → Connecting → Idle ⇄ AwaitingResponse ⇄ AwaitingPermission
                                ↘ Error ↗
```

**MacTCP usage:** Opens one TCP stream to the proxy via MacTCP (the `.IPP` driver: `TCPCreate`, `TCPActiveOpen`, `TCPSend`, `TCPRcv`, `TCPClose`). Receives are integrated into the main `WaitNextEvent` loop using asynchronous parameter blocks / non-blocking reads so the UI stays responsive — the app never blocks on the socket. This is the single trickiest part of the SE app (see §12 Risks).

**Text encoding:** The SE works in **Mac Roman**. All UTF-8 ⇄ Mac Roman transcoding happens in the proxy (§5.2); the SE treats record payloads as Mac Roman bytes and draws them directly.

**Configuration:** The SE needs the proxy's host and port. v1: a "Connect…" dialog (and/or an editable `STR ` resource default) holding `host:port`, persisted in a small preferences file in the System Folder. Default host is the Basilisk II slirp gateway `10.0.2.2` (see §9).

### 5.2 Proxy daemon — maccode-relay

Language: TypeScript on Node. Single long-running process started by Erik on the modern Mac.

**Responsibilities:**

- **TCP server** (Node `net`) accepting one SE client connection at a time (v1: single client; additional connections are rejected or replace the prior one — TBD, see §13).
- **Drive Claude Code** via the Claude Agent SDK: start/resume a session with `cwd` set to the configured project directory; consume the streaming event iterator; expose `interrupt()`; handle the `canUseTool` permission callback.
- **Translate events → records:** map SDK events to proxy→SE records (assistant text deltas → `TEXT`; status/verb → `VERB`; tool start/result → `TOOL`; permission request → `ASK`; turn end → `DONE`; errors → `ERR`; session/cwd info → `INFO`).
- **Translate records → actions:** map SE→proxy records to SDK actions (`PROMPT` → send user message; `PERM` → resolve the pending `canUseTool`; `STOP` → `interrupt()`; `NEW`/`RESUME` → start/resume session).
- **UTF-8 ⇄ Mac Roman transcoding:** outbound, map Claude's UTF-8 to Mac Roman, translating common typographic characters that Mac Roman supports (em dash `—`→0xD1, curly quotes→0xD2–0xD5, ellipsis `…`→0xC9, bullet `•`→0xA5) and degrading unmappable characters to the nearest ASCII (or `?`). Inbound (SE prompts) Mac Roman → UTF-8.
- **Hold auth / do TLS:** relies on the modern Mac's existing Claude Code authentication; the SE never sees credentials.
- **Project scope:** one project directory, set at proxy launch (CLI arg / env / config file).

The proxy is where essentially all complexity lives and is fully unit- and integration-testable (§10).

> **Driver choice:** the Agent SDK is the chosen integration. If a needed capability (e.g. resume semantics, partial-text streaming, or the permission callback) turns out to be cleaner via the `claude` CLI in `--output-format stream-json` mode, that is a drop-in alternative behind the same translation layer. Exact SDK API names will be confirmed against current Claude Agent SDK docs during planning/implementation.

---

## 6. Session model (v1)

- The proxy is launched against **one project directory**.
- From the SE, the user can:
  - **New Conversation** (`NEW`) — start a fresh Claude Code session in that project.
  - **Resume Last** (`RESUME`) — resume the most recent session in that project.
- On connect, the proxy auto-starts (or resumes) a session and sends an `INFO` record describing the project/cwd and session state so the SE can show context.
- No switching between multiple live sessions; no cross-project selection.

---

## 7. Wire protocol (SE ⇄ proxy)

A single TCP stream carrying a sequence of length-framed, typed records. The SE parses with a `switch` on the type byte; no general parser.

### 7.1 Framing

```
+--------+-------------------+-----------------------+
| type   | length (uint16 BE)| payload (length bytes)|
| 1 byte | 2 bytes           | 0..65535 bytes        |
+--------+-------------------+-----------------------+
```

- `type` — one byte identifying the record (values below).
- `length` — big-endian unsigned 16-bit payload length.
- `payload` — Mac Roman text or a small fixed structure, per record type.

Records ≤ 64 KB. The proxy chunks long assistant output across multiple `TEXT` records. (16-bit length chosen for SE simplicity; chunking handles anything larger.)

### 7.2 Records: proxy → SE

| Type | Name | Payload | Meaning |
|------|------|---------|---------|
| `TEXT` | assistant text | Mac Roman text | One assistant text block, emitted as it completes; SE appends to transcript. (The Agent SDK surfaces whole assistant messages, not token-level deltas, so "streaming" is per assistant message, not per character.) |
| `VERB` | status/verb | Mac Roman text | Current verb line ("Forging…"); empty payload clears it. |
| `TOOL` | tool activity | Mac Roman text | A one-line tool-activity summary, e.g. `● Edit main.c (+12 −3)`. |
| `ASK`  | permission request | id (uint32 BE) + Mac Roman text | Claude requests approval; text is a one-line description; `id` correlates the reply. |
| `DONE` | turn complete | (empty) | Current turn finished; SE re-enables input, clears verb. |
| `INFO` | notice/context | Mac Roman text | Session/cwd info or a non-error notice for the transcript or title. |
| `ERR`  | error | Mac Roman text | An error to display (Claude error, proxy issue). |

### 7.3 Records: SE → proxy

| Type | Name | Payload | Meaning |
|------|------|---------|---------|
| `HELLO` | handshake | version (uint16 BE) | Sent on connect; proxy replies with `INFO`. |
| `PROMPT` | user input | Mac Roman text | A user prompt to send to Claude. |
| `PERM` | permission reply | id (uint32 BE) + result (1 byte: 0=deny,1=allow) | Answers an `ASK`. |
| `STOP` | interrupt | (empty) | Interrupt the running turn (esc). |
| `NEW`  | new conversation | (empty) | Start a fresh session. |
| `RESUME` | resume | (empty) | Resume the most recent session. |

### 7.4 Representative flows

**Prompt → response:**
```
SE → PROMPT "fix the parser bug"
proxy → VERB "Cogitating…"
proxy → TEXT "I found the off-by-one in scan()…"   (one or more)
proxy → TOOL "● Read main.c"
proxy → TOOL "● Edit main.c (+12 −3)"
proxy → VERB ""            (cleared)
proxy → DONE
```

**Permission:**
```
proxy → ASK  id=7 "Run: npm test"
SE   → (shows Allow/Deny dialog)
SE   → PERM id=7 allow
proxy → TOOL "● Bash npm test"
…
```

**Interrupt:**
```
SE → STOP
proxy → INFO "Interrupted."
proxy → DONE
```

**Connect:**
```
SE → HELLO version=1
proxy → INFO "Project: ~/code/myapp · session resumed"
(ready)
```

---

## 8. Error handling & resilience

- **Proxy not running / connect fails:** SE shows a clear "Can't reach proxy at host:port" state with a retry affordance; stays in `Disconnected`.
- **TCP drop mid-session:** SE returns to `Disconnected`, preserves the visible transcript, and offers reconnect. On reconnect the proxy sends fresh `INFO`; transcript continuity across reconnects is best-effort (the SE keeps what it already received; it does not re-fetch history in v1).
- **Claude/SDK error:** proxy sends `ERR` with a readable message and a `DONE`; SE re-enables input.
- **Malformed/overlong record:** receiver drops the connection rather than risk desync (records are length-framed, so this is detectable).
- **Permission timeout:** if the SE never answers an `ASK`, the proxy leaves the turn paused; an SE `STOP` cancels it.

---

## 9. Security considerations

- **Plaintext on a trusted LAN.** The SE↔proxy link is unencrypted (the SE can't do TLS). This is acceptable only on a trusted local network. The proxy must **not** be exposed to untrusted networks.
- **Bind address.** The emulated SE reaches the host through the Basilisk II slirp gateway at `10.0.2.2`; slirp forwards that to the host, so the proxy listens on the host. The exact bind interface (loopback vs the host's reachable interface) is confirmed by the connectivity spike before UI work. The proxy must **not** be exposed on untrusted networks.
- **Auth stays on the modern Mac.** Anthropic credentials live only in the proxy/Claude Code; the SE holds nothing sensitive.
- **Permission gating is real.** Tool approvals are surfaced to the user via `ASK`/`PERM`; the proxy must not auto-approve. The human at the SE is the approval authority, exactly as in real Claude Code.
- **Single client.** v1 accepts one SE connection; this limits exposure and avoids multi-client state.

---

## 10. Testing strategy

Per project policy: unit, integration, and end-to-end coverage. Split by where logic lives.

**Proxy (TS/Node) — fully testable, TDD:**
- **Unit:** record encode/decode (framing, type bytes, lengths, chunking), UTF-8 ⇄ Mac Roman transcoding (incl. typographic mappings and unmappable degradation), event→record and record→action translation.
- **Integration:** a mock SE socket client exercising full flows (prompt→stream→done, permission allow/deny, interrupt, new/resume) against a **mocked Agent SDK** boundary — asserting the proxy emits the correct record sequences. (The SDK itself is mocked; we test our translation logic, never the SDK's behavior.)
- **End-to-end:** an opt-in test that drives a real Claude Agent SDK session in a scratch project through the proxy over a real socket, asserting a real round-trip. Gated behind credentials/availability.

**SE client (C) — split testable / not:**
- **Pure-logic unit tests (off-device):** the protocol codec, Mac Roman handling, line-wrapping, and transcript ring-buffer are written as pure C with no Toolbox dependencies and compiled & unit-tested natively on the host (a small native test target), independent of Retro68.
- **GUI / MacTCP:** not unit-testable on 68K. Verified by:
  - **Scripted e2e** where feasible via LaunchAPPL + Mini vMac for headless logic round-trips (exit-code assertions), if the networking path permits it under Mini vMac; otherwise
  - **Manual verification** in Basilisk II following a written test checklist (connect, prompt, stream, tool verbs, permission allow/deny, interrupt, scrollback, new/resume, disconnect/reconnect).

> Any test type that genuinely cannot be automated for the SE GUI will be called out explicitly for Erik's authorization rather than silently skipped.

---

## 11. Build & run

- **SE app:** a `add_application(...)` target in the project `CMakeLists.txt` (C sources + a `.r` resource file), built against the global Retro68 toolchain; deployed/tested via `scripts/run-basiliskii.sh`.
- **Proxy (`maccode-relay`):** a Node/TypeScript package in its own subdirectory (`proxy/`) with `npm` scripts (`build`, `start`, `test`); launched manually against a project directory.
- A short top-level doc will describe the end-to-end dev loop: start proxy → launch SE app in Basilisk II → connect → iterate.

---

## 12. Risks

- **MacTCP async I/O** integrated with `WaitNextEvent` is the highest-risk SE component; classic MacTCP is finicky. Mitigation: build a minimal "connect + echo" MacTCP spike first, before any UI.
- **Basilisk II ⇄ host networking/addressing** must be pinned down early (which IP the SE dials). Mitigation: validate reachability with the spike.
- **Transcript memory** on a RAM-constrained machine. Mitigation: bounded ring buffer, custom drawing (not TextEdit).
- **Agent SDK specifics** (resume, partial-text streaming, permission callback shape) to be confirmed against current docs. Mitigation: confirm during planning; CLI stream-json fallback exists behind the same translation layer.
- **Encoding fidelity** (UTF-8 → Mac Roman) for code-heavy output. Mitigation: explicit mapping table + tests; degrade gracefully.

---

## 13. Resolved decisions

1. **Proxy↔SE addressing:** the SE dials the Basilisk II slirp gateway `10.0.2.2:<port>`; the proxy listens on the host. Confirmed by the connectivity spike before UI work.
2. **Naming:** the SE app is **MacCode**; the proxy is **maccode-relay**.
3. **Reconnect/history:** v1 keeps only the transcript already received; no history replay on reconnect.
4. **Multiple clients:** single client only; the proxy rejects additional connections with an `ERR`.
5. **Project directory:** fixed at proxy launch via CLI arg/env.
6. **Verb text:** use the verbs the SDK surfaces; fall back to a small local gerund list when none is available.

---

## 14. Success criteria

v1 is done when, from a Macintosh SE in Basilisk II (System 7.0.1), Erik can:

1. Launch the app, connect to the proxy, and see project context.
2. Type a prompt and watch Claude's reply stream into the transcript with the `✻` verb line and `●` tool-activity bullets.
3. Be prompted to Allow/Deny a tool action and have that decision honored.
4. Press `esc` to interrupt a running turn.
5. Scroll back through the conversation.
6. Start a new conversation and resume the most recent one.
7. Survive a proxy disconnect/reconnect without crashing.

…and it *feels* like Claude Code — unmistakably the same interface, rendered as a real Mac SE application.
