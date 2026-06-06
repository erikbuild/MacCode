/* ABOUTME: Persistent TCP connection wrapper over the vendored MacTCP/TCPHi stack.
   ABOUTME: Provides NetInit/NetConnect/NetSend/NetPoll/NetClose for the SE app event loop. */
#include "netmac.h"
#include "TCPHi.h"
#include "TCPRoutines.h"
#include "MacTCP.h"
#include <stdio.h>   /* sscanf */

#define NET_RECV_BUF 8192

static unsigned long gStream    = 0;
static Boolean       gConnected = false;
static bool          gCancel    = false;

/* Releases the stream and its receive buffer, then marks us disconnected.
   ReleaseStream is the correct way to free buffers even on an already
   closing/terminated stream. Safe to call on every dead-connection path;
   guarded so it never double-releases. */
static void teardown(NetGiveTime giveTime) {
    if (gStream) {
        ReleaseStream(gStream, (GiveTimePtr)giveTime, &gCancel);
        gStream = 0;
    }
    gConnected = false;
}

static unsigned long parseIP(const char *s) {
    unsigned long a, b, c, d;
    if (sscanf(s, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

OSErr NetInit(void) {
    return InitNetwork();
}

OSErr NetConnect(const char *dottedQuad, unsigned short port, NetGiveTime giveTime) {
    OSErr err;
    unsigned long ip = parseIP(dottedQuad);
    if (gConnected) return (OSErr)-1;   /* don't overwrite/leak an existing stream */
    if (ip == 0) return (OSErr)-1;
    err = CreateStream(&gStream, NET_RECV_BUF, (GiveTimePtr)giveTime, &gCancel);
    if (err != noErr) return err;
    err = OpenConnection(gStream, (long)ip, (short)port, 30, (GiveTimePtr)giveTime, &gCancel);
    if (err != noErr) {
        ReleaseStream(gStream, (GiveTimePtr)giveTime, &gCancel);
        gStream = 0;
        return err;
    }
    gConnected = true;
    gCancel    = false;
    return noErr;
}

Boolean NetIsConnected(void) {
    return gConnected;
}

OSErr NetSend(const void *data, unsigned short len, NetGiveTime giveTime) {
    if (!gConnected) return (OSErr)-1;
    return SendData(gStream, (Ptr)data, len, true, (GiveTimePtr)giveTime, &gCancel);
}

/* Non-blocking receive: returns bytes read (0 if none ready), -1 on close/error. */
long NetPoll(void *buf, unsigned short maxLen, NetGiveTime giveTime) {
    OSErr err;
    TCPStatusPB status;
    unsigned short avail, n;

    if (!gConnected) return -1;
    if (maxLen < 2) return 0;   /* need room for RecvData's NUL terminator */

    err = LowTCPStatus((StreamPtr)gStream, &status, (GiveTimePtr)giveTime, &gCancel);
    if (err == connectionClosing || err == connectionTerminated ||
        err == connectionDoesntExist || err == invalidStreamPtr) {
        teardown(giveTime);
        return -1;
    }
    if (err != noErr) return 0;   /* commandTimeout / other transient — nothing this poll */

    avail = status.amtUnreadData;
    if (avail == 0) return 0;

    /* Reserve one byte: RecvData writes buf[n] = '\0' on success, so cap the
       request at maxLen-1 to keep that terminator inside the caller's buffer. */
    {
        unsigned short cap = (unsigned short)(maxLen - 1);
        n = (avail < cap) ? avail : cap;
    }
    err = RecvData(gStream, (Ptr)buf, &n, false, (GiveTimePtr)giveTime, &gCancel);
    if (err == noErr) return (long)n;
    if (err == connectionClosing || err == connectionTerminated) {
        teardown(giveTime);
        return (n > 0) ? (long)n : -1;
    }
    if (err == connectionDoesntExist || err == invalidStreamPtr) {
        teardown(giveTime);
        return -1;
    }
    return 0;   /* commandTimeout / other transient — nothing this poll */
}

void NetClose(NetGiveTime giveTime) {
    if (gConnected) {
        CloseNetConnection(gStream, (GiveTimePtr)giveTime, &gCancel);
        teardown(giveTime);
    }
}
