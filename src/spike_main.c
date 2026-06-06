// ABOUTME: MacTCP connectivity spike — connects to the host echo proxy and verifies round-trip.
// ABOUTME: GUI app (no RetroConsole); shows results in a NoteAlert via ParamText.

#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <Dialogs.h>
#include <TextEdit.h>
#include <Events.h>
#include <Devices.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "netmac.h"

static void GiveTime(void) {
    EventRecord e;
    WaitNextEvent(0, &e, 1, NULL);
}

/* Appends a printf-style line to msg, using '\r' as Mac line separator. */
static char msg[512];
static int msgLen = 0;

static void AppendLine(const char *fmt, ...) {
    char tmp[128];
    int n;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    n = (int)strlen(tmp);
    if (msgLen + n + 1 >= (int)sizeof(msg)) return; /* guard overflow */
    if (msgLen > 0) {
        msg[msgLen++] = '\r';
    }
    memcpy(msg + msgLen, tmp, n);
    msgLen += n;
    msg[msgLen] = '\0';
}

/* Converts a C string to a Pascal string in a caller-supplied Str255. */
static void cstr2pstr(const char *c, Str255 p) {
    size_t n = strlen(c);
    if (n > 255) n = 255;
    p[0] = (unsigned char)n;
    memcpy(p + 1, c, n);
}

int main(void) {
    OSErr err;
    unsigned char hello[5] = { 0x10, 0x00, 0x02, 0x00, 0x01 };

    /* Standard Toolbox init */
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    /* Step 1: Init network */
    err = NetInit();
    AppendLine("NetInit err=%d", (int)err);

    /* Step 2: Connect */
    if (err == noErr) {
        AppendLine("Connecting 10.0.2.2:4242...");
        err = NetConnect("10.0.2.2", 4242, GiveTime);
        AppendLine("NetConnect err=%d", (int)err);
    }

    /* Step 3: Send HELLO frame: type=0x10, length=0x0002, payload=0x0001 */
    if (err == noErr) {
        err = NetSend(hello, 5, GiveTime);
        AppendLine("NetSend err=%d", (int)err);
    }

    /* Step 4: Receive echo via poll loop */
    if (err == noErr) {
        unsigned char buf[64];
        int total = 0, iter = 0;
        while (total < 5 && iter < 60) {
            long n = NetPoll(buf + total, (unsigned short)(sizeof(buf) - total), GiveTime);
            if (n < 0) {
                AppendLine("NetPoll: closed/err");
                break;
            }
            total += (int)n;
            GiveTime();
            iter++;
        }

        /* Format received bytes as hex */
        {
            char hexline[128];
            int pos = 0, i;
            pos += snprintf(hexline + pos, sizeof(hexline) - pos, "Recv %d bytes:", total);
            for (i = 0; i < total && pos < (int)sizeof(hexline) - 4; i++) {
                pos += snprintf(hexline + pos, sizeof(hexline) - pos, " %02X", buf[i]);
            }
            AppendLine("%s", hexline);
        }

        if (total >= 5 && memcmp(buf, hello, 5) == 0) {
            AppendLine("ROUND-TRIP OK");
        } else {
            AppendLine("MISMATCH / NO DATA");
        }
    }

    /* Teardown */
    NetClose(GiveTime);

    /* Show result in a NoteAlert */
    {
        Str255 p;
        cstr2pstr(msg, p);
        ParamText(p, "\p", "\p", "\p");
        NoteAlert(128, NULL);
    }

    return 0;
}
