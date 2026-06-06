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
#include "TCPHi.h"

static void GiveTime(void) {
    EventRecord e;
    WaitNextEvent(0, &e, 1, NULL);
}

static unsigned long ParseIP(const char *s) {
    unsigned long a, b, c, d;
    if (sscanf(s, "%lu.%lu.%lu.%lu", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    return (a << 24) | (b << 16) | (c << 8) | d;
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
    unsigned long stream = 0;
    bool cancel = false;

    /* Standard Toolbox init */
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    /* Step 1: Init network */
    err = InitNetwork();
    AppendLine("InitNetwork err=%d", (int)err);

    /* Step 2: Create stream */
    if (err == noErr) {
        err = CreateStream(&stream, 8192, GiveTime, &cancel);
        AppendLine("CreateStream err=%d", (int)err);
    }

    /* Step 3: Open connection */
    if (err == noErr) {
        AppendLine("Connecting 10.0.2.2:4242...");
        err = OpenConnection(stream, (long)ParseIP("10.0.2.2"), 4242, 30, GiveTime, &cancel);
        AppendLine("OpenConnection err=%d", (int)err);
    }

    /* Step 4: Send HELLO frame: type=0x10, length=0x0002, payload=0x0001 */
    if (err == noErr) {
        unsigned char hello[5] = { 0x10, 0x00, 0x02, 0x00, 0x01 };
        err = SendData(stream, (Ptr)hello, 5, true, GiveTime, &cancel);
        AppendLine("SendData err=%d", (int)err);

        /* Step 5: Receive echo — accumulate until we have >=5 bytes or give up */
        if (err == noErr) {
            unsigned char buf[64];
            int total = 0, iter = 0;
            while (total < 5 && iter < 20) {
                unsigned short n = (unsigned short)(sizeof(buf) - total);
                OSErr r = RecvData(stream, (Ptr)(buf + total), &n, false, GiveTime, &cancel);
                if (r == noErr) {
                    total += n;
                } else if (r == connectionClosing || r == connectionTerminated) {
                    if (n > 0) total += n;
                    break;
                }
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
    }

    /* Teardown */
    if (stream) {
        CloseNetConnection(stream, GiveTime, &cancel);
        ReleaseStream(stream, GiveTime, &cancel);
    }

    /* Show result in a NoteAlert */
    {
        Str255 p;
        cstr2pstr(msg, p);
        ParamText(p, "\p", "\p", "\p");
        NoteAlert(128, NULL);
    }

    return 0;
}
