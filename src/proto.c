/* ABOUTME: SE protocol glue — build/send outbound frames and dispatch one decoded inbound frame. */
/* ABOUTME: Bridges the wire codec (wire.h) to the netmac socket and the UI/transcript model. */
#include "proto.h"
#include "app.h"
#include "ui.h"
#include "netmac.h"
#include "transcript.h"
#include "wire.h"
#include <string.h>

/* Encode and send one frame. Static buffer (single-threaded, non-re-entrant). */
static void SendFrame(unsigned char type, const unsigned char *payload, unsigned short len){
  static unsigned char out[WIRE_BUF_SIZE];
  long total = WireEncode(type, payload, len, out);
  if (total > 0 && NetIsConnected()){
    NetSend(out, (unsigned short)total, AppGiveTime);
  }
}

static unsigned long ReadU32BE(const unsigned char *p){
  return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
         ((unsigned long)p[2] << 8)  | (unsigned long)p[3];
}

/* Copy a length-bounded wire payload into a NUL-terminated temp and append it. */
static void AppendPayload(const unsigned char *p, unsigned short len, TrKind kind){
  static char tmp[WIRE_BUF_SIZE];          /* NUL-terminated copy of one payload */
  unsigned short n = len;
  if (n > (unsigned short)(sizeof(tmp) - 1)) n = (unsigned short)(sizeof(tmp) - 1);
  if (n > 0) memcpy(tmp, p, n);
  tmp[n] = '\0';
  TrAppend(gApp.transcript, tmp, kind, UI_WrapCols());
}

void ProtoSendHello(void){
  unsigned char v[2] = { (unsigned char)(kProtocolVersion >> 8),
                         (unsigned char)(kProtocolVersion & 0xFF) };
  SendFrame(RT_HELLO, v, 2);
}

void ProtoSendPrompt(const char *text){
  static unsigned char payload[WIRE_BUF_SIZE];
  unsigned short len = 0;
  unsigned short cap = (unsigned short)(WIRE_BUF_SIZE - 3);
  while (text[len] != '\0' && len < cap){
    char c = text[len];
    payload[len] = (c == '\r') ? '\n' : (unsigned char)c;
    len++;
  }
  SendFrame(RT_PROMPT, payload, len);
}

void ProtoSendPerm(unsigned long id, Boolean allow){
  unsigned char buf[5];
  buf[0] = (unsigned char)(id >> 24);
  buf[1] = (unsigned char)(id >> 16);
  buf[2] = (unsigned char)(id >> 8);
  buf[3] = (unsigned char)(id & 0xFF);
  buf[4] = allow ? 1 : 0;
  SendFrame(RT_PERM, buf, 5);
}

void ProtoSendStop(void){   SendFrame(RT_STOP,   NULL, 0); }
void ProtoSendNew(void){    SendFrame(RT_NEW,    NULL, 0); }
void ProtoSendResume(void){ SendFrame(RT_RESUME, NULL, 0); }

void ProtoDispatch(const WireFrame *f){
  switch (f->type){
    case RT_TEXT:
      AppendPayload(f->payload, f->len, TR_ASSISTANT);
      UI_TranscriptChanged();
      break;
    case RT_TOOL:
      AppendPayload(f->payload, f->len, TR_TOOL);
      UI_TranscriptChanged();
      break;
    case RT_INFO:
      AppendPayload(f->payload, f->len, TR_INFO);
      UI_TranscriptChanged();
      break;
    case RT_ERR:
      AppendPayload(f->payload, f->len, TR_ERR);
      UI_TranscriptChanged();
      UI_SetVerb("");
      gApp.state = ST_IDLE;
      UI_SetInputEnabled(true);
      break;
    case RT_VERB: {
      /* empty payload => clear; otherwise copy to a NUL-terminated temp and set */
      static char vtmp[64];
      unsigned short n = f->len;
      if (n > (unsigned short)(sizeof(vtmp) - 1)) n = (unsigned short)(sizeof(vtmp) - 1);
      if (n > 0) memcpy(vtmp, f->payload, n);
      vtmp[n] = '\0';
      UI_SetVerb(vtmp);
      break;
    }
    case RT_DONE:
      UI_SetVerb("");
      gApp.state = ST_IDLE;
      UI_SetInputEnabled(true);
      break;
    case RT_ASK: {
      if (f->len >= 4){
        Boolean allow;
        gApp.pendingAskId = ReadU32BE(f->payload);
        gApp.state = ST_AWAITING_PERMISSION;
        /* UI_ShowPermission copies the description before running the modal, so the
           transient payload pointer is safe. */
        allow = UI_ShowPermission((const char *)(f->payload + 4), (short)(f->len - 4));
        ProtoSendPerm(gApp.pendingAskId, allow);
        gApp.pendingAskId = 0;
        gApp.state = ST_AWAITING_RESPONSE;   /* turn continues after the decision */
      }
      break;
    }
    default:
      break;   /* unknown type: ignore */
  }
}
