/* ABOUTME: MacCode wire protocol — frame encode/decode, shared semantics with the proxy.
   ABOUTME: Pure C, no Toolbox. Big-endian lengths. Keep RT_* in sync with proxy/src/protocol.ts. */
#ifndef WIRE_H
#define WIRE_H
#include <stddef.h>

/* proxy -> SE */
#define RT_TEXT 0x01
#define RT_VERB 0x02
#define RT_TOOL 0x03
#define RT_ASK  0x04
#define RT_DONE 0x05
#define RT_INFO 0x06
#define RT_ERR  0x07
/* SE -> proxy */
#define RT_HELLO  0x10
#define RT_PROMPT 0x11
#define RT_PERM   0x12
#define RT_STOP   0x13
#define RT_NEW    0x14
#define RT_RESUME 0x15

#define WIRE_MAX_PAYLOAD 0xFFFF
#define WIRE_BUF_SIZE    4096   /* max in-flight frame; proxy MUST chunk text payloads to fit (<= WIRE_BUF_SIZE-3) */

/* Encode a frame into out (must hold 3+len bytes). Returns total bytes, or -1 if len > WIRE_MAX_PAYLOAD. */
long WireEncode(unsigned char type, const unsigned char *payload, unsigned short len, unsigned char *out);

typedef struct {
  unsigned char buf[WIRE_BUF_SIZE];
  unsigned short used;
  unsigned short pending;   /* bytes of a returned-but-not-yet-consumed frame (deferred shift) */
} WireDecoder;

typedef struct { unsigned char type; const unsigned char *payload; unsigned short len; } WireFrame;

void WireDecoderInit(WireDecoder *d);
/* Feed bytes (data may be NULL/len 0 to drain). Returns:
     1  = frame ready; payload points into d->buf, valid only until the next push — copy it immediately,
     0  = need more bytes,
    -1  = protocol/usage error: declared length can't fit WIRE_BUF_SIZE, OR this push exceeds remaining buffer room.
   Caller contract: feed at most WIRE_BUF_SIZE-used bytes per call and DRAIN by calling
   WireDecoderPush(d, NULL, 0, &fr) repeatedly until it returns 0 before feeding more;
   treat -1 as fatal — drop the connection and call WireDecoderInit before any reuse.
   Note: the proxy must chunk text payloads to <= WIRE_BUF_SIZE-3. */
int WireDecoderPush(WireDecoder *d, const unsigned char *data, unsigned short len, WireFrame *out);

#endif
