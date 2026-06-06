/* ABOUTME: Minimal MacTCP type definitions for building MacTCPHelper.
   ABOUTME: Based on Inside Macintosh: Networking and Apple's MacTCP headers. */

#ifndef __MACTCP__
#define __MACTCP__

#include <Types.h>
#include <MacMemory.h>
#include <Devices.h>

/* Multiversal only provides PBControlSync/PBControlAsync. MacTCPHelper
   uses the older PBControl(pb, async) form — bridge with macros. */
#define PBControl(pb, async) ((async) ? PBControlAsync(pb) : PBControlSync(pb))
#define PBKillIO(pb, async)  PBControlSync(pb)

typedef unsigned long ip_addr;
typedef unsigned short tcp_port;
typedef Ptr StreamPtr;

struct ICMPReport {
    unsigned short streamPtr;
    ip_addr        localHost;
    ip_addr        remoteHost;
};

/* csCode values for TCPiopb */
enum {
    TCPCreate      = 30,
    TCPPassiveOpen = 31,
    TCPActiveOpen  = 32,
    TCPSend        = 34,
    TCPNoCopyRcv   = 35,
    TCPRcvBfrReturn= 36,
    TCPRcv         = 37,
    TCPClose       = 38,
    TCPAbort       = 39,
    TCPStatus      = 40,
    TCPRelease     = 42,
    TCPGlobalInfo  = 43
};

/* MacTCP error codes */
enum {
    connectionClosing    = -23005,
    connectionDoesntExist= -23008,
    connectionTerminated = -23006,
    commandTimeout       = -23016,
    invalidStreamPtr     = -23000,
    streamAlreadyOpen    = -23001,
    openFailed           = -23004,
    duplicateSocket      = -23010
};

typedef pascal void (*TCPNotifyProcPtr)(
    StreamPtr tcpStream,
    unsigned short eventCode,
    Ptr userDataPtr,
    unsigned short terminReason,
    struct ICMPReport *icmpMsg
);

/* Write Data Structure entry */
typedef struct wdsEntry {
    unsigned short length;
    Ptr            ptr;
} wdsEntry;

/* Read Data Structure entry */
typedef struct rdsEntry {
    unsigned short length;
    Ptr            ptr;
} rdsEntry;

/* TCP parameter block sub-structures */

typedef struct TCPCreatePB {
    Ptr             rcvBuff;
    unsigned long   rcvBuffLen;
    TCPNotifyProcPtr notifyProc;
    Ptr             userDataPtr;
} TCPCreatePB;

typedef struct TCPOpenPB {
    unsigned char   ulpTimeoutValue;
    unsigned char   ulpTimeoutAction;
    unsigned char   validityFlags;
    unsigned char   commandTimeoutValue;
    ip_addr         remoteHost;
    tcp_port        remotePort;
    ip_addr         localHost;
    tcp_port        localPort;
    unsigned char   tosFlags;
    unsigned char   precedence;
    unsigned char   dontFrag;
    unsigned char   timeToLive;
    unsigned char   security;
    unsigned char   optionCnt;
    unsigned char   options[40];
} TCPOpenPB;

typedef struct TCPSendPB {
    unsigned char   ulpTimeoutValue;
    unsigned char   ulpTimeoutAction;
    unsigned char   validityFlags;
    Boolean         pushFlag;
    Boolean         urgentFlag;
    Ptr             wdsPtr;
    unsigned short  sendFree;
    unsigned short  sendLength;
} TCPSendPB;

typedef struct TCPReceivePB {
    unsigned char   commandTimeoutValue;
    Boolean         markFlag;
    Boolean         urgentFlag;
    Ptr             rcvBuff;
    unsigned short  rcvBuffLen;
    Ptr             rdsPtr;
    unsigned short  rdsLength;
    unsigned short  secondTimeStamp;
} TCPReceivePB;

typedef struct TCPClosePB {
    unsigned char   ulpTimeoutValue;
    unsigned char   ulpTimeoutAction;
    unsigned char   validityFlags;
} TCPClosePB;

typedef struct TCPStatusPB {
    unsigned char   ulpTimeoutValue;
    unsigned char   ulpTimeoutAction;
    long            unused;
    ip_addr         remoteHost;
    tcp_port        remotePort;
    ip_addr         localHost;
    tcp_port        localPort;
    unsigned char   tosFlags;
    unsigned char   precedence;
    unsigned char   connectionState;
    unsigned short  sendWindow;
    unsigned short  rcvWindow;
    unsigned short  amtUnackedData;
    unsigned short  amtUnreadData;
    Ptr             securityLevelPtr;
    unsigned long   sendUnacked;
    unsigned long   sendNext;
    unsigned long   congestionWindow;
    unsigned long   rcvNext;
    unsigned long   srtt;
    unsigned long   lastRtt;
    unsigned long   sendMaxSegSize;
} TCPStatusPB;

typedef struct TCPGlobalInfoPB {
    Ptr             tcpParamPtr;
    Ptr             tcpStatsPtr;
    Ptr             tcpCDBTable;
    Ptr             userDataPtr;
    unsigned short  maxTCPConnections;
} TCPGlobalInfoPB;

/* The main TCP I/O parameter block */
typedef struct TCPiopb {
    char            fill12[12];
    ProcPtr         ioCompletion;
    OSErr           ioResult;
    char            ioNamePtr[4];
    short           ioVRefNum;
    short           ioCRefNum;
    short           csCode;
    StreamPtr       tcpStream;
    union {
        TCPCreatePB      create;
        TCPOpenPB        open;
        TCPSendPB        send;
        TCPReceivePB     receive;
        TCPClosePB       close;
        TCPStatusPB      status;
        TCPGlobalInfoPB  globalInfo;
    } csParam;
} TCPiopb;

#endif /* __MACTCP__ */
