/* ABOUTME: MacCode window UI — transcript drawing, scrollbar, input box, verb line. */
#ifndef UI_H
#define UI_H
#include <MacTypes.h>
#include <Events.h>

void    UI_Init(void);               /* fonts/metrics + create the vertical scrollbar */
void    UI_Update(void);             /* full content redraw incl controls (from updateEvt) */
void    UI_DrawTranscript(void);     /* draw just the visible transcript lines */
void    UI_ContentClick(Point local); /* handle a click in the content (scrollbar + input) */
void    UI_TranscriptChanged(void);  /* recompute scroll range after append; autoscroll */
void    UI_ScrollToBottom(void);
int     UI_WrapCols(void);           /* content width in monospace columns */

void    UI_InitInput(void);
void    UI_DrawInputAndVerb(void);
Boolean UI_InputKey(EventRecord *ev);   /* true = Return pressed (caller should send) */
void    UI_GetInput(char *buf, short max);
void    UI_ClearInput(void);
void    UI_SetVerb(const char *verb);   /* "" clears */
void    UI_SetInputEnabled(Boolean on);
void    UI_Idle(void);                  /* TEIdle for the caret */
void    UI_ToggleDark(void);
Boolean UI_IsDark(void);

/* Modal tool-permission prompt. desc = Mac Roman bytes (len). Returns true=Allow, false=Deny. */
Boolean UI_ShowPermission(const char *desc, short len);

/* Modal dialog to edit the proxy server IP and port; saves prefs on OK. */
void UI_ShowSettings(void);

#endif
