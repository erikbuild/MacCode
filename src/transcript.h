/* ABOUTME: MacCode transcript model — bounded ring of wrapped display lines.
   ABOUTME: Pure C; the UI draws the visible window of lines. */
#ifndef TRANSCRIPT_H
#define TRANSCRIPT_H

#define TR_MAX_LINES 400
#define TR_LINE_CAP  128   /* chars per wrapped line incl NUL */

typedef enum { TR_USER, TR_ASSISTANT, TR_TOOL, TR_INFO, TR_ERR } TrKind;

typedef struct { char text[TR_LINE_CAP]; TrKind kind; } TrLine;

typedef struct {
  TrLine lines[TR_MAX_LINES];
  int count;   /* total lines ever appended */
  int head;    /* index of oldest live line */
  int live;    /* number of live lines (<= TR_MAX_LINES) */
} Transcript;

void TrInit(Transcript *t);
/* Append text (may contain '\n'); greedy word-wrap at wrapCols; tag each produced line with kind. */
void TrAppend(Transcript *t, const char *text, TrKind kind, int wrapCols);
int  TrLiveCount(const Transcript *t);
/* Get the i-th live line (0 = oldest). Returns NULL if out of range. */
const TrLine *TrGet(const Transcript *t, int i);

#endif
