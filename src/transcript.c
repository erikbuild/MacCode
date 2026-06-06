/* ABOUTME: MacCode transcript model — bounded ring of wrapped display lines.
   ABOUTME: Greedy word-wrap at wrapCols; oldest lines drop when full. */
#include "transcript.h"
#include <string.h>

void TrInit(Transcript *t){ t->count=0; t->head=0; t->live=0; }

static TrLine *pushLine(Transcript *t){
  int idx;
  if (t->live < TR_MAX_LINES){ idx = (t->head + t->live) % TR_MAX_LINES; t->live++; }
  else { idx = t->head; t->head = (t->head + 1) % TR_MAX_LINES; }
  t->count++;
  return &t->lines[idx];
}

static void emit(Transcript *t, const char *s, int n, TrKind kind){
  TrLine *ln;
  if (n >= TR_LINE_CAP) n = TR_LINE_CAP - 1;
  ln = pushLine(t);
  memcpy(ln->text, s, n); ln->text[n] = '\0'; ln->kind = kind;
}

void TrAppend(Transcript *t, const char *text, TrKind kind, int wrapCols){
  const char *p = text;
  if (wrapCols > TR_LINE_CAP - 1) wrapCols = TR_LINE_CAP - 1;
  if (wrapCols < 1) wrapCols = 1;
  while (*p){
    const char *nl = strchr(p, '\n');
    int seg = nl ? (int)(nl - p) : (int)strlen(p);
    int start = 0;
    while (start < seg){
      int remain = seg - start;
      int brk, i;
      if (remain <= wrapCols){ emit(t, p+start, remain, kind); break; }
      brk = -1;
      for (i = wrapCols; i > 0; i--){ if (p[start+i] == ' '){ brk = i; break; } }
      if (brk <= 0) brk = wrapCols;          /* hard-break a long word */
      emit(t, p+start, brk, kind);
      start += brk;
      while (start < seg && p[start] == ' ') start++;  /* skip the run of break spaces */
    }
    if (seg == 0 && nl) emit(t, "", 0, kind);   /* blank line from a bare newline */
    p += seg;
    if (nl) p++;                                 /* skip the '\n' */
  }
}

int TrLiveCount(const Transcript *t){ return t->live; }

const TrLine *TrGet(const Transcript *t, int i){
  if (i < 0 || i >= t->live) return 0;
  return &t->lines[(t->head + i) % TR_MAX_LINES];
}
