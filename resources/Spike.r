#include "Dialogs.r"
#include "Processes.r"

resource 'ALRT' (128) {
    {40, 40, 300, 480},
    128,
    { OK, visible, sound1; OK, visible, sound1; OK, visible, sound1; OK, visible, sound1 },
    alertPositionMainScreen
};

resource 'DITL' (128) {
    {
        {220, 360, 240, 430}, Button { enabled, "OK" };
        {10, 20, 210, 430}, StaticText { disabled, "^0" };
    }
};

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    1024 * 1024,
    512 * 1024
};
