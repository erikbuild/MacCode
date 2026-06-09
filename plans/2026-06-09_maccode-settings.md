# MacCode 1.1.0 — Proxy Server Settings (IP/Port) — implementation plan

> **For agentic workers:** Execute task-by-task with the implement → Opus-review flow. GUI tasks end with an **emulator checkpoint** Erik runs (the cross-build is authoritative; editor clang "header not found" errors are host false positives). **Erik commits — never commit on your own.** Build the SE app with `cmake --build build --target MacCode_APPL` (the `_APPL` target, not bare `MacCode`).

**Goal:** Let the user set the proxy server **IP and port** from a settings dialog on the SE, replacing the hardcoded `10.0.2.2:4242`, and **persist** the choice in a preferences file (store-only — the user then uses Session ▸ Connect).

**Architecture:** The target address moves into `AppGlobals` (`serverIP`/`serverPort`, default `10.0.2.2`/`4242`). A new `src/prefs.c` reads/writes a `"MacCode Prefs"` text file (`ip:port`) in the System Folder's Preferences folder (`FindFolder` + FSSpec I/O). A modal `DLOG`/`DITL` 200, opened from **Session ▸ Server…**, edits the fields, validates, and on OK stores into `gApp` + saves the prefs file.

**Tech:** C / Retro68 + classic Toolbox (Dialog Manager, File Manager `FSSpec`/`FSp*`, `FindFolder`). All APIs confirmed present in the Multiversal CIncludes.

## File map
```
src/app.h            # AppGlobals: + char serverIP[16]; + unsigned short serverPort;
src/prefs.h / .c     # NEW: PrefsLoad()/PrefsSave() — Preferences-folder "MacCode Prefs" file
src/main.c           # init defaults; PrefsLoad() at launch; StartConnect uses gApp.server*; menu wiring
src/ui.h / .c        # UI_ShowSettings() — modal dialog: edit/validate IP+port, store + PrefsSave
resources/MacCode.r  # DLOG/DITL 200; Session menu "Server…" item
CMakeLists.txt       # add src/prefs.c to the MacCode target
```

---

# Task 1 — Server address in app state + prefs file (model layer)

No UI yet; after this, the connect target lives in `gApp` and is loaded from / saved to the prefs file. Verifiable by build + a no-prefs-file launch (still connects to the default).

**Files:** Modify `src/app.h`, `src/main.c`, `CMakeLists.txt`; Create `src/prefs.h`, `src/prefs.c`.

- [ ] **Step 1 — `src/app.h`:** add two fields to `AppGlobals` (after `short scrollTop;`):
```c
  char          serverIP[16];   /* dotted-quad proxy address, e.g. "10.0.2.2" */
  unsigned short serverPort;    /* proxy TCP port, e.g. 4242 */
```

- [ ] **Step 2 — `src/prefs.h`:**
```c
/* ABOUTME: Load/save the proxy server IP+port to a "MacCode Prefs" file in the
   ABOUTME: System Folder's Preferences folder. Keeps gApp defaults if absent/invalid. */
#ifndef PREFS_H
#define PREFS_H
void PrefsLoad(void);   /* read serverIP/serverPort into gApp (no-op if no/invalid file) */
void PrefsSave(void);   /* write gApp's serverIP/serverPort to the prefs file */
#endif
```

- [ ] **Step 3 — `src/prefs.c`:**
```c
/* ABOUTME: Preferences-folder persistence for the proxy server IP+port.
   ABOUTME: Stores "ip:port" as a small text file; classic File Manager (FSSpec) I/O. */
#include <Multiverse.h>
#include <string.h>
#include <stdio.h>      /* sscanf / sprintf */
#include "app.h"
#include "prefs.h"

#define PREFS_NAME    "\pMacCode Prefs"
#define PREFS_CREATOR 'MCde'
#define PREFS_TYPE    'pref'

/* FSSpec for the prefs file in the (created-if-needed) Preferences folder. */
static OSErr PrefsSpec(FSSpec *spec) {
    short vRefNum; long dirID; OSErr err;
    err = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &vRefNum, &dirID);
    if (err != noErr) return err;
    return FSMakeFSSpec(vRefNum, dirID, PREFS_NAME, spec);
}

void PrefsLoad(void) {
    FSSpec spec; short refNum; long count; OSErr err; char buf[48];
    unsigned long a, b, c, d, port; char *colon;
    if (PrefsSpec(&spec) != noErr) return;                 /* keep defaults */
    if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr) return; /* no file -> defaults */
    count = (long)(sizeof(buf) - 1);
    err = FSRead(refNum, &count, buf);                      /* eofErr is fine — bytes still read */
    FSClose(refNum);
    if ((err != noErr && err != eofErr) || count <= 0) return;
    buf[count] = '\0';
    if (sscanf(buf, "%lu.%lu.%lu.%lu:%lu", &a, &b, &c, &d, &port) == 5 &&
        a < 256 && b < 256 && c < 256 && d < 256 && port > 0 && port < 65536) {
        colon = strchr(buf, ':');
        if (colon) *colon = '\0';                           /* keep just the IP text */
        strncpy(gApp.serverIP, buf, sizeof(gApp.serverIP) - 1);
        gApp.serverIP[sizeof(gApp.serverIP) - 1] = '\0';
        gApp.serverPort = (unsigned short)port;
    }
}

void PrefsSave(void) {
    FSSpec spec; short refNum; long count; char buf[48];
    if (PrefsSpec(&spec) != noErr) return;
    FSpCreate(&spec, PREFS_CREATOR, PREFS_TYPE, smSystemScript);  /* dupFNErr is fine */
    if (FSpOpenDF(&spec, fsWrPerm, &refNum) != noErr) return;
    SetFPos(refNum, fsFromStart, 0);
    SetEOF(refNum, 0);
    sprintf(buf, "%s:%u", gApp.serverIP, (unsigned)gApp.serverPort);
    count = (long)strlen(buf);
    FSWrite(refNum, &count, buf);
    FSClose(refNum);
}
```

- [ ] **Step 4 — `src/main.c`:** set the defaults during init (next to the other `gApp.*` initialisers, before `UI_Init()`), then load any saved prefs before connecting. Add:
```c
  /* default proxy address, overridden by the prefs file if present */
  { const char *def = "10.0.2.2"; short i = 0; while (def[i] && i < 15){ gApp.serverIP[i] = def[i]; i++; } gApp.serverIP[i] = '\0'; }
  gApp.serverPort = 4242;
```
and after `NetInit();` (before `StartConnect();`), add `PrefsLoad();`. Add `#include "prefs.h"` to the includes.

- [ ] **Step 5 — `src/main.c` `StartConnect`:** change the hardcoded connect to use the app state. Replace:
```c
  err = NetConnectBegin("10.0.2.2", 4242, AppGiveTime);
```
with:
```c
  err = NetConnectBegin(gApp.serverIP, gApp.serverPort, AppGiveTime);
```

- [ ] **Step 6 — `CMakeLists.txt`:** add `src/prefs.c` to the `add_application(MacCode ...)` source list (next to `src/proto.c`).

- [ ] **Step 7 — build** `MacCode_APPL`, clean. (No visible change yet; with no prefs file, defaults `10.0.2.2:4242` are used.)
- [ ] **EMULATOR CHECKPOINT (Erik):** launch + Session ▸ Connect still connects to `10.0.2.2:4242` exactly as before (proves the state-driven connect works and PrefsLoad is harmless with no file).
- [ ] **commit:** `feat(se): proxy address in app state + prefs file persistence`

---

# Task 2 — Settings dialog + Session ▸ Server… menu

**Files:** Modify `resources/MacCode.r` (DLOG/DITL 200 + Session menu item), `src/ui.h`, `src/ui.c`, `src/main.c`.

- [ ] **Step 1 — `resources/MacCode.r`:** add the dialog. A modal `dBoxProc` window, initially invisible (shown after the fields are filled). Items: OK(1, default), Cancel(2), IP editText(3), Port editText(4), labels(5–7).
```
resource 'DLOG' (200, "Server") {
    {80, 90, 226, 410}, dBoxProc, invisible, noGoAway, 0x0, 200, "Server", noAutoCenter
};
resource 'DITL' (200) {
    { {112, 236, 132, 306}, Button { enabled, "OK" };
      {112, 150, 132, 220}, Button { enabled, "Cancel" };
      { 34, 96,  50, 300}, EditText { enabled, "" };
      { 64, 96,  80, 180}, EditText { enabled, "" };
      { 34, 16,  50, 92},  StaticText { disabled, "Server IP:" };
      { 64, 16,  80, 92},  StaticText { disabled, "Port:" };
      {  8, 16,  26, 300}, StaticText { disabled, "Proxy server address" } }
};
```
Also add a **"Server…"** item to the Session `MENU` (131). Change its item list to:
```
    { "Connect...", noIcon, noKey, noMark, plain;
      "Disconnect", noIcon, noKey, noMark, plain;
      "-", noIcon, noKey, noMark, plain;
      "Server...", noIcon, noKey, noMark, plain }
```

- [ ] **Step 2 — `src/ui.h`:** add `void UI_ShowSettings(void);` next to the other `UI_*` prototypes.

- [ ] **Step 3 — `src/ui.c`:** add the dialog item ids near the other `#define`s:
```c
#define kSettingsDLOG   200
#define kSetOKItem      1
#define kSetCancelItem  2
#define kSetIPItem      3
#define kSetPortItem    4
```
Add small Str255⇄C helpers and a dotted-quad validator (near the top of the file, after the statics):
```c
/* Str255 (Pascal) -> C string into out (max incl NUL). */
static void PToC(ConstStr255Param p, char *out, short max) {
    short n = p[0];
    if (n > max - 1) n = max - 1;
    BlockMoveData(p + 1, out, n);
    out[n] = '\0';
}
/* C string -> Str255 (Pascal). */
static void CToP(const char *c, Str255 p) {
    short n = 0;
    while (c[n] && n < 255) { p[n + 1] = (unsigned char)c[n]; n++; }
    p[0] = (unsigned char)n;
}
/* true if s is a valid dotted-quad (four 0..255 octets). */
static Boolean ValidIP(const char *s) {
    unsigned long a, b, c, d; char extra;
    if (sscanf(s, "%lu.%lu.%lu.%lu%c", &a, &b, &c, &d, &extra) != 4) return false;
    return (a < 256 && b < 256 && c < 256 && d < 256);
}
```
Implement the modal dialog (place near `UI_ShowPermission`):
```c
void UI_ShowSettings(void) {
    DialogPtr dlg;
    short hit, itype;
    Handle ih;
    Rect ir;
    Str255 s;
    char ip[16];
    long port;

    dlg = GetNewDialog(kSettingsDLOG, NULL, (WindowPtr)-1L);
    if (!dlg) return;

    /* preload current values */
    CToP(gApp.serverIP, s);
    GetDialogItem(dlg, kSetIPItem, &itype, &ih, &ir);   SetDialogItemText(ih, s);
    NumToString((long)gApp.serverPort, s);
    GetDialogItem(dlg, kSetPortItem, &itype, &ih, &ir); SetDialogItemText(ih, s);
    SelectDialogItemText(dlg, kSetIPItem, 0, 32767);
    ShowWindow(dlg);

    for (;;) {
        ModalDialog(NULL, &hit);
        if (hit == kSetCancelItem) break;
        if (hit == kSetOKItem) {
            GetDialogItem(dlg, kSetIPItem, &itype, &ih, &ir);   GetDialogItemText(ih, s); PToC(s, ip, sizeof ip);
            GetDialogItem(dlg, kSetPortItem, &itype, &ih, &ir); GetDialogItemText(ih, s); StringToNum(s, &port);
            if (ValidIP(ip) && port > 0 && port < 65536) {
                short i = 0; while (ip[i] && i < 15) { gApp.serverIP[i] = ip[i]; i++; } gApp.serverIP[i] = '\0';
                gApp.serverPort = (unsigned short)port;
                PrefsSave();
                break;
            }
            SysBeep(10);   /* invalid IP/port: stay in the dialog */
        }
    }
    DisposeDialog(dlg);
}
```
Add `#include "prefs.h"` to ui.c's includes (for `PrefsSave`). `NumToString`/`StringToNum`/`SysBeep`/the Dialog calls come via the already-included Toolbox headers (`ui.c` includes `<Multiverse.h>`).

- [ ] **Step 4 — `src/main.c`:** wire the menu. Add `#define kServerItem 4` near the Session item defines, and in `HandleMenu`'s Session branch:
```c
  } else if (id == kSessionMenuID){
    if (item == kConnectItem) StartConnect();
    else if (item == kDisconnectItem) DoDisconnect();
    else if (item == kServerItem) UI_ShowSettings();
  }
```
(Session menu items are now Connect(1) / Disconnect(2) / -(3) / Server(4).)

- [ ] **Step 5 — build** `MacCode_APPL`, clean.
- [ ] **EMULATOR CHECKPOINT (Erik):**
  1. **Session ▸ Server…** opens the dialog showing the current `10.0.2.2` / `4242`.
  2. Change the IP and/or port, click **OK**; an **invalid** IP (e.g. `1.2.3`) or out-of-range port beeps and keeps the dialog open; **Cancel** discards changes.
  3. After OK, **Session ▸ Connect** connects to the **new** address (watch the proxy log / point it at a different `--port` to confirm).
  4. **Quit and relaunch** → the dialog shows the saved values and connect targets them (proves the prefs file persisted).
- [ ] **commit:** `feat(se): proxy server settings dialog (Session ▸ Server…)`

---

## Exit criteria
The proxy IP/port are editable from Session ▸ Server…, validated, used by Connect, and persisted across launches in the Preferences folder. Bump the version to **1.1.0** (`project(... VERSION 1.1.0)`, `proxy/package.json`, the `'vers'`/signature resources) as the release step after both tasks land and the checkpoint passes.
```
