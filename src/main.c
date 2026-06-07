/* ABOUTME: MacCode entry — toolbox init, window, menus, event loop + state machine. */
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <Events.h>
#include <Dialogs.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <Devices.h>
#include <OSUtils.h>
#include "app.h"

#define kMBARID        128
#define kAppleMenuID   128
#define kFileMenuID    129
#define kEditMenuID    130
#define kSessionMenuID 131
#define kWindowID      128
#define kAlertID       128
#define kAboutItem     1
#define kQuitItem      4   /* File: New(1) Resume(2) -(3) Quit(4) */

AppGlobals gApp;

static void InitToolbox(void){
  InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus();
  TEInit(); InitDialogs(NULL); InitCursor(); FlushEvents(everyEvent, 0);
}
static void SetupMenus(void){
  Handle mbar = GetNewMBar(kMBARID);
  SetMenuBar(mbar);
  AppendResMenu(GetMenuHandle(kAppleMenuID), 'DRVR');
  DrawMenuBar();
}
static void ShowMsg(const char *cmsg){
  Str255 p; size_t n = 0; while (cmsg[n] && n < 255) n++;
  p[0] = (unsigned char)n; BlockMoveData(cmsg, p+1, n);
  ParamText(p, "\p", "\p", "\p");
  NoteAlert(kAlertID, NULL);
}
static void HandleMenu(long mc){
  short id = HiWord(mc), item = LoWord(mc);
  if (id == kAppleMenuID){
    if (item == kAboutItem) ShowMsg("MacCode \xC9 Claude Code for the Macintosh SE");
    else { Str255 nm; GetMenuItemText(GetMenuHandle(kAppleMenuID), item, nm); OpenDeskAcc(nm); }
  } else if (id == kFileMenuID){
    if (item == kQuitItem) gApp.quitting = true;
    /* New / Resume wired in Task 5.8 */
  } else if (id == kSessionMenuID){
    /* Connect / Disconnect wired in Task 5.6 */
  }
  HiliteMenu(0);
}
int main(void){
  EventRecord ev;
  InitToolbox();
  SetupMenus();
  gApp.transcript = (Transcript*)NewPtr((Size)sizeof(Transcript));
  if (gApp.transcript == NULL){ ShowMsg("Out of memory: cannot allocate transcript."); return 0; }
  TrInit(gApp.transcript);
  WireDecoderInit(&gApp.dec);
  gApp.win = GetNewWindow(kWindowID, NULL, (WindowPtr)-1L);
  SetPort(gApp.win);
  gApp.state = ST_DISCONNECTED; gApp.quitting = false;
  gApp.verb[0] = '\0'; gApp.pendingAskId = 0; gApp.scrollTop = 0;

  while (!gApp.quitting){
    if (WaitNextEvent(everyEvent, &ev, 10L, NULL)){
      switch (ev.what){
        case mouseDown: {
          WindowPtr w; short part = FindWindow(ev.where, &w);
          switch (part){
            case inMenuBar: HandleMenu(MenuSelect(ev.where)); break;
            case inDrag:    DragWindow(w, ev.where, &qd.screenBits.bounds); break;
            case inGoAway:  if (TrackGoAway(w, ev.where)) gApp.quitting = true; break;
            case inContent: if (w != FrontWindow()) SelectWindow(w); break;  /* transcript clicks in 5.4 */
            case inSysWindow: SystemClick(&ev, w); break;
          }
        } break;
        case keyDown: case autoKey: {
          char c = ev.message & charCodeMask;
          if (ev.modifiers & cmdKey){ long mc = MenuKey(c); if (HiWord(mc)) HandleMenu(mc); }
          /* text input handled in 5.5 */
        } break;
        case updateEvt: {
          WindowPtr uw = (WindowPtr)ev.message;
          SetPort(uw); BeginUpdate(uw);
          /* transcript drawing added in 5.4 */
          EndUpdate(uw);
        } break;
      }
    }
  }
  return 0;
}
