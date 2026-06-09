# MacCode — Claude Code for the Macintosh SE

MacCode is a native System 7 application for a 68K Macintosh (e.g. a Mac SE) that re-creates the Claude Code terminal UI on classic hardware and relays a Claude Agent SDK session running on a modern Mac. Three layers:

- the **SE app** — C/Retro68 + the classic Toolbox + MacTCP
- a tiny typed-record **wire protocol** over plain TCP (no SSL — trusted LAN / the emulator's slirp link)
- a **Node/TypeScript proxy** — the Claude Agent SDK

You type prompts on the SE; Claude's replies stream back into a scrolling Monaco transcript with an animated ✻ verb line and ● tool lines, tool use is gated by an Allow/Deny dialog, and `!` commands run a host shell.

> Built on the [VibeRetro68](docs/RETRO68_SETUP.md) template (Retro68 toolchain + Basilisk II). The toolchain/emulator setup guides live in [`docs/`](docs/).

---

## Prerequisites

- **macOS host.** Run `make setup` once to populate `deps/` with the Retro68 cross-toolchain and a preconfigured Basilisk II (System 7.x) with MacTCP over slirp. `make setup` runs four idempotent steps — `brew bundle`, `make fetch-deps`, `make build-retro68` (~30–60 min, one-time), `make doctor` — see [docs/RETRO68_SETUP.md](docs/RETRO68_SETUP.md) and [docs/EMULATOR_SETUP.md](docs/EMULATOR_SETUP.md) for detail. `deps/` is gitignored; every clone builds its own.
- **Node.js** for the proxy.
- **An authenticated Claude environment** for the Agent SDK (the proxy uses your existing Claude credentials).

---

## Build the SE app

```bash
cmake --build build --target MacCode_APPL
```

**Important:** build the **`MacCode_APPL`** target, not the bare `MacCode`. `MacCode` only links the code; `MacCode_APPL` runs the Rez/packaging step that produces the runnable `build/MacCode.bin` / `.dsk` (and sets the Finder bundle bit for the app icon). If `MacCode.bin`'s timestamp doesn't update after a build, you built the wrong target.

Simpler — the run script builds everything, deploys `MacCode.bin` into Basilisk II's shared folder, and launches the emulator:

```bash
scripts/run-basiliskii.sh MacCode
```

---

## Run the proxy

**Start the proxy before launching the app.** From the directory you want Claude to work in:

```bash
make proxy                 # or: scripts/run-proxy.sh
```

That serves the current directory as the project on port 4242 (and installs the proxy's
dependencies on first run). Flags pass through to override the defaults:

```bash
scripts/run-proxy.sh --project ~/some/project --port 4242
```

| Flag | Default | Purpose |
|------|---------|---------|
| `--project` | directory you run it from | Working dir for the Claude session **and** for `!` shell commands. |
| `--port` | `4242` | TCP port to listen on. |
| `--host` | `0.0.0.0` | Bind address; the default lets the emulator reach the host. |
| `--model` | — | Optional model override. |
| `--echo` | off | A bring-up echo server. Not for normal use. |

The proxy logs timestamped lines to stderr (client connect, `<- prompt`, `-> text`, `shell:`, …).

---

## Launch and connect

1. Start the proxy (above).
2. `scripts/run-basiliskii.sh MacCode`, then open **MacCode** inside the emulated Mac.

On launch it connects to `10.0.2.2:4242` (the slirp gateway = the host). If the proxy is down it reports "could not connect" but stays responsive (the connect is non-blocking). Use **Session ▸ Connect / Disconnect** to retry.

---

## Using it

Type a prompt and press **Return**. The ✻ verb line animates while Claude works, ● tool lines show tool calls, and the reply streams into the transcript.

- **Tool permissions.** Dangerous / not-pre-approved operations pop an **Allow / Deny** dialog — **Deny is the default** (Return denies) so an accidental keypress can't approve. Safe/allow-listed ops run without asking.
- **`esc`.** Interrupts the current turn; also cancels an in-progress connect.
- **File ▸ New Conversation / Resume Last.** Fresh conversation or resume the most recent one (when idle).
- **`!` bang commands.** `!somecommand` runs a shell command directly on the **proxy host** (project dir), output streamed back — bypasses Claude, no permission dialog (you invoked it). Arbitrary shell exec, by design.
- **View ▸ Dark Mode.** White-on-black, session-only.
- **Scrollback.** Scroll up to read history; the view follows new output only when you're at the bottom.

### Permissions model

The proxy runs the Agent SDK with your host's settings, honoring the allow-rules in `~/.claude` and the project's `.claude/settings*.json`: safe/allow-listed operations auto-run; dangerous/unlisted ones prompt the Allow/Deny dialog on the SE.

---

## Known limits

- Single client only (one SE at a time).
- Text is **Mac Roman**; characters with no Mac Roman equivalent render as `?`.
- The proxy should not stream very large output while a permission ask is outstanding (the SE pauses reading during the modal dialog).
- Clipboard sharing is not yet implemented.

---

## Project layout

```
src/                 SE app (C): event loop, UI, wire codec, transcript, netmac, proto
resources/MacCode.r  Rez resources: window, menus, dialogs, SIZE, app icon (ICN#/BNDL/…)
proxy/               Node/TypeScript proxy (Claude Agent SDK) + vitest tests
tests-native/        Host-side unit tests for the SE's pure-C logic (wire codec, transcript)
tools/               Build helpers (e.g. set_bundle_bit.py)
scripts/             setup / build / emulator-launch scripts
docs/                Toolchain + emulator setup guides
deps/                Retro68 toolchain + emulators (gitignored)
```

See the toolchain and emulator guides in [`docs/`](docs/): [RETRO68_SETUP.md](docs/RETRO68_SETUP.md), [EMULATOR_SETUP.md](docs/EMULATOR_SETUP.md), [WORKFLOW.md](docs/WORKFLOW.md).

---

## Smoke test

connect → prompt → streamed reply (✻ verb, ● tool lines) → Allow a tool, Deny another → `esc` to interrupt → scroll back → File ▸ New / Resume → `!ls` → View ▸ Dark Mode → kill the proxy mid-session and reconnect via Session ▸ Connect → no crashes.

---

## License

MIT — see [LICENSE](LICENSE).
