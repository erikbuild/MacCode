/* ABOUTME: Persistent TCP connection for MacCode over the vendored TCPHi/MacTCP stack.
   ABOUTME: Non-blocking NetPoll for event-loop integration; dotted-quad addressing (no DNR). */
#ifndef NETMAC_H
#define NETMAC_H
#include <MacTypes.h>

typedef void (*NetGiveTime)(void);

OSErr   NetInit(void);                                                  /* open MacTCP driver (once) */
OSErr   NetConnect(const char *dottedQuad, unsigned short port, NetGiveTime giveTime);
Boolean NetIsConnected(void);
OSErr   NetSend(const void *data, unsigned short len, NetGiveTime giveTime);
/* Non-blocking: bytes read into buf (0 if none available), or -1 on close/error. */
long    NetPoll(void *buf, unsigned short maxLen, NetGiveTime giveTime);
void    NetClose(NetGiveTime giveTime);

#endif
