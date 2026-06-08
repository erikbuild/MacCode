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
  AppState      state;
  Transcript   *transcript;     /* NewPtr'd (~52KB) — never on the stack */
  WireDecoder   dec;
  WindowPtr     win;
  Boolean       quitting;
  char          verb[64];       /* current verb text ("" = none) */
  unsigned long pendingAskId;   /* ASK id awaiting PERM (0 = none) */
  short         scrollTop;      /* first visible transcript line */
} AppGlobals;

extern AppGlobals gApp;
#define kProtocolVersion 1

void AppGiveTime(void);   /* SystemTask-only give-time callback for blocking MacTCP calls */
#endif
