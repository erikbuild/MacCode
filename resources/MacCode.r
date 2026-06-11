#include "Windows.r"
#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'WIND' (128, "MacCode") {
    {40, 2, 340, 510}, documentProc, visible, goAway, 0x0, "MacCode", noAutoCenter
};

resource 'MBAR' (128) { { 128, 129, 130, 131, 132 } };

resource 'MENU' (128) {
    128, textMenuProc, allEnabled, enabled, apple,
    { "About MacCode...", noIcon, noKey, noMark, plain;
      "-", noIcon, noKey, noMark, plain }
};
resource 'MENU' (129, "File") {
    129, textMenuProc, allEnabled, enabled, "File",
    { "New Conversation", noIcon, noKey, noMark, plain;
      "Resume Last", noIcon, noKey, noMark, plain;
      "-", noIcon, noKey, noMark, plain;
      "Quit", noIcon, "Q", noMark, plain }
};
resource 'MENU' (130, "Edit") {
    130, textMenuProc, allEnabled, enabled, "Edit",
    { "Undo", noIcon, "Z", noMark, plain; "-", noIcon, noKey, noMark, plain;
      "Cut", noIcon, "X", noMark, plain; "Copy", noIcon, "C", noMark, plain;
      "Paste", noIcon, "V", noMark, plain; "Clear", noIcon, noKey, noMark, plain }
};
resource 'MENU' (131, "Session") {
    131, textMenuProc, allEnabled, enabled, "Session",
    { "Connect...", noIcon, noKey, noMark, plain;
      "Disconnect", noIcon, noKey, noMark, plain;
      "-", noIcon, noKey, noMark, plain;
      "Server...", noIcon, noKey, noMark, plain }
};
resource 'MENU' (132, "View") {
    132, textMenuProc, allEnabled, enabled, "View",
    { "Dark Mode", noIcon, noKey, noMark, plain }
};

resource 'ALRT' (128) {
    {60, 60, 200, 452}, 128,
    { OK, visible, sound1; OK, visible, sound1; OK, visible, sound1; OK, visible, sound1 },
    alertPositionMainScreen
};
resource 'DITL' (128) {
    { {110, 312, 130, 382}, Button { enabled, "OK" };
      {10, 20, 100, 382}, StaticText { disabled, "^0" } }
};

resource 'ALRT' (129) {
    {60, 60, 210, 452}, 129,
    { OK, visible, sound1; OK, visible, sound1; OK, visible, sound1; OK, visible, sound1 },
    alertPositionMainScreen
};
resource 'DITL' (129) {
    { {120, 312, 140, 382}, Button { enabled, "Deny" };
      {120, 222, 140, 302}, Button { enabled, "Allow" };
      {10, 70, 110, 382}, StaticText { disabled, "^0" } }
};

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

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    1024 * 1024, 512 * 1024
};

/* ---- App icon + Finder bundle (raw data; Retro68's Rez has no ICN#/BNDL templates) ----
   Generated from assets/icon.gif (the Claude sparkle) — 1-bit silhouette, icon == mask. */

data 'MCde' (0) {                         /* signature resource: "MacCode 1.2.0" pstring */
    $"0D4D6163436F646520312E322E30"
};

data 'vers' (1) {                         /* Finder "Get Info": Version 1.2.0 / "MacCode 1.2.0" */
    $"01208000000005312E322E300D4D6163436F646520312E322E30"
};

data 'FREF' (128) {                       /* 'APPL', icon local id 0, empty name */
    $"4150504C000000"
};

data 'BNDL' (128) {                       /* owner 'MCde'; maps local 0 -> ICN#/FREF id 128 */
    $"4D4364650000000149434E2300000000008046524546000000000080"
};

data 'ICN#' (128) {                       /* 32x32 icon + mask */
    $"01E0700001F0780001F0780001F870F0"
    $"00F8F1F010FCF3F03C7CF3F03F7EF7F0"
    $"3FBEFFE01FDFFFC00FFFFF8003FFFF00"
    $"01FFFF0F007FFFFFE03FFFFFFFFFFFFE"
    $"FFFFFFC00FFFFFFE003FFFFF00FFFFFF"
    $"03FFFF1F07FFFF801FDFFFC01FBDFFE0"
    $"1E7DDFF000FBDF7800F3CFB801E38798"
    $"01C3878001C383800007800000038000"
    $"01E0700001F0780001F0780001F870F0"
    $"00F8F1F010FCF3F03C7CF3F03F7EF7F0"
    $"3FBEFFE01FDFFFC00FFFFF8003FFFF00"
    $"01FFFF0F007FFFFFE03FFFFFFFFFFFFE"
    $"FFFFFFC00FFFFFFE003FFFFF00FFFFFF"
    $"03FFFF1F07FFFF801FDFFFC01FBDFFE0"
    $"1E7DDFF000FBDF7800F3CFB801E38798"
    $"01C3878001C383800007800000038000"
};

data 'ics#' (128) {                       /* 16x16 icon + mask */
    $"0C400CC00EDC6EDC7FF83FF00FF7EFFF"
    $"FFFE0FFF1FF23FF82FFC0DB619100180"
    $"0C400CC00EDC6EDC7FF83FF00FF7EFFF"
    $"FFFE0FFF1FF23FF82FFC0DB619100180"
};
