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
#include "ui.h"
#include "proto.h"
#include "netmac.h"

#define kMBARID        128
#define kAppleMenuID   128
#define kFileMenuID    129
#define kEditMenuID    130
#define kSessionMenuID 131
#define kWindowID      128
#define kAlertID       128
#define kAboutItem     1
#define kNewItem       1   /* File menu: New(1) Resume(2) -(3) Quit(4) */
#define kResumeItem    2
#define kQuitItem      4
#define kConnectItem    1
#define kDisconnectItem 2
#define kSleep          4L
#define kConnectTimeoutTicks 600L   /* ~10s at 60 ticks/sec before giving up on a connect */

AppGlobals gApp;
static unsigned long gConnectDeadline = 0;   /* TickCount at which we abandon an in-progress connect */

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
void AppGiveTime(void){ SystemTask(); }
/* Begin a non-blocking connect; the event loop's PumpConnect drives it to completion. */
static void StartConnect(void){
  OSErr err;
  if (NetIsConnected() || gApp.state == ST_CONNECTING) return;
  WireDecoderInit(&gApp.dec);
  err = NetConnectBegin("10.0.2.2", 4242, AppGiveTime);
  if (err != noErr){
    gApp.state = ST_ERROR;
    UI_SetVerb("");
    TrAppend(gApp.transcript, "* could not start a connection", TR_ERR, UI_WrapCols());
    UI_TranscriptChanged();
    return;
  }
  gApp.state = ST_CONNECTING;
  gConnectDeadline = TickCount() + kConnectTimeoutTicks;
  UI_SetVerb("Connecting");
}

/* Tear down a failed/abandoned connect and report it in the transcript. */
static void ConnectFailed(const char *why){
  NetConnectCancel(AppGiveTime);   /* safe no-op if NetConnectPoll already cleaned up */
  gApp.state = ST_DISCONNECTED;
  UI_SetVerb("");
  UI_SetInputEnabled(false);
  TrAppend(gApp.transcript, why, TR_INFO, UI_WrapCols());
  UI_TranscriptChanged();
}

/* Advance an in-progress connect; called each idle pass. */
static void PumpConnect(void){
  int r;
  if (gApp.state != ST_CONNECTING) return;
  r = NetConnectPoll(AppGiveTime);
  if (r == 1){                                  /* still connecting */
    if ((long)(TickCount() - gConnectDeadline) >= 0)
      ConnectFailed("* could not reach the proxy (10.0.2.2:4242) \xD0 Session \xC9 Connect to retry");
    return;
  }
  if (r == 0){                                  /* connected */
    ProtoSendHello();
    gApp.state = ST_IDLE;
    UI_SetVerb("");
    UI_SetInputEnabled(true);
    return;
  }
  ConnectFailed("* could not connect \xD0 Session \xC9 Connect to retry");   /* r < 0 */
}
static void DoDisconnect(void){
  if (gApp.state == ST_CONNECTING) NetConnectCancel(AppGiveTime);
  else if (NetIsConnected()) NetClose(AppGiveTime);
  gApp.state = ST_DISCONNECTED;
  UI_SetVerb("");
  UI_SetInputEnabled(false);
}
static void PumpNetwork(void){
  static unsigned char tmp[2048];
  WireFrame fr;
  unsigned short room, want;
  long n;
  int r;

  if (!NetIsConnected()) return;
  room = (unsigned short)(WIRE_BUF_SIZE - gApp.dec.used);
  if (room == 0) return;                       /* full frame buffered but undrained — shouldn't happen */
  /* NetPoll spends 1 byte of maxLen on its NUL terminator (and refuses maxLen<2), so ask for
     room+1 (capped to tmp) — this lets us deliver a frame's final byte even when only 1 byte of
     decoder room remains. tmp is always >= maxLen, so the NUL stays in bounds. */
  want = (room + 1 < (unsigned short)sizeof(tmp)) ? (unsigned short)(room + 1) : (unsigned short)sizeof(tmp);
  n = NetPoll(tmp, want, AppGiveTime);
  if (n < 0){                                 /* socket closed/errored */
    DoDisconnect();
    TrAppend(gApp.transcript, "* disconnected", TR_INFO, UI_WrapCols());
    UI_TranscriptChanged();
    return;
  }
  if (n == 0) return;
  r = WireDecoderPush(&gApp.dec, tmp, (unsigned short)n, &fr);
  while (r == 1){
    ProtoDispatch(&fr);
    r = WireDecoderPush(&gApp.dec, NULL, 0, &fr);
  }
  if (r < 0){                                 /* fatal protocol error */
    DoDisconnect();
    WireDecoderInit(&gApp.dec);
    TrAppend(gApp.transcript, "* protocol error \xD0 disconnected", TR_ERR, UI_WrapCols());
    UI_TranscriptChanged();
  }
}
static void HandleMenu(long mc){
  short id = HiWord(mc), item = LoWord(mc);
  if (id == kAppleMenuID){
    if (item == kAboutItem) ShowMsg("MacCode \xC9 Claude Code for the Macintosh SE");
    else { Str255 nm; GetMenuItemText(GetMenuHandle(kAppleMenuID), item, nm); OpenDeskAcc(nm); }
  } else if (id == kFileMenuID){
    if (item == kNewItem)         { if (NetIsConnected() && gApp.state == ST_IDLE) ProtoSendNew(); }
    else if (item == kResumeItem) { if (NetIsConnected() && gApp.state == ST_IDLE) ProtoSendResume(); }
    else if (item == kQuitItem)   gApp.quitting = true;
  } else if (id == kSessionMenuID){
    if (item == kConnectItem) StartConnect();
    else if (item == kDisconnectItem) DoDisconnect();
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
  UI_Init();
  UI_SetInputEnabled(false);   /* enabled on successful connect */
  UI_Update();                 /* paint the empty window before the (possibly slow) connect */
  NetInit();                   /* open the MacTCP driver once before connecting */
  StartConnect();

  while (!gApp.quitting){
    if (WaitNextEvent(everyEvent, &ev, kSleep, NULL)){
      switch (ev.what){
        case mouseDown: {
          WindowPtr w; short part = FindWindow(ev.where, &w);
          switch (part){
            case inMenuBar: HandleMenu(MenuSelect(ev.where)); break;
            case inDrag:    DragWindow(w, ev.where, &qd.screenBits.bounds); break;
            case inGoAway:  if (TrackGoAway(w, ev.where)) gApp.quitting = true; break;
            case inContent: if (w != FrontWindow()) SelectWindow(w); else { Point pt = ev.where; GlobalToLocal(&pt); UI_ContentClick(pt); } break;
            case inSysWindow: SystemClick(&ev, w); break;
          }
        } break;
        case keyDown: case autoKey: {
          char c = (char)(ev.message & charCodeMask);
          if (ev.modifiers & cmdKey){ long mc = MenuKey(c); if (HiWord(mc)) HandleMenu(mc); }
          else if (c == 0x1B){                   /* esc */
            if (gApp.state == ST_AWAITING_RESPONSE){ ProtoSendStop(); UI_SetVerb("Stopping"); }
            else if (gApp.state == ST_CONNECTING){ DoDisconnect(); }
          }
          else {
            if (UI_InputKey(&ev)){               /* unmodified Return in the input box */
              if (gApp.state == ST_IDLE){
                char line[512];
                UI_GetInput(line, sizeof line);
                if (line[0]){
                  TrAppend(gApp.transcript, line, TR_USER, UI_WrapCols());
                  ProtoSendPrompt(line);
                  UI_ClearInput();
                  UI_TranscriptChanged();
                  gApp.state = ST_AWAITING_RESPONSE;
                  UI_SetInputEnabled(false);
                }
              }
              /* not idle: ignore Return */
            }
          }
        } break;
        case updateEvt: {
          WindowPtr uw = (WindowPtr)ev.message;
          SetPort(uw); BeginUpdate(uw);
          UI_Update();
          EndUpdate(uw);
        } break;
      }
    } else {
      UI_Idle();
      PumpConnect();
      PumpNetwork();
    }
  }
  if (gApp.state == ST_CONNECTING) NetConnectCancel(AppGiveTime);
  else if (NetIsConnected()) NetClose(AppGiveTime);
  return 0;
}
