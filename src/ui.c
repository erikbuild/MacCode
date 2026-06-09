/* ABOUTME: MacCode window UI — transcript drawing + vertical scrollbar over the Transcript model.
   ABOUTME: Monaco 9 monospaced; column-count wrap matches pixel width. */
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Multiverse.h>
#include <string.h>
#include "app.h"
#include "transcript.h"
#include "ui.h"

#define SCROLLBAR_W    15
#define INPUT_H        40   /* bottom strip reserved for input + verb (filled in 5.5) */
#define LMARGIN         3
#define kPermAlertID   129
#define kPermDenyItem  1
#define kPermAllowItem 2

static ControlHandle  gVScroll = NULL;
static short          gFontNum;     /* Monaco */
static short          gLineH = 12;
static short          gCharW = 6;
static short          gVisRows = 1;
static ControlActionUPP gScrollUPP = NULL;
static TEHandle       gTE = NULL;
static Boolean        gInputEnabled = true;
static short          gSparklePhase = 0;
static Boolean        gDark = false;

static Rect ContentRect(void) {
    Rect r = gApp.win->portRect;
    r.right  -= SCROLLBAR_W;
    r.bottom -= INPUT_H;
    return r;
}

/* top 14px of the bottom strip — verb/sparkle line */
static Rect VerbRect(void) {
    Rect r = gApp.win->portRect;
    r.top = r.bottom - 40;
    r.bottom = r.top + 14;
    return r;
}

/* lower part of the bottom strip — framed text-entry box */
static Rect InputBox(void) {
    Rect r = gApp.win->portRect;
    r.top    = r.bottom - 24;
    r.bottom -= 2;
    r.left   += LMARGIN;
    r.right  -= LMARGIN;
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
    UI_InitInput();
}

/* Sets fore/back so EraseRect paints the background and text/lines paint legibly.
   On the 1-bit SE under Color QuickDraw, white/black map to the two pixel values. */
static void ApplyColors(void) {
    if (gDark) { BackColor(blackColor); ForeColor(whiteColor); }
    else       { BackColor(whiteColor); ForeColor(blackColor); }
}

void UI_DrawTranscript(void) {
    Rect cr = ContentRect();
    short i, y;
    ApplyColors();
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
    Rect full = gApp.win->portRect;
    RecomputeMetrics();
    ApplyColors();
    EraseRect(&full);          /* paint the whole content (incl. gutter) in the bg color */
    DrawControls(gApp.win);
    UI_DrawTranscript();
    UI_DrawInputAndVerb();
}

void UI_ScrollToBottom(void) {
    short mx;
    ScrollMax(&mx);
    gApp.scrollTop = mx;
    if (gVScroll) SetControlValue(gVScroll, mx);
}

void UI_TranscriptChanged(void) {
    short oldMax, newMax;
    Rect cr = ContentRect();
    Boolean atBottom;
    ScrollMax(&oldMax);
    atBottom = (gApp.scrollTop >= oldMax);   /* were we pinned to the latest line? */
    RecomputeMetrics();
    ScrollMax(&newMax);
    if (gVScroll) SetControlMaximum(gVScroll, newMax);
    if (atBottom) UI_ScrollToBottom();        /* follow the tail only if we were already there */
    else if (gApp.scrollTop > newMax) {       /* keep position, but clamp if the max shrank */
        gApp.scrollTop = newMax;
        if (gVScroll) SetControlValue(gVScroll, newMax);
    }
    InvalRect(&cr);
}

void UI_ContentClick(Point local) {
    ControlHandle ctl;
    Rect boxRect = InputBox();
    INTEGER part = FindControl(local, gApp.win, &ctl);
    if (ctl == gVScroll && part != 0) {
        if (part == inThumb) {
            TrackControl(ctl, local, NULL);
            gApp.scrollTop = GetControlValue(ctl);
            UI_DrawTranscript();
        } else {
            TrackControl(ctl, local, gScrollUPP);
        }
    } else if (PtInRect(local, &boxRect) && gTE) {
        TEClick(local, false, gTE);
    }
}

void UI_InitInput(void) {
    Rect box = InputBox();
    Rect dest = box;
    InsetRect(&dest, 2, 2);
    TextFont(gFontNum); TextSize(9); TextFace(normal);
    gTE = TENew(&dest, &dest);
    TEActivate(gTE);
    gInputEnabled = true;
}

/* draws a small ✻ sparkle as four crossed strokes; the phase omits one stroke for a twinkle */
static void DrawSparkle(short cx, short cy) {
    short p = gSparklePhase & 3;
    if (p != 0) { MoveTo(cx,   cy-3); LineTo(cx,   cy+3); }   /* | */
    if (p != 1) { MoveTo(cx-3, cy);   LineTo(cx+3, cy);   }   /* - */
    if (p != 2) { MoveTo(cx-2, cy-2); LineTo(cx+2, cy+2); }   /* \ */
    if (p != 3) { MoveTo(cx-2, cy+2); LineTo(cx+2, cy-2); }   /* / */
}

void UI_DrawInputAndVerb(void) {
    Rect v   = VerbRect();
    Rect box = InputBox();
    ApplyColors();
    /* separator line above the bottom strip */
    MoveTo(0, v.top - 1);
    LineTo(gApp.win->portRect.right, v.top - 1);
    EraseRect(&v);
    if (gApp.verb[0]) {
        short cx = 8, cy = v.top + 7;
        DrawSparkle(cx, cy);
        TextFont(gFontNum); TextSize(9); TextFace(italic);
        MoveTo(cx + 8, v.top + 10);
        DrawText(gApp.verb, 0, (short)strlen(gApp.verb));
        if (gApp.state == ST_AWAITING_RESPONSE) {
            TextFace(normal);
            DrawText("   (esc to stop)", 0, 16);
        }
        TextFace(normal);
    }
    FrameRect(&box);
    if (gTE) {
        Rect viewRect = (*gTE)->viewRect;
        TEUpdate(&viewRect, gTE);
    }
}

Boolean UI_InputKey(EventRecord *ev) {
    char c = (char)(ev->message & charCodeMask);
    if (!gInputEnabled || !gTE) return false;
    if (c == '\r' || c == 0x03) {              /* Return / Enter */
        if (ev->modifiers & shiftKey) {
            TEKey('\r', gTE);                  /* shift-Return = newline in box */
            return false;
        }
        return true;                           /* signal: send */
    }
    TEKey(c, gTE);
    return false;
}

void UI_GetInput(char *buf, short max) {
    short len = (*gTE)->teLength;
    CharsHandle h = TEGetText(gTE);
    if (len > max - 1) len = max - 1;
    if (len > 0) BlockMoveData(*h, buf, len);
    buf[len] = '\0';
}

void UI_ClearInput(void) {
    if (gTE) {
        TESetText("", 0, gTE);
        InvalRect(&(*gTE)->viewRect);
    }
}

void UI_SetVerb(const char *verb) {
    short i = 0;
    while (verb[i] && i < 63) { gApp.verb[i] = verb[i]; i++; }
    gApp.verb[i] = '\0';
    { Rect v = VerbRect(); InvalRect(&v); }
}

void UI_SetInputEnabled(Boolean on) {
    gInputEnabled = on;
    if (gTE) {
        if (on) TEActivate(gTE);
        else    TEDeactivate(gTE);
    }
}

void UI_ToggleDark(void) {
    gDark = !gDark;
    InvalRect(&gApp.win->portRect);   /* force a full redraw in the new colors */
}
Boolean UI_IsDark(void) { return gDark; }

void UI_Idle(void) {
    if (gTE && gInputEnabled) TEIdle(gTE);
    if (gApp.verb[0] && (gApp.state == ST_AWAITING_RESPONSE || gApp.state == ST_CONNECTING)) {
        static unsigned long nextTick = 0;
        if ((long)(TickCount() - nextTick) >= 0) {
            nextTick = TickCount() + 15;     /* 15 ticks ≈ 0.25s → ~4 fps */
            gSparklePhase++;
            { Rect v = VerbRect(); InvalRect(&v); }
        }
    }
}

Boolean UI_ShowPermission(const char *desc, short len) {
    Str255 p;
    short  n = len;
    short  hit;
    if (n < 0)   n = 0;
    if (n > 255) n = 255;          /* Str255 holds at most 255 bytes */
    p[0] = (unsigned char)n;
    if (n > 0) memcpy(p + 1, desc, (size_t)n);   /* copy NOW — payload is transient */
    ParamText(p, "\p", "\p", "\p");
    hit = CautionAlert(kPermAlertID, NULL);
    return (Boolean)(hit == kPermAllowItem);
}
