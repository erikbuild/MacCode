/* ABOUTME: MacCode window UI — transcript drawing + vertical scrollbar over the Transcript model.
   ABOUTME: Monaco 9 monospaced; column-count wrap matches pixel width. */
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Multiverse.h>
#include <string.h>
#include "app.h"
#include "transcript.h"
#include "ui.h"

#define SCROLLBAR_W 15
#define INPUT_H     40   /* bottom strip reserved for input + verb (filled in 5.5) */
#define LMARGIN      3

static ControlHandle  gVScroll = NULL;
static short          gFontNum;     /* Monaco */
static short          gLineH = 12;
static short          gCharW = 6;
static short          gVisRows = 1;
static ControlActionUPP gScrollUPP = NULL;

static Rect ContentRect(void) {
    Rect r = gApp.win->portRect;
    r.right  -= SCROLLBAR_W;
    r.bottom -= INPUT_H;
    return r;
}

static void RecomputeMetrics(void) {
    FontInfo fi;
    Rect cr = ContentRect();
    TextFont(gFontNum); TextSize(9); TextFace(normal);
    GetFontInfo(&fi);
    gLineH = fi.ascent + fi.descent + fi.leading;
    if (gLineH < 1) gLineH = 12;
    gCharW = CharWidth('0');
    if (gCharW < 1) gCharW = 6;
    gVisRows = (cr.bottom - cr.top) / gLineH;
    if (gVisRows < 1) gVisRows = 1;
}

int UI_WrapCols(void) {
    Rect cr = ContentRect();
    int cols = (cr.right - cr.left - LMARGIN) / gCharW;
    if (cols < 8) cols = 8;
    return cols;
}

static void ScrollMax(short *outMax) {
    int live = TrLiveCount(gApp.transcript);
    int mx = live - gVisRows;
    if (mx < 0) mx = 0;
    *outMax = (short)mx;
}

static pascal void ScrollAction(ControlHandle c, int16_t part) {
    short v = GetControlValue(c), mx = GetControlMaximum(c), d = 0;
    switch (part) {
        case inUpButton:   d = -1; break;
        case inDownButton: d = +1; break;
        case inPageUp:     d = -(gVisRows - 1); break;
        case inPageDown:   d = +(gVisRows - 1); break;
        default: return;
    }
    v += d;
    if (v < 0) v = 0;
    if (v > mx) v = mx;
    if (v != GetControlValue(c)) {
        SetControlValue(c, v);
        gApp.scrollTop = v;
        UI_DrawTranscript();
    }
}

void UI_Init(void) {
    Rect sb;
    GetFNum("\pMonaco", &gFontNum);
    if (gFontNum == 0) gFontNum = 4; /* monaco */
    RecomputeMetrics();
    sb = gApp.win->portRect;
    SetRect(&sb, sb.right - SCROLLBAR_W, -1, sb.right + 1, sb.bottom - INPUT_H + 1);
    gVScroll = NewControl(gApp.win, &sb, "\p", true, 0, 0, 0, scrollBarProc, 0L);
    gScrollUPP = NewControlActionUPP(ScrollAction);
    gApp.scrollTop = 0;
}

void UI_DrawTranscript(void) {
    Rect cr = ContentRect();
    short i, y;
    TextFont(gFontNum); TextSize(9);
    EraseRect(&cr);
    for (i = 0; i < gVisRows; i++) {
        const TrLine *ln = TrGet(gApp.transcript, gApp.scrollTop + i);
        if (!ln) break;
        switch (ln->kind) {
            case TR_USER: TextFace(bold); break;
            case TR_INFO: TextFace(italic); break;
            case TR_ERR:  TextFace(bold); break;
            default:      TextFace(normal); break;
        }
        y = cr.top + i * gLineH + (gLineH - 3);   /* baseline */
        MoveTo(cr.left + LMARGIN, y);
        DrawText((Ptr)ln->text, 0, (short)strlen(ln->text));
    }
    TextFace(normal);
}

void UI_Update(void) {
    RecomputeMetrics();
    DrawControls(gApp.win);
    UI_DrawTranscript();
}

void UI_ScrollToBottom(void) {
    short mx;
    ScrollMax(&mx);
    gApp.scrollTop = mx;
    if (gVScroll) SetControlValue(gVScroll, mx);
}

void UI_TranscriptChanged(void) {
    short mx;
    Rect cr = ContentRect();
    RecomputeMetrics();
    ScrollMax(&mx);
    if (gVScroll) SetControlMaximum(gVScroll, mx);
    UI_ScrollToBottom();
    InvalRect(&cr);   /* request redraw */
}

void UI_ContentClick(Point local) {
    ControlHandle ctl;
    INTEGER part = FindControl(local, gApp.win, &ctl);
    if (ctl == gVScroll && part != 0) {
        if (part == inThumb) {
            TrackControl(ctl, local, NULL);
            gApp.scrollTop = GetControlValue(ctl);
            UI_DrawTranscript();
        } else {
            TrackControl(ctl, local, gScrollUPP);
        }
    }
}
