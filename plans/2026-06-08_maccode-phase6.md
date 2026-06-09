# MacCode Phase 6 — Polish, Features, Hardening & Docs (detailed plan)

> **For agentic workers:** Execute task-by-task. Each SE/GUI task ends with an **emulator checkpoint** Erik runs (the cross-build is authoritative; editor clang errors about `Quickdraw.h`/`Types.h` are host false positives). Proxy tasks are vitest-driven (TDD). **Erik commits — never commit on your own.** Build the SE app with the LOCAL toolchain and the `MacCode_APPL` target: `cmake --build build --target MacCode_APPL` (the bare `MacCode` target leaves the runnable `.bin`/`.dsk` stale). Start the proxy before launching the app.

**Goal:** Take the working Phase-5 MacCode SE app to a shippable v1 — fix the review findings, polish the UX, add bang-command and dark-mode features, harden connect/encoding edges, and write the user-facing setup/run guide.

**Architecture:** Unchanged from Phase 5 — SE GUI app (C/Retro68, classic Toolbox + vendored MacTCP) ⇄ tiny typed-record wire protocol ⇄ Node/TS proxy (Claude Agent SDK). Bang commands are a **proxy-only** change (the SE already sends input verbatim as `RT_PROMPT`; the proxy detects the `!` prefix). Dark mode is **SE-only** (a draw-time color swap + a View menu toggle, session-only — no prefs file).

**Tech:** C / Retro68 (Window/Menu/Control/Dialog/TextEdit/QuickDraw); TypeScript / Node (Agent SDK, `node:child_process`), vitest.

---

## Review findings → tasks map
- #2 Allow-default permission alert → **6.1**
- #1 New/Resume desync → **6.2**
- #6 ASK robustness → **6.3**
- #5 force-scroll-to-bottom → **6.4**
- #3 connect-cancel stall → **6.10**
- #8 unused `gCancel` (repurpose for esc-cancel-connect) → **6.7**
- #4 data stall during modal → documented in **6.13** (no code; proxy must not stream during an outstanding ASK)

## File map (touched in this phase)
```
resources/MacCode.r   # WIND title; Session "..." text; ALRT/DITL 129 default; View MENU(132) + MBAR
src/app.h             # (maybe) shared dark-mode flag accessor — but prefer keeping it in ui.c
src/ui.{c,h}          # smart autoscroll; animate sparkle; dark-mode color swap + toggle; perm item ids
src/main.c            # New/Resume gate; esc-cancels-connect; View menu handling
src/netmac.{c,h}      # connect-cancel proper abort (6.10)
proxy/src/shellrun.ts # NEW: run a shell command, stream stdout/stderr as RelayEvents (bang commands)
proxy/src/server.ts   # detect '!' prefix in prompt → shellrun instead of session.prompt
proxy/test/shellrun.test.ts  # NEW: unit tests for the runner
docs/MACCODE.md       # NEW: setup + run guide
```

---

# Bucket A — Correctness & safety

## Task 6.1 — Permission dialog: Deny is the default (#2)

**Why:** You press Return to *send* a prompt; a reflexive second Return must not silently approve a destructive tool. Make the safe choice (Deny) the default/bold button.

**Files:** Modify `resources/MacCode.r` (DITL 129 item order + button rects), `src/ui.c` (item-id defines).

- [ ] **Step 1 — reorder DITL 129 so Deny is item 1 (default/bold, bottom-right) and Allow is item 2 (to its left).** Replace the current DITL 129 with:
```
resource 'DITL' (129) {
    { {120, 312, 140, 382}, Button { enabled, "Deny" };
      {120, 222, 140, 302}, Button { enabled, "Allow" };
      {10, 70, 110, 382}, StaticText { disabled, "^0" } }
};
```
Leave `ALRT 129`'s stage list as `{ OK; OK; OK; OK }` — `OK` = item 1, which is now **Deny**, so Return/Enter denies (and `CautionAlert` outlines Deny).
- [ ] **Step 2 — update `src/ui.c` item ids:** change the defines to
```c
#define kPermAlertID   129
#define kPermDenyItem  1
#define kPermAllowItem 2
```
`UI_ShowPermission` keeps `return (Boolean)(hit == kPermAllowItem);` — now a mouse click on the (item-2) Allow button is the only thing that returns true.
- [ ] **Step 3 — build** `MacCode_APPL`, clean.
- [ ] **EMULATOR CHECKPOINT (Erik):** prompt "run rm foo.txt" → dialog appears with **Deny** as the bold/default button. Pressing **Return** denies. Clicking **Allow** runs it. Confirm both.
- [ ] **commit:** `fix(se): permission dialog defaults to Deny`

## Task 6.2 — Gate New/Resume to ST_IDLE (#1)

**Why:** Sending RT_NEW/RT_RESUME mid-turn leaves the SE in `ST_AWAITING_RESPONSE` with input disabled and possibly no DONE coming.

**Files:** Modify `src/main.c` (`HandleMenu` File branch).

- [ ] **Step 1 — gate on idle:** in `HandleMenu`, change the File branch to only act when connected **and** idle:
```c
  } else if (id == kFileMenuID){
    if (item == kNewItem)         { if (NetIsConnected() && gApp.state == ST_IDLE) ProtoSendNew(); }
    else if (item == kResumeItem) { if (NetIsConnected() && gApp.state == ST_IDLE) ProtoSendResume(); }
    else if (item == kQuitItem)   gApp.quitting = true;
  }
```
(Defer "interrupt-then-new" to a later phase; for v1, simply ignoring New/Resume while busy is correct and predictable.)
- [ ] **Step 2 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** during a streaming reply, File ▸ New is a no-op (no desync); when idle, New shows `New conversation.` and Resume shows `Resumed most recent conversation.`
- [ ] **commit:** `fix(se): only send New/Resume when idle`

## Task 6.3 — ASK robustness (#6)

**Why:** A malformed (`len < 4`) ASK currently neither shows a dialog nor answers; an ASK arriving outside a response state shouldn't be acted on.

**Files:** Modify `src/proto.c` (`ProtoDispatch` RT_ASK case).

- [ ] **Step 1 — harden the case:**
```c
    case RT_ASK: {
      if (f->len < 4) break;                 /* malformed — ignore (can't read an id to answer) */
      gApp.pendingAskId = ReadU32BE(f->payload);
      gApp.state = ST_AWAITING_PERMISSION;
      {
        Boolean allow = UI_ShowPermission((const char *)(f->payload + 4), (short)(f->len - 4));
        ProtoSendPerm(gApp.pendingAskId, allow);
      }
      gApp.pendingAskId = 0;
      gApp.state = ST_AWAITING_RESPONSE;
      break;
    }
```
(Behavior is unchanged on the happy path; this just makes the malformed case a clean no-op. Acting on ASK regardless of prior state is acceptable since the proxy only asks during a turn; the explicit `len < 4` guard is the safety win.)
- [ ] **Step 2 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** normal Allow/Deny still works (covered by 6.1's checkpoint — can be verified together).
- [ ] **commit:** `fix(se): ignore malformed ASK frames`

---

# Bucket B — UX polish

## Task 6.4 — Smart auto-scroll: preserve scrollback while streaming (#5)

**Why:** `UI_TranscriptChanged` unconditionally jumps to the bottom, so scrolling up to read history during a reply yanks you back.

**Files:** Modify `src/ui.c` (`UI_TranscriptChanged`).

- [ ] **Step 1 — only auto-scroll when already at the bottom.** Capture whether the user was pinned to the bottom *before* recomputing the new max:
```c
void UI_TranscriptChanged(void) {
    short oldMax, newMax;
    Rect cr = ContentRect();
    Boolean atBottom;
    ScrollMax(&oldMax);
    atBottom = (gApp.scrollTop >= oldMax);   /* were we pinned to the latest line? */
    RecomputeMetrics();
    ScrollMax(&newMax);
    if (gVScroll) SetControlMaximum(gVScroll, newMax);
    if (atBottom) UI_ScrollToBottom();        /* follow the tail only if we were already there */
    else if (gApp.scrollTop > newMax) { gApp.scrollTop = newMax; if (gVScroll) SetControlValue(gVScroll, newMax); }
    InvalRect(&cr);
}
```
(`ScrollMax`/`RecomputeMetrics`/`UI_ScrollToBottom` already exist in ui.c.)
- [ ] **Step 2 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** send a prompt with a long reply; while it streams, scroll up — you stay put and can read; scroll back to the bottom and new lines follow again.
- [ ] **commit:** `feat(se): keep scrollback position during streaming`

## Task 6.5 — Window title "MacCode" + Session "Connect…" text

**Why:** WIND title still says "Claude Code"; the Session ▸ "Connect…" item renders the `\xC9` ellipsis escape literally (Retro68 Rez doesn't honor `\xC9` the way C does).

**Files:** Modify `resources/MacCode.r`.

- [ ] **Step 1 — title:** change the `WIND` (128) title from `"Claude Code"` to `"MacCode"`.
- [ ] **Step 2 — ellipsis:** in `resources/MacCode.r` replace the `\xC9` escapes in **resource strings** with literal `...` (three ASCII periods) to sidestep the Rez escape ambiguity — i.e. `"About MacCode..."` and `"Connect..."`. (Leave `\xC9`/`\xD0` in the `.c` files alone — those are C escapes that correctly emit Mac Roman bytes and render fine on the SE.)
- [ ] **Step 3 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** window title reads "MacCode"; Apple ▸ "About MacCode..." and Session ▸ "Connect..." render correctly (no stray "xC9").
- [ ] **commit:** `fix(se): window title + menu ellipsis text`

## Task 6.6 — Animate the sparkle

**Why:** The ✻ verb glyph is static; a subtle animation during a turn signals liveness (Claude Code's spinner vibe).

**Files:** Modify `src/ui.c` (`DrawSparkle`, `UI_DrawInputAndVerb`, `UI_Idle`), `src/main.c` (idle tick already calls `UI_Idle`).

- [ ] **Step 1 — phase state + animated glyph.** Add a static `gSparklePhase` and make `DrawSparkle` draw one of a few rotations of the asterisk (vary which strokes are drawn by phase):
```c
static short gSparklePhase = 0;

/* Draws the ✻ as 4 strokes; the phase rotates which strokes are bright for a twinkle. */
static void DrawSparkle(short cx, short cy) {
    short p = gSparklePhase & 3;
    if (p != 0) { MoveTo(cx,   cy-3); LineTo(cx,   cy+3); }   /* | */
    if (p != 1) { MoveTo(cx-3, cy);   LineTo(cx+3, cy);   }   /* - */
    if (p != 2) { MoveTo(cx-2, cy-2); LineTo(cx+2, cy+2); }   /* \ */
    if (p != 3) { MoveTo(cx-2, cy+2); LineTo(cx+2, cy-2); }   /* / */
}
```
- [ ] **Step 2 — advance the phase on idle while a turn is active**, throttled so it twinkles ~4×/sec, and only redraw the verb strip:
```c
/* in UI_Idle(), after the TEIdle call: */
void UI_Idle(void) {
    if (gTE && gInputEnabled) TEIdle(gTE);
    if (gApp.verb[0] && (gApp.state == ST_AWAITING_RESPONSE || gApp.state == ST_CONNECTING)) {
        static unsigned long nextTick = 0;
        if ((long)(TickCount() - nextTick) >= 0) {
            nextTick = TickCount() + 15;     /* ~4 fps */
            gSparklePhase++;
            { Rect v = VerbRect(); InvalRect(&v); }
        }
    }
}
```
(`TickCount`, `VerbRect`, `gApp` are already available in ui.c.)
- [ ] **Step 3 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** during a reply (and while "Connecting"), the ✻ twinkles; it stops when the turn ends/clears. No flicker elsewhere.
- [ ] **commit:** `feat(se): animate the verb sparkle`

## Task 6.7 — esc cancels an in-progress connect (#8)

**Why:** While `ST_CONNECTING`, esc currently does nothing; let it abort the attempt (gives the user an out, and exercises the otherwise-dead cancel path).

**Files:** Modify `src/main.c` (keyDown esc handling).

- [ ] **Step 1 — extend the esc branch:**
```c
          else if (c == 0x1B){                   /* esc */
            if (gApp.state == ST_AWAITING_RESPONSE){ ProtoSendStop(); UI_SetVerb("Stopping"); }
            else if (gApp.state == ST_CONNECTING){ DoDisconnect(); }   /* abort the connect */
          }
```
(`DoDisconnect` already calls `NetConnectCancel` when `ST_CONNECTING`.)
- [ ] **Step 2 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** with the proxy down in a way that lingers on "Connecting", press esc → returns to disconnected promptly; with the proxy up, a fast connect is unaffected.
- [ ] **commit:** `feat(se): esc cancels an in-progress connect`

---

# Bucket C — Features

## Task 6.8 — Bang (`!`) commands: direct shell on the proxy host

**Why:** Mirror Claude Code's `!` bash mode — `!ls` runs the raw command on the proxy host and streams output back, bypassing Claude. **Proxy-only**: the SE already sends the typed line verbatim as `RT_PROMPT`, so the proxy detects the `!` prefix. User-invoked ⇒ no permission prompt (matches Claude Code).

**Files:** Create `proxy/src/shellrun.ts`; Create `proxy/test/shellrun.test.ts`; Modify `proxy/src/server.ts`.

- [ ] **Step 1 — failing test** (`proxy/test/shellrun.test.ts`):
```ts
import { describe, it, expect } from "vitest";
import { runShell } from "../src/shellrun";
import type { RelayEvent } from "../src/events";

function collect(cmd: string, cwd: string): Promise<RelayEvent[]> {
  return new Promise((resolve) => {
    const evs: RelayEvent[] = [];
    runShell(cmd, cwd, (ev) => { evs.push(ev); if (ev.kind === "done") resolve(evs); });
  });
}

describe("runShell", () => {
  it("streams stdout then a done event", async () => {
    const evs = await collect("echo hello", process.cwd());
    const text = evs.filter(e => e.kind === "text").map(e => (e as any).text).join("");
    expect(text).toContain("hello");
    expect(evs[evs.length - 1].kind).toBe("done");
  });
  it("reports a non-zero exit as an error event then done", async () => {
    const evs = await collect("exit 3", process.cwd());
    expect(evs.some(e => e.kind === "error")).toBe(true);
    expect(evs[evs.length - 1].kind).toBe("done");
  });
});
```
- [ ] **Step 2 — run, confirm FAIL** (`runShell` not exported): `npm --prefix proxy test -- shellrun`
- [ ] **Step 3 — implement `proxy/src/shellrun.ts`:**
```ts
// ABOUTME: Runs a user-invoked "!" shell command on the proxy host and streams its
// ABOUTME: stdout/stderr back as RelayEvents. Bypasses Claude; the SE user invoked it.
import { spawn } from "node:child_process";
import type { RelayEvent } from "./events";
import { log } from "./log";

export function runShell(command: string, cwd: string, onEvent: (ev: RelayEvent) => void): void {
  log(`shell: ${JSON.stringify(command)}`);
  onEvent({ kind: "info", text: `$ ${command}` });
  const child = spawn("/bin/sh", ["-c", command], { cwd });
  child.stdout.on("data", (b: Buffer) => onEvent({ kind: "text", text: b.toString("utf8") }));
  child.stderr.on("data", (b: Buffer) => onEvent({ kind: "text", text: b.toString("utf8") }));
  child.on("error", (e) => { onEvent({ kind: "error", text: e.message }); onEvent({ kind: "done" }); });
  child.on("close", (code) => {
    if (code && code !== 0) onEvent({ kind: "error", text: `exit ${code}` });
    onEvent({ kind: "done" });
  });
}
```
- [ ] **Step 4 — wire into `proxy/src/server.ts`** `prompt` case: detect the `!` prefix and route to `runShell` instead of the session, reusing the existing `onEvent` → frame path and verb:
```ts
          case "prompt": {
            const text = a.text;
            if (text.startsWith("!")) {
              send(verbFrame("Running"));
              runShell(text.slice(1).trim(), deps.project ?? process.cwd(), onEvent);
              break;
            }
            send(verbFrame(verbForTurn(turn++)));
            session.prompt(text);
            break;
          }
```
Add `import { runShell } from "./shellrun";` and add a `project: string` field to `ServerDeps` (passed from `index.ts` as the resolved project path) so the command runs in the project dir. (`onEvent` already chunks text via `framesForEvent` and emits the `clearVerbFrame()` on `done`.)
- [ ] **Step 5 — update `proxy/src/index.ts`** to pass `project` into `createServer({ ..., project })`, and adjust any `ServerDeps` typing + existing tests that construct the server (add the `project` field).
- [ ] **Step 6 — full proxy suite green + `tsc --noEmit` clean + pristine test output:** `npm --prefix proxy test` and `npm --prefix proxy run build`.
- [ ] **Step 7 — EMULATOR CHECKPOINT (Erik):** with the proxy up, type `!ls` → `$ ls` line then the directory listing streams in; `!pwd` shows the project path; a failing command (`!false`) shows `exit 1`. Claude is **not** invoked (proxy log shows `shell:` not `session.start`).
- [ ] **Step 8 — commit:** `feat(proxy): bang (!) commands run a shell on the host`

## Task 6.9 — Dark mode (View menu toggle, session-only)

**Why:** White-on-black option. 1-bit SE: swap QuickDraw fore/back so the background paints black and text/lines paint white. Session-only (no prefs file).

**Files:** Modify `resources/MacCode.r` (new `MENU` 132 "View" + add to `MBAR`); Modify `src/ui.{c,h}` (color swap + toggle); Modify `src/main.c` (View menu handling).

- [ ] **Step 1 — resources:** add a View menu and register it in the MBAR. Append after the Session menu:
```
resource 'MENU' (132, "View") {
    132, textMenuProc, allEnabled, enabled, "View",
    { "Dark Mode", noIcon, noKey, noMark, plain }
};
```
and change `resource 'MBAR' (128) { { 128, 129, 130, 131 } };` → `{ { 128, 129, 130, 131, 132 } };`.
- [ ] **Step 2 — ui.h:** add `void UI_ToggleDark(void);` and `Boolean UI_IsDark(void);`.
- [ ] **Step 3 — ui.c:** add `static Boolean gDark = false;` and a helper that sets the draw colors, called at the top of each drawing routine (`UI_DrawTranscript`, `UI_DrawInputAndVerb`, and once in `UI_Update`):
```c
static void ApplyColors(void) {
    if (gDark) { BackColor(blackColor); ForeColor(whiteColor); }
    else       { BackColor(whiteColor); ForeColor(blackColor); }
}
```
Call `ApplyColors()` before the `EraseRect`/draw calls in `UI_DrawTranscript` and `UI_DrawInputAndVerb` (EraseRect paints with the back color → black in dark mode; DrawText/lines use the fore color → white). In `UI_Update`, call `ApplyColors()` first, then `EraseRect(&gApp.win->portRect)` so the whole content (including the scrollbar gutter) repaints. Add:
```c
void UI_ToggleDark(void) {
    gDark = !gDark;
    InvalRect(&gApp.win->portRect);   /* full redraw */
}
Boolean UI_IsDark(void) { return gDark; }
```
Note: the scrollbar is a system control drawn by `DrawControls`; on 1-bit it will draw in the current fore/back, which is acceptable for v1 (verify in the emulator — if it looks wrong, leaving the gutter its default is fine).
- [ ] **Step 4 — main.c:** add `#define kViewMenuID 132` and `#define kDarkItem 1`; handle it in `HandleMenu`:
```c
  } else if (id == kViewMenuID){
    if (item == kDarkItem){ UI_ToggleDark(); CheckItem(GetMenuHandle(kViewMenuID), kDarkItem, UI_IsDark()); }
  }
```
- [ ] **Step 5 — build** `MacCode_APPL`, clean.
- [ ] **Step 6 — EMULATOR CHECKPOINT (Erik):** View ▸ Dark Mode flips to white-on-black (transcript, verb, input box, sparkle all legible); checkmark tracks state; toggling back restores light; relaunch returns to light. Tune any unreadable element.
- [ ] **Step 7 — commit:** `feat(se): dark mode (View menu, session-only)`

---

# Bucket D — Hardening & docs

## Task 6.10 — Connect-cancel: real abort, not just PBKillIO (#3)

**Why:** `NetConnectCancel` spins on `gOpenPB->ioResult` after `LowKillTCP` (PBKillIO). If MacTCP ignores `PBKillIO` for a pending `TCPActiveOpen`, the spin blocks (SystemTask only) until the open's own ~30s timeout — a UI freeze on cancel/quit-during-connect. It's memory-safe today (we never free until the PB actually completes) but can stall.

**Files:** Modify `src/netmac.c` (`NetConnectCancel`); possibly use `LowTCPAbort`/`AbortConnection` (vendored, already declared) on the stream to force the open to complete.

- [ ] **Step 1 — abort via the stream so the open completes promptly.** Issuing `TCPAbort`/`TCPRelease` on the stream forces a pending `TCPActiveOpen` PB to complete with an error quickly. Rework `NetConnectCancel`:
```c
void NetConnectCancel(NetGiveTime giveTime) {
    if (!gConnecting) return;
    /* Force the pending active-open to complete: kill the call, and abort the stream
       so MacTCP finishes the PB promptly even if PBKillIO alone is ignored for ActiveOpen. */
    if (gOpenPB) {
        LowKillTCP(gOpenPB);
        if (gStream) AbortConnection((StreamPtr)gStream, (GiveTimePtr)giveTime, &gCancel);
        while (gOpenPB->ioResult > 0) (*giveTime)();   /* now completes quickly */
        DisposePtr((Ptr)gOpenPB);
        gOpenPB = NULL;
    }
    if (gStream) { ReleaseStream((StreamPtr)gStream, (GiveTimePtr)giveTime, &gCancel); gStream = 0; }
    gConnecting = false;
}
```
(`AbortConnection` is declared in `TCPHi.h` / `TCPRoutines.h`. The spin still only frees after real completion, so it stays memory-safe; the abort just makes completion fast. **Verify in the emulator** — if `AbortConnection` on a not-yet-open stream errors, it's harmless, but confirm the cancel returns quickly.)
- [ ] **Step 2 — build** clean.
- [ ] **EMULATOR CHECKPOINT:** force a lingering "Connecting" (proxy down on a setup that doesn't RST, or kill the proxy at the right moment), then esc / Session ▸ Disconnect / Quit — each returns control within ~1s, no long freeze. (If your slirp always RSTs, this is hard to trigger; note that and rely on code review.)
- [ ] **commit:** `fix(se): abort pending connect promptly on cancel`

## Task 6.11 — Encoding & resilience edge-case hardening

**Why:** Tighten the seams that only show under stress.

**Files:** Review/modify `proxy/src/translate.ts` (chunk boundary), `proxy/src/macroman.ts` (unmapped chars), `src/proto.c`/`src/wire.c` (already solid — verify only).

- [ ] **Step 1 — unmapped Mac Roman chars:** confirm `toMacRoman` maps characters with no Mac Roman equivalent to a visible placeholder (e.g. `?`) rather than dropping or emitting `0x00` (a NUL would truncate SE-side strings). Add a test in `proxy/test/macroman.test.ts` for an unmapped char (e.g. an emoji) → expect a non-NUL placeholder byte. Fix `toMacRoman` if needed.
- [ ] **Step 2 — chunk-boundary newline safety:** verify `textFrames`/`chunkText` never splits a multi-byte sequence (Mac Roman is single-byte, so this is just confirming the 4093 cap is byte-based, which it is) and that a chunk boundary mid-word only causes a wrap, never a dropped char. Add/confirm a test that re-concatenated chunks equal the original Mac Roman bytes (already covered by the 5.1 test — extend if gaps).
- [ ] **Step 3 — proxy resilience:** confirm the server cleanly handles the SE disconnecting mid-turn (socket close while `session` is streaming) — the existing `cleanup()` calls `session.stop()`; add a test or a log-verified manual check. For bang commands, ensure a `runShell` child is killed if the client disconnects mid-run (track the active child and `child.kill()` in `cleanup`).
- [ ] **Step 4 — full proxy suite + tsc clean; `tests-native` (`cd tests-native && make check`) still green.**
- [ ] **commit:** `harden(proxy): encoding fallbacks + mid-turn disconnect cleanup`

## Task 6.12 — End-to-end test

**Why:** A single automated check that the proxy path works against the real Agent SDK, plus a documented manual SE e2e.

**Files:** Modify `proxy/test/e2e.test.ts` (the existing opt-in e2e) to also cover a bang command; add a manual e2e checklist to `docs/MACCODE.md` (written in 6.13).

- [ ] **Step 1 — extend the opt-in e2e** (gated behind the existing env flag so CI/normal runs skip it) to: connect a socket, send HELLO, send a `!echo hi` prompt, assert the streamed frames contain `hi` and a DONE. Keep it `it.skip`/env-gated so the default suite stays hermetic and pristine.
- [ ] **Step 2 — run the opt-in e2e once locally** to confirm green (Erik runs it with the flag set, since it needs Agent SDK auth + a real socket).
- [ ] **commit:** `test(proxy): e2e covers bang commands (opt-in)`

## Task 6.13 — `docs/MACCODE.md` user guide

**Why:** Someone (including future-Erik) needs to go from clone → talking to Claude on the SE without spelunking.

**Files:** Create `docs/MACCODE.md`.

- [ ] **Step 1 — write the guide** covering, concretely:
  - **What it is** (one paragraph + the 3-layer architecture diagram in words).
  - **Prerequisites:** the local Retro68 toolchain in `deps/` (`make setup`), Node for the proxy, Basilisk II with MacTCP/slirp (already configured in `deps/`).
  - **Build the SE app:** `cmake --build build --target MacCode_APPL` (note: the `_APPL` target is required for a runnable `.bin`/`.dsk`).
  - **Run the proxy:** `npm --prefix proxy start -- --project <dir> --port 4242` (flags: `--project`, `--port`, `--host`, `--model`, `--echo`). **Start the proxy before launching the app.**
  - **Launch + connect:** `scripts/run-basiliskii.sh MacCode`; it auto-connects to `10.0.2.2:4242`; Session ▸ Connect/Disconnect to retry.
  - **Usage:** typing prompts; the ✻ verb + ● tool lines; **tool permissions** (Allow/Deny, Deny is the default); **esc** to interrupt; **File ▸ New/Resume**; **`!cmd`** bang commands (direct shell, no permission prompt — note the trust implication); **View ▸ Dark Mode**; scrollback.
  - **Permissions model:** the proxy inherits the host's `.claude` allow-rules; safe ops auto-run, dangerous/unlisted ops prompt on the SE (by design — link the decision).
  - **Known limits / notes:** the proxy must not stream large output while an ASK is outstanding (#4); single client only; Mac Roman text; clipboard sharing is not yet implemented.
  - **Manual e2e checklist:** the Phase-5/6 QA flow (connect → prompt → permissions → esc → New/Resume → bang → dark mode → disconnect/reconnect).
- [ ] **commit:** `docs: add docs/MACCODE.md setup + usage guide`

---

## Phase 6 exit criteria
Review findings #1/#2/#6 fixed; scrollback survives streaming; title/menu text correct; sparkle animates; esc cancels connect; `!` commands run a host shell and stream back; dark mode toggles cleanly; connect-cancel returns promptly; encoding/disconnect edges hardened; opt-in e2e green; `docs/MACCODE.md` lets a fresh user get to a working session. Deferred to a later phase: **clipboard sharing** (@schrockwell).
```
