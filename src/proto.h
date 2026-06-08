/* ABOUTME: SE protocol glue — build/send outbound frames and dispatch one decoded inbound frame. */
/* ABOUTME: Bridges the wire codec (wire.h) to the netmac socket and the UI/transcript model. */
#ifndef PROTO_H
#define PROTO_H
#include <MacTypes.h>
#include "wire.h"

void ProtoSendHello(void);                          /* 2-byte BE kProtocolVersion */
void ProtoSendPrompt(const char *text);             /* '\r' -> '\n'; RT_PROMPT */
void ProtoSendPerm(unsigned long id, Boolean allow);/* 4-byte BE id + 1 allow byte; RT_PERM */
void ProtoSendStop(void);                           /* RT_STOP, empty */
void ProtoSendNew(void);                            /* RT_NEW, empty */
void ProtoSendResume(void);                         /* RT_RESUME, empty */
void ProtoDispatch(const WireFrame *f);             /* update gApp + UI from one decoded frame */

#endif
