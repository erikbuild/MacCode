# MacCode Phase 5 — SE Application (detailed plan)

> Expands the roadmap-level "Phase 5" in `plans/2026-06-06_maccode.md` into bite-sized tasks. Builds on the **proven** Phase 3 MacTCP stack (`src/MacTCP.h`, `src/TCPRoutines.{c,h}`, `src/TCPHi.{c,h}`) and Phase 4 pure-logic (`src/wire.{c,h}`, `src/transcript.{c,h}`).

**Goal:** The real MacCode GUI app — a System 7 window that connects to the proxy, streams Claude's responses into a scrolling transcript with the ✻ verb line and ● tool lines, sends prompts from a bottom input box, surfaces Allow/Deny tool permissions, and supports interrupt + new/resume.

**Architecture:** GUI Toolbox app (no console). Event-loop driven; the idle branch polls a **non-blocking** netmac receive, feeds `WireDecoder`, and dispatches frames into the transcript/verb model, then redraws. Outbound frames are built with `WireEncode` + `NetSend`.

**Tech:** C / Retro68; classic Toolbox (Window/Dialog/Control/TextEdit/QuickDraw); vendored MacTCP `TCPHi`/`TCPRoutines`.

---

## Fidelity note (read first)

Tasks 5.1–5.2, 5.6 protocol glue, app.h, resources, and the frame-dispatch switch have **complete code** (I'm confident in those APIs). The drawing/TextEdit/scrollbar tasks (5.4–5.5, 5.7) give **complete interfaces + concrete Toolbox call sequences + verification**, but the pixel-level drawing is expected to be **tuned in the emulator** — each ends with a "you verify in Basilisk II" checkpoint, since I can't see the screen. Every task: build clean, then (for GUI tasks) Erik runs `scripts/run-basiliskii.sh MacCode` and reports.

**Standing constraints (from prior reviews):**
- `Transcript` (~52 KB) is allocated once via `NewPtr` and kept in a global — **never on the stack**.
- The event loop feeds `WireDecoderPush` in **≤ (WIRE_BUF_SIZE − used)** slices, **drains** (`NULL,0`) until it returns 0, and treats **−1 as fatal** (drop + `WireDecoderInit`). Copy `fr.payload` immediately.
- Wire text is `\n`-delimited; convert the user's Return (`\r`) → `\n` before sending a PROMPT.
- GUI only (RetroConsole crashes here — see memory).

## File map

```
src/app.h        # AppState enum, AppGlobals struct, shared decls
src/netmac.h/.c  # persistent TCP over TCPHi: connect / send / non-blocking poll / close
src/proto.h/.c   # SE protocol glue: build+send HELLO/PROMPT/PERM/STOP/NEW/RESUME; dispatch one decoded frame
src/ui.h/.c      # window, transcript drawing, scrollbar, input (TextEdit), verb line, permission dialog
src/main.c       # toolbox init, Transcript alloc, event loop + state machine
resources/MacCode.r  # WIND, MBAR/MENU (Apple/File/Edit/Session), DLOG/DITL (perm + connect), ALRT, SIZE
CMakeLists.txt   # MacCode target (+ vendored TCP files w/ -Wno flags, C11); drop MyApp + spike at the end
```

---

# Task 5.1 — Proxy: chunk text frames ≤ WIRE_BUF_SIZE−3 (TS, unit-tested)

Resolves tracked task #22. Autonomous (no emulator). A single large assistant block must not produce a frame the SE decoder rejects (−1).

**Files:** Modify `proxy/src/translate.ts`; Modify `proxy/test/translate.test.ts`.

- [ ] **Step 1 — failing test** (append to `translate.test.ts`):
```ts
import { textFrames } from "../src/translate";
describe("textFrames chunking", () => {
  it("splits text longer than the SE buffer into multiple TEXT frames ≤ 4093 bytes", () => {
    const big = "a".repeat(10000);
    const bufs = textFrames(big);
    const d = new FrameDecoder();
    const frames = bufs.flatMap(b => d.push(b));
    expect(frames.length).toBeGreaterThan(1);
    for (const f of frames) { expect(f.type).toBe(RT.TEXT); expect(f.payload.length).toBeLessThanOrEqual(4093); }
    expect(Buffer.concat(frames.map(f => f.payload)).toString()).toBe(big);
  });
  it("emits a single frame for short text", () => {
    const d = new FrameDecoder();
    const frames = (textFrames("hi")).flatMap(b => d.push(b));
    expect(frames).toHaveLength(1);
    expect(frames[0].payload.toString()).toBe("hi");
  });
});
```
- [ ] **Step 2 — run, confirm FAIL** (`textFrames` not exported): `cd proxy && npx vitest run test/translate.test.ts`
- [ ] **Step 3 — implement** in `translate.ts`: add a `SE_MAX_PAYLOAD = 4093` const and a `textFrames` helper that Mac-Roman-encodes then splits into ≤ SE_MAX_PAYLOAD chunks, each a `RT.TEXT` frame:
```ts
export const SE_MAX_PAYLOAD = 4093; // WIRE_BUF_SIZE(4096) - 3-byte header

export function textFrames(text: string): Buffer[] {
  const mac = toMacRoman(text);
  if (mac.length === 0) return [encodeFrame(RT.TEXT, mac)];
  const out: Buffer[] = [];
  for (let i = 0; i < mac.length; i += SE_MAX_PAYLOAD) {
    out.push(encodeFrame(RT.TEXT, mac.subarray(i, i + SE_MAX_PAYLOAD)));
  }
  return out;
}
```
Then change `eventToFrames` so the `text` case returns possibly-multiple frames — simplest: have `eventToFrames` return `Buffer` for non-text and the server special-cases text via `textFrames`. **Cleaner:** make `eventToFrames` return `Buffer[]` for all kinds (wrap single frames in an array), and update `server.ts` `onEvent` to `for (const fb of eventToFrames(ev)) send(fb)`. Apply the same ≤4093 chunking to `info`/`error`/`tool` payloads (reuse a private `chunkedFrames(type, macBytes)`).
- [ ] **Step 4 — update `server.ts`** `onEvent` to iterate frames; update existing translate tests that asserted a single `Buffer` to read `eventToFrames(...)[0]` or flatten. Run full proxy suite green; `tsc --noEmit` clean.
- [ ] **Step 5 — commit:** `feat(proxy): chunk SE-bound text frames to <=4093 bytes`

---

# Task 5.2 — netmac: persistent TCP over TCPHi (non-blocking poll) + validate via spike

**Files:** Create `src/netmac.h`, `src/netmac.c`; Modify `src/spike_main.c` (use netmac); Modify `CMakeLists.txt` (spike links netmac).

- [ ] **Step 1 — `src/netmac.h`:**
```c
/* ABOUTME: Persistent TCP connection for MacCode over the vendored TCPHi/MacTCP stack.
   ABOUTME: Non-blocking NetPoll for event-loop integration; dotted-quad addressing (no DNR). */
#ifndef NETMAC_H
#define NETMAC_H
#include <MacTypes.h>

typedef void (*NetGiveTime)(void);

OSErr   NetInit(void);                                   /* open MacTCP driver (once) */
OSErr   NetConnect(const char *dottedQuad, unsigned short port, NetGiveTime giveTime);
Boolean NetIsConnected(void);
OSErr   NetSend(const void *data, unsigned short len, NetGiveTime giveTime); /* push send */
/* Non-blocking: returns bytes read into buf (0 if none available), or -1 on close/error. */
long    NetPoll(void *buf, unsigned short maxLen, NetGiveTime giveTime);
void    NetClose(NetGiveTime giveTime);

#endif
```
- [ ] **Step 2 — `src/netmac.c`:** wrap TCPHi. Keep a `static StreamPtr gStream; static Boolean gConnected; static bool gCancel;`.
  - `NetInit` → `InitNetwork()`.
  - `NetConnect` → `Net_ParseIP`-style dotted-quad parse (inline the parser); `CreateStream(&gStream, 8192, giveTime, &gCancel)`; `OpenConnection(gStream, (long)ip, port, 30, giveTime, &gCancel)`; set `gConnected` on noErr.
  - `NetSend` → `SendData(gStream, (Ptr)data, len, true, giveTime, &gCancel)`.
  - `NetPoll` → `LowTCPStatus(gStream, &statusPB, giveTime, &gCancel)` (from `TCPRoutines.h`); if `statusPB.amtUnreadData == 0` return 0; else `unsigned short n = min(amtUnreadData, maxLen); RecvData(gStream, buf, &n, false, giveTime, &gCancel)`; on `connectionClosing`/`connectionTerminated` set `gConnected=false` and return -1; else return n. **Confirm `TCPStatusPB.amtUnreadData` against `src/MacTCP.h`.**
  - `NetClose` → if gConnected `CloseNetConnection` then `ReleaseStream`; clear gConnected.
- [ ] **Step 3 — repoint the spike at netmac:** rewrite `src/spike_main.c`'s body to use `NetInit/NetConnect("10.0.2.2",4242,GiveTime)/NetSend(hello,5,GiveTime)`, then a **poll loop** (`for ~40 iterations: n=NetPoll(buf+total, ...); if n<0 break; total+=n; GiveTime()`) until total≥5, then the same ParamText+NoteAlert result. Add `src/netmac.c` to the `maccode_spike` target in CMakeLists (with the existing C11 + -Wno flags also applied to netmac.c if it includes the vendored headers).
- [ ] **Step 4 — build:** `cmake --build build` → clean; `maccode_spike.bin` produced.
- [ ] **Step 5 — EMULATOR CHECKPOINT (Erik):** echo proxy running; `scripts/run-basiliskii.sh maccode_spike`; open it → alert shows `ROUND-TRIP OK` via the **non-blocking** path. This validates `NetPoll` before any UI is built.
- [ ] **Step 6 — commit:** `feat(se): netmac persistent TCP wrapper (non-blocking poll), validated via spike`

---

# Task 5.3 — App skeleton: state, globals, window, menus, event loop

**Files:** Create `src/app.h`; Create `src/main.c` (replaces the stub's role); Create `resources/MacCode.r`; Modify `CMakeLists.txt` (add `MacCode` target).

- [ ] **Step 1 — `src/app.h`:**
```c
/* ABOUTME: MacCode shared app state — the AppState machine and global model. */
#ifndef APP_H
#define APP_H
#include <MacTypes.h>
#include "transcript.h"
#include "wire.h"

typedef enum {
  ST_DISCONNECTED, ST_CONNECTING, ST_IDLE,
  ST_AWAITING_RESPONSE, ST_AWAITING_PERMISSION, ST_ERROR
} AppState;

typedef struct {
  AppState     state;
  Transcript  *transcript;       /* NewPtr'd, never on the stack */
  WireDecoder  dec;
  WindowPtr    win;
  Boolean      quitting;
  char         verb[64];         /* current ✻ verb text ("" = none) */
  unsigned long pendingAskId;    /* ASK id awaiting PERM (0 = none) */
  short        scrollTop;        /* first visible transcript line index */
  /* TE handles, control handles added in 5.4/5.5 */
} AppGlobals;

extern AppGlobals gApp;
#define kProtocolVersion 1
#endif
```
- [ ] **Step 2 — `resources/MacCode.r`:** `WIND` (full screen minus menu bar, document proc, goAway, title "Claude Code"), `MBAR` 128 → `MENU` Apple(128, "About MacCode…"), File(129: New Conversation/Resume Last/—/Quit-Q), Edit(130: standard Undo/Cut/Copy/Paste/Clear for TE), Session(131: Connect…/Disconnect). `SIZE` (1 MB pref / 512 KB min, is32BitCompatible, acceptSuspendResume). (DLOG/DITL/ALRT added in 5.6/5.7.)
- [ ] **Step 3 — `src/main.c`:** standard init (`InitGraf…InitDialogs/InitCursor/FlushEvents`); `gApp.transcript = (Transcript*)NewPtr(sizeof(Transcript)); TrInit(gApp.transcript);` (check NewPtr != NULL, else StopAlert + quit); `WireDecoderInit(&gApp.dec)`; `GetNewMBar`/`SetMenuBar`/`AppendResMenu(...,'DRVR')`/`DrawMenuBar`; `GetNewWindow(kWindowID,…)`; `gApp.state = ST_DISCONNECTED`. Event loop: `WaitNextEvent(everyEvent,&ev,kSleep,NULL)` switching mouseDown(`FindWindow`→inMenuBar/inDrag/inGoAway/inContent), keyDown (cmd-key → MenuKey), updateEvt (BeginUpdate/EndUpdate stub), with an **idle** hook (the poll, added in 5.6). Implement `HandleMenu` (About→note alert; Quit→quitting; others stubbed). `kSleep = 10` ticks for now (tightened in 5.6).
- [ ] **Step 4 — CMakeLists:** add
```cmake
add_application(MacCode
    CREATOR "MCde"
    resources/MacCode.r
    src/main.c
    src/netmac.c
    src/proto.c
    src/ui.c
    src/wire.c
    src/transcript.c
    src/TCPRoutines.c
    src/TCPHi.c
)
set_target_properties(MacCode PROPERTIES C_STANDARD 11)
set_source_files_properties(src/TCPRoutines.c src/TCPHi.c PROPERTIES
    COMPILE_FLAGS "-Wno-incompatible-pointer-types -Wno-int-conversion")
```
(`proto.c`/`ui.c` are created in later tasks; until then, stub them as empty files with their ABOUTME header so the target links, OR add them to the target as each is created — simplest: create empty `src/proto.c`/`src/ui.c` now with just headers + a `/* filled in Task 5.x */`.)
- [ ] **Step 5 — build** → clean; `MacCode.bin` produced.
- [ ] **Step 6 — EMULATOR CHECKPOINT (Erik):** `scripts/run-basiliskii.sh MacCode` → a "Claude Code" window + menu bar appear; About shows; Quit works; no crash.
- [ ] **Step 7 — commit:** `feat(se): MacCode app skeleton — window, menus, state, event loop`

---

# Task 5.4 — Transcript view: draw lines + scrollbar

**Files:** Create `src/ui.h`, fill `src/ui.c`; Modify `src/main.c` (call UI draw/scroll).

Interface:
```c
void UI_Init(void);                 /* create the VScroll control, set fonts */
void UI_DrawTranscript(void);       /* draw visible TrLines in the content area */
void UI_TranscriptResized(void);    /* recompute wrapCols + scrollMax after append/resize */
int  UI_WrapCols(void);             /* content width / char width */
void UI_HandleContentClick(Point p);/* scrollbar hit-testing / TrackControl */
void UI_ScrollToBottom(void);
```
Approach (concrete Toolbox): a `VScroll` `ControlHandle` on the window's right edge; content rect = window minus scrollbar minus input/verb strip (reserve bottom ~40 px for 5.5). On update: `TextFont(geneva); TextSize(9);` compute `lineH = ascent+descent+leading` via `GetFontInfo`; for visible rows `i = scrollTop .. scrollTop+visibleRows`, `MoveTo(left, top + (i-scrollTop)*lineH + ascent)` and draw `TrGet(i)` — bold face for `TR_USER`, plain otherwise, the `●`/text already in the line. Scrollbar max = `TrLiveCount - visibleRows`; `SetControlValue` = scrollTop. `UI_HandleContentClick` → `FindControl`+`TrackControl` (with an action proc for page/line) updating `scrollTop` and invalidating. `UI_ScrollToBottom` sets scrollTop to max (called after each append unless the user has scrolled up).
- [ ] Steps: write the interface; implement draw; implement scrollbar; wire `updateEvt`→`UI_DrawTranscript`+`UpdateControls`, `inContent`→`UI_HandleContentClick`; in `main` after init, **seed** a few `TrAppend` lines (temporary) to verify rendering.
- [ ] **EMULATOR CHECKPOINT:** seeded lines render in Geneva 9, long lines wrapped (proves `UI_WrapCols` feeds `TrAppend`), scrollbar scrolls. Remove the seed lines after.
- [ ] **commit:** `feat(se): transcript view with scrollbar`

---

# Task 5.5 — Input box (TextEdit) + verb line

**Files:** Modify `src/ui.{c,h}`, `src/main.c`.

Interface: `void UI_InitInput(void)`, `void UI_DrawInputAndVerb(void)`, `Boolean UI_InputKey(EventRecord*)` (returns true if Return pressed → caller sends), `void UI_GetInput(char *buf, int max)`, `void UI_ClearInput(void)`, `void UI_SetVerb(const char *verb)`, `void UI_SetInputEnabled(Boolean)`.
Approach: a single-line-ish `TEHandle` in the bottom strip (`TENew` in an input rect; `TEActivate`; route `keyDown`→`TEKey` unless it's Return). Return (`\r`) with no modifiers → signal send (don't insert). Verb strip just above input: draw a small ✻ glyph (hand-drawn star with `MoveTo`/`Line`, or a tiny `PICT`) + `gApp.verb` text + "(esc to stop)" when `state==ST_AWAITING_RESPONSE`. `UI_SetInputEnabled(false)` while awaiting response/permission (skip `TEKey`, grey the strip).
- [ ] Steps: TENew input; idle `TEIdle` for caret; `keyDown` routing; verb drawing; enable/disable.
- [ ] **EMULATOR CHECKPOINT:** type into the box; caret blinks; Return is detected (log via a temporary `TrAppend` of the typed text); verb line shows/clears when `UI_SetVerb` called from a temporary test.
- [ ] **commit:** `feat(se): input box + verb line`

---

# Task 5.6 — Wire it up: connect, HELLO, poll→dispatch, send PROMPT

**Files:** Create `src/proto.h`, fill `src/proto.c`; Modify `src/main.c`.

`src/proto.h`:
```c
/* ABOUTME: SE protocol glue — build/send frames and dispatch one decoded inbound frame. */
#ifndef PROTO_H
#define PROTO_H
#include "wire.h"
void ProtoSendHello(void);
void ProtoSendPrompt(const char *utf8ish);   /* converts \r -> \n; encodes; NetSend */
void ProtoSendPerm(unsigned long id, Boolean allow);
void ProtoSendStop(void);
void ProtoSendNew(void);
void ProtoSendResume(void);
void ProtoDispatch(const WireFrame *f);       /* update gApp/UI from one decoded frame */
#endif
```
- [ ] **Step 1 — `proto.c` senders:** each builds a frame with `WireEncode` into a static buffer and `NetSend`s it (with the app's `GiveTime`). `ProtoSendHello`: payload = 2-byte BE `kProtocolVersion`. `ProtoSendPrompt`: copy input replacing `'\r'`→`'\n'`, then `RT_PROMPT`. `ProtoSendPerm`: 4-byte BE id + 1 allow byte, `RT_PERM`. STOP/NEW/RESUME: empty payload.
- [ ] **Step 2 — `proto.c` `ProtoDispatch`:**
```c
void ProtoDispatch(const WireFrame *f){
  switch (f->type){
    case RT_TEXT:  TrAppendN(f->payload, f->len, TR_ASSISTANT); break;
    case RT_TOOL:  TrAppendN(f->payload, f->len, TR_TOOL); break;
    case RT_INFO:  TrAppendN(f->payload, f->len, TR_INFO); break;
    case RT_ERR:   TrAppendN(f->payload, f->len, TR_ERR);  gApp.state = ST_IDLE; break;
    case RT_VERB:  UI_SetVerbN(f->payload, f->len); break;     /* empty => clear */
    case RT_DONE:  UI_SetVerb(""); gApp.state = ST_IDLE; UI_SetInputEnabled(true); break;
    case RT_ASK:   { gApp.pendingAskId = ReadU32BE(f->payload);
                     gApp.state = ST_AWAITING_PERMISSION;
                     UI_ShowPermission(f->payload+4, f->len-4); } break;  /* dialog in 5.7 */
  }
  UI_ScrollToBottom(); InvalContent();
}
```
(Add a `TrAppendN(bytes,len,kind)` helper to transcript.c that NUL-terminates into a temp + calls `TrAppend` with the app's wrapCols — or extend `TrAppend` to take a length. Note: payload is Mac Roman already; `wrapCols = UI_WrapCols()`.)
- [ ] **Step 3 — main loop integration:** `GiveTime` = a function that calls `WaitNextEvent` with 0 sleep + handles update/null only (no re-entrancy into send). On launch (or Session→Connect): `NetInit`; `NetConnect("10.0.2.2",4242,GiveTime)`; on success `ProtoSendHello()`, `state=ST_IDLE`; else `state=ST_ERROR` + alert. **Idle hook each loop:** while connected, `room = WIRE_BUF_SIZE - gApp.dec.used; n = NetPoll(tmp, min(room,sizeof tmp), GiveTime);` if `n<0` → disconnect (`state=ST_DISCONNECTED`, NetClose); if `n>0` → `int r; WireDecoderPush(&dec, tmp, n, &fr)` then **drain**: `while ((r=WireDecoderPush(&dec,NULL,0,&fr))==1) ProtoDispatch(&fr); if (r<0) { /* fatal: drop+reinit */ }`. (Careful: push the fed bytes once, then drain; handle the first push's return too.) On Return in the input (`UI_InputKey`): if `state==ST_IDLE`, read input, `TrAppend` it as `TR_USER`, `ProtoSendPrompt`, `UI_ClearInput`, `state=ST_AWAITING_RESPONSE`, `UI_SetInputEnabled(false)`. Tighten `kSleep` to ~3–6 ticks for responsiveness while connected.
- [ ] **Step 4 — build** clean.
- [ ] **Step 5 — EMULATOR CHECKPOINT (the big one):** run the **real** proxy `node --import tsx proxy/src/index.ts --project <somedir> --port 4242`; `scripts/run-basiliskii.sh MacCode`; connect; type "hello" → Claude's reply streams into the transcript, ✻ verb shows during the turn and clears on DONE, ● tool lines appear. Report what renders.
- [ ] **Step 6 — commit:** `feat(se): connect, HELLO, poll/dispatch loop, send prompts`

---

# Task 5.7 — Permission dialog (ASK / PERM)

**Files:** Modify `resources/MacCode.r` (DLOG/DITL 200: text + Allow + Deny), `src/ui.{c,h}` (`UI_ShowPermission`), `src/main.c`.

Approach: a modal dialog (or `CautionAlert`) showing the ASK description (Mac Roman text, set into a StaticText via `ParamText` or a userItem). Two buttons: Allow (1), Deny (2). `UI_ShowPermission(descPtr,len)` builds a Str255, `ParamText`, `Alert`/`ModalDialog`; returns the choice; caller `ProtoSendPerm(gApp.pendingAskId, allow)`, clears `pendingAskId`, `state=ST_AWAITING_RESPONSE`. Keep it modal for v1 (simplest correct).
- [ ] **EMULATOR CHECKPOINT:** prompt Claude to do something needing approval (e.g. "run `ls`") → Allow/Deny dialog appears; Allow proceeds (tool line appears), Deny is honored.
- [ ] **commit:** `feat(se): tool permission Allow/Deny dialog`

---

# Task 5.8 — Interrupt, sessions, resilience, input gating

**Files:** Modify `src/main.c`, `src/ui.c`, `resources/MacCode.r` (Session menu wired).

- esc keyDown while `ST_AWAITING_RESPONSE` → `ProtoSendStop()`.
- File menu: New Conversation → `ProtoSendNew()` (optionally clear transcript or mark a divider); Resume Last → `ProtoSendResume()`. Session menu: Connect…/Disconnect (Disconnect → `NetClose`, `state=ST_DISCONNECTED`).
- Socket drop (NetPoll −1) → `ST_DISCONNECTED`, keep transcript, show an INFO/ERR line, offer reconnect (Session→Connect re-runs the connect path; reset `WireDecoderInit`).
- Input enabled only in `ST_IDLE`; greyed otherwise. About box. Apple-menu DAs via `OpenDeskAcc`.
- [ ] **EMULATOR CHECKPOINT:** interrupt a long turn; New/Resume; kill the proxy mid-turn → graceful Disconnected + reconnect; input gating correct.
- [ ] **commit:** `feat(se): interrupt, sessions, disconnect/reconnect, input gating`

---

# Task 5.9 — Cleanup + manual QA + remove scaffolding

**Files:** Modify `CMakeLists.txt` (remove `MyApp` stub target + `maccode_spike` target + `src/spike_main.c`, `resources/Spike.r`); delete `src/spike_main.c`, `resources/Spike.r`, and the template `src/main.c` stub remnants if any; revert `project(MyApp C CXX)` → `project(MacCode C)` (no CXX/CONSOLE anymore).

- [ ] Run the full manual QA checklist in Basilisk II (System 7.x): connect; prompt→stream with ✻ + ● ; Allow/Deny; esc interrupt; scrollback; New; Resume; disconnect/reconnect; no crashes; memory within the SIZE partition.
- [ ] `cd tests-native && make check` still green (wire+transcript unaffected).
- [ ] **commit:** `chore(se): remove spike scaffolding; MacCode is the app`

---

## Phase 5 exit criteria
Spec §14 success criteria met from a Mac SE in Basilisk II: launch → connect → prompt → streamed reply with verb + tool lines → Allow/Deny honored → esc interrupt → scrollback → new/resume → survives disconnect. Then Phase 6 (end-to-end polish, `docs/MACCODE.md`, encoding/resilience hardening) from the original plan.
