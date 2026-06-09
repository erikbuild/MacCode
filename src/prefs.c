/* ABOUTME: Preferences-folder persistence for the proxy server IP+port.
   ABOUTME: Stores "ip:port" as a small text file; classic File Manager (FSSpec) I/O. */
#include <Multiverse.h>
#include <string.h>
#include <stdio.h>      /* sscanf / sprintf */
#include "app.h"
#include "prefs.h"

#define PREFS_NAME    "\pMacCode Prefs"
#define PREFS_CREATOR 'MCde'
#define PREFS_TYPE    'pref'

/* kOnSystemDisk (-32768) and kCreateFolder (true) are not in these headers. */
#define kOnSystemDisk    ((short)-32768)

/* FSSpec for the prefs file in the (created-if-needed) Preferences folder.
   FSMakeFSSpec returns fnfErr when the file doesn't exist yet (first run), but it
   still fills in a valid spec we can hand to FSpCreate — so treat fnfErr as success. */
static OSErr PrefsSpec(FSSpec *spec) {
    short vRefNum; long dirID; OSErr err;
    err = FindFolder(kOnSystemDisk, kPreferencesFolderType, true, &vRefNum, &dirID);
    if (err != noErr) return err;
    err = FSMakeFSSpec(vRefNum, dirID, PREFS_NAME, spec);
    if (err == noErr || err == fnfErr) return noErr;
    return err;
}

void PrefsLoad(void) {
    FSSpec spec; short refNum; long count; OSErr err; char buf[48];
    unsigned long a, b, c, d, port; char *colon;
    if (PrefsSpec(&spec) != noErr) return;                  /* keep defaults */
    if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr) return; /* no file -> defaults */
    count = (long)(sizeof(buf) - 1);
    err = FSRead(refNum, &count, buf);                       /* eofErr is fine — bytes still read */
    FSClose(refNum);
    if ((err != noErr && err != eofErr) || count <= 0) return;
    buf[count] = '\0';
    if (sscanf(buf, "%lu.%lu.%lu.%lu:%lu", &a, &b, &c, &d, &port) == 5 &&
        a < 256 && b < 256 && c < 256 && d < 256 && port > 0 && port < 65536) {
        colon = strchr(buf, ':');
        if (colon) *colon = '\0';                            /* keep just the IP text */
        strncpy(gApp.serverIP, buf, sizeof(gApp.serverIP) - 1);
        gApp.serverIP[sizeof(gApp.serverIP) - 1] = '\0';
        gApp.serverPort = (unsigned short)port;
    }
}

void PrefsSave(void) {
    FSSpec spec; short refNum; long count; char buf[48];
    if (PrefsSpec(&spec) != noErr) return;
    FSpCreate(&spec, PREFS_CREATOR, PREFS_TYPE, smSystemScript);  /* dupFNErr is fine */
    if (FSpOpenDF(&spec, fsWrPerm, &refNum) != noErr) return;
    SetFPos(refNum, fsFromStart, 0);
    SetEOF(refNum, 0);
    sprintf(buf, "%s:%u", gApp.serverIP, (unsigned)gApp.serverPort);
    count = (long)strlen(buf);
    FSWrite(refNum, &count, buf);
    FSClose(refNum);
}
