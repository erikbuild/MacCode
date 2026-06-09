/* ABOUTME: MacCode wire protocol codec — frame encode + incremental decode.
   ABOUTME: Pure C; shared semantics with proxy/src/protocol.ts. */
#include "wire.h"
#include <string.h>

long WireEncode(unsigned char type, const unsigned char *payload, unsigned short len, unsigned char *out) {
  if (len > WIRE_MAX_PAYLOAD) return -1;
  out[0] = type;
  out[1] = (unsigned char)(len >> 8);
  out[2] = (unsigned char)(len & 0xff);
  if (len && payload) memcpy(out + 3, payload, len);
  return 3L + len;
}

void WireDecoderInit(WireDecoder *d) { d->used = 0; d->pending = 0; }

int WireDecoderPush(WireDecoder *d, const unsigned char *data, unsigned short len, WireFrame *out) {
  /* Consume the previously-returned frame now. The shift is deferred to here — rather than
     done in the push that returned the frame — so out->payload (which points into d->buf)
     stays valid until this next call, as the header contract promises. */
  if (d->pending > 0) {
    memmove(d->buf, d->buf + d->pending, (size_t)(d->used - d->pending));
    d->used = (unsigned short)(d->used - d->pending);
    d->pending = 0;
  }
  if (len && data) {
    unsigned short room = (unsigned short)(sizeof(d->buf) - d->used);
    if (len > room) return -1;   /* would overflow the buffer; refuse rather than silently drop (caller must drop the connection) */
    memcpy(d->buf + d->used, data, len);
    d->used = (unsigned short)(d->used + len);
  }
  if (d->used < 3) return 0;
  {
    unsigned short plen = (unsigned short)(((unsigned)d->buf[1] << 8) | d->buf[2]);
    unsigned long total = 3UL + plen;
    if (total > sizeof(d->buf)) return -1;          /* can never fit -> protocol error */
    if (d->used < total) return 0;                  /* need more bytes */
    out->type = d->buf[0];
    out->payload = d->buf + 3;
    out->len = plen;
    d->pending = (unsigned short)total;             /* defer the shift to the next push */
    return 1;
  }
}
