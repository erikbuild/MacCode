#include "Windows.r"
#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'WIND' (128, "MacCode") {
    {40, 2, 340, 510}, documentProc, visible, goAway, 0x0, "Claude Code", noAutoCenter
};

resource 'MBAR' (128) { { 128, 129, 130, 131 } };

resource 'MENU' (128) {
    128, textMenuProc, allEnabled, enabled, apple,
    { "About MacCode\xC9", noIcon, noKey, noMark, plain;
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
    { "Connect\xC9", noIcon, noKey, noMark, plain;
      "Disconnect", noIcon, noKey, noMark, plain }
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

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    1024 * 1024, 512 * 1024
};
