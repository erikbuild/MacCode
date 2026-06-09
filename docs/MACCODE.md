# MacCode — Claude Code for the Macintosh SE

MacCode is a native System 7 application for a 68K Macintosh (e.g. a Mac SE) that re-creates the Claude Code terminal UI on classic hardware and relays a Claude Agent SDK session running on a modern Mac. There are three layers: the **SE app** (C/Retro68 + the classic Toolbox + MacTCP) talks over a tiny typed-record **wire protocol** on plain TCP to a **Node/TypeScript proxy** (the Claude Agent SDK). There is no SSL — it is plain TCP intended for a trusted LAN or the emulator's slirp link.

This guide takes a fresh user from clone to talking to Claude on the SE.

---

## Prerequisites

- **macOS host.** Run `make setup` once to populate `deps/` with the Retro68 cross-toolchain and a preconfigured Basilisk II (System 7.x) that has MacTCP over slirp. These dependencies live under `deps/` and are self-contained.
- **Node.js** for the proxy.
- **An authenticated Claude environment** for the Agent SDK. The proxy uses your existing Claude credentials.

---

## Build the SE app

Build the **`MacCode_APPL`** target:

```bash
cmake --build build --target MacCode_APPL
```

**Important:** build `MacCode_APPL`, not the bare `MacCode` target. `MacCode` only links the code; `MacCode_APPL` runs the Rez/packaging step that produces the runnable `build/MacCode.bin` and `.dsk`. If `MacCode.bin`'s timestamp doesn't update after a build, you built the wrong target.

**Simpler:** use the run script, which builds everything, deploys `MacCode.bin` into Basilisk II's shared folder, and launches the emulator:

```bash
scripts/run-basiliskii.sh MacCode
```

---

## Run the proxy

**Start the proxy before launching the app.**

```bash
npm --prefix proxy start -- --project <project-dir> --port 4242
```

### Flags

| Flag | Default | Purpose |
|------|---------|---------|
| `--project` | current working directory | Working dir for the Claude session **and** for `!` shell commands. |
| `--port` | `4242` | TCP port to listen on. |
| `--host` | `0.0.0.0` | Bind address. The default binds all interfaces so the emulator can reach the host. |
| `--model` | — | Optional model override. |
| `--echo` | off | A bring-up echo server. Not for normal use. |

### Notes

- Run the proxy from a shell where `tsx` resolves. The `npm --prefix proxy start --` form handles that for you.
- The proxy logs timestamped lines to stderr — client connect, `<- prompt`, `-> text`, `shell:`, and similar — so you can watch a session as it runs.

---

## Launch and connect

1. Start the proxy (see above).
2. Run `scripts/run-basiliskii.sh MacCode`, then open **MacCode** inside the emulated Mac.

On launch, the app connects to `10.0.2.2:4242` — `10.0.2.2` is the slirp gateway address, which is the host. **Start the proxy first.** If the proxy is down or unreachable, the app reports "could not connect"; because the connect is non-blocking, the app stays responsive. Use **Session ▸ Connect / Disconnect** to retry.

---

## Using it

Type a prompt and press **Return**. The `✻` verb line animates while Claude works, `●` tool lines show tool calls, and the reply streams into the scrolling transcript.

- **Tool permissions.** Operations the proxy deems dangerous (or that aren't pre-approved) pop an **Allow / Deny** dialog on the SE. **Deny is the default** — pressing Return denies — so an accidental keypress can't approve anything. Safe and allow-listed operations run without asking.
- **`esc`.** Interrupts the current turn. It also cancels an in-progress connect.
- **File ▸ New Conversation / Resume Last.** Start a fresh conversation or resume the most recent one. Available when idle.
- **`!` bang commands.** Type `!somecommand` to run a shell command directly on the **proxy host**, in the project dir, with its output streamed back. This **bypasses Claude** and — since you invoked it yourself — shows **no permission dialog**. Note the trust implication: this is arbitrary shell execution on the host.
- **View ▸ Dark Mode.** White-on-black. Session-only — it resets to light on relaunch.
- **Scrollback.** Scroll up to read history. The view follows new output only when you're already scrolled to the bottom.

---

## Permissions model

The proxy runs the Agent SDK with your host's settings, so it honors the allow-rules in `~/.claude` and the project's `.claude/settings*.json`. Safe and allow-listed operations auto-run; dangerous or unlisted ones prompt the Allow / Deny dialog on the SE.

---

## Known limits and notes

- **Single client only** — one SE at a time.
- Text is **Mac Roman**; characters with no Mac Roman equivalent render as `?`.
- The proxy should not stream very large output while a permission ask is outstanding — the SE pauses reading during the modal dialog.
- **Clipboard sharing is not yet implemented.**

---

## Manual QA / smoke-test checklist

Run an end-to-end pass:

- [ ] Connect to the proxy.
- [ ] Send a prompt and get a streamed reply with verb (`✻`) and tool (`●`) lines.
- [ ] Trigger a tool and **Allow** it, then trigger one and **Deny** it.
- [ ] Press `esc` to interrupt a long reply.
- [ ] Scroll back through history.
- [ ] **File ▸ New Conversation**, then **File ▸ Resume Last**.
- [ ] Run a `!ls` bang command.
- [ ] Toggle **View ▸ Dark Mode**.
- [ ] Kill the proxy mid-session, then reconnect via **Session ▸ Connect**.
- [ ] Confirm no crashes throughout.
