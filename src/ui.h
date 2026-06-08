/* ABOUTME: MacCode window UI — transcript drawing, scrollbar, (input/verb in 5.5). */
#ifndef UI_H
#define UI_H
#include <MacTypes.h>

void UI_Init(void);              /* fonts/metrics + create the vertical scrollbar */
void UI_Update(void);            /* full content redraw incl controls (from updateEvt) */
void UI_DrawTranscript(void);    /* draw just the visible transcript lines */
void UI_ContentClick(Point local); /* handle a click in the content (scrollbar tracking) */
void UI_TranscriptChanged(void); /* recompute scroll range after append; autoscroll */
void UI_ScrollToBottom(void);
int  UI_WrapCols(void);          /* content width in monospace columns */

#endif
