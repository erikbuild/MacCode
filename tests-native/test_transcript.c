/* ABOUTME: Native munit tests for the MacCode transcript ring buffer (transcript.c).
   ABOUTME: Verifies word-wrap, newline splitting, and oldest-line eviction. */
#include "munit.h"
#include "transcript.h"
#include <string.h>
#include <stdio.h>

static MunitResult wrap(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  TrAppend(&t, "aaaa bbbb cccc", TR_ASSISTANT, 9);
  munit_assert_int(TrLiveCount(&t), ==, 2);
  munit_assert_string_equal(TrGet(&t,0)->text, "aaaa bbbb");
  munit_assert_string_equal(TrGet(&t,1)->text, "cccc");
  return MUNIT_OK;
}
static MunitResult newline(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  TrAppend(&t, "one\ntwo", TR_INFO, 80);
  munit_assert_int(TrLiveCount(&t), ==, 2);
  munit_assert_string_equal(TrGet(&t,0)->text, "one");
  munit_assert_string_equal(TrGet(&t,1)->text, "two");
  return MUNIT_OK;
}
static MunitResult longword(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  TrAppend(&t, "abcdefghijür", TR_ASSISTANT, 4);  /* no spaces -> hard break at wrapCols */
  munit_assert_int(TrLiveCount(&t), >=, 3);
  munit_assert_string_equal(TrGet(&t,0)->text, "abcd");
  return MUNIT_OK;
}
static MunitResult ring(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  char line[8];
  int i;
  for (i=0;i<TR_MAX_LINES+5;i++){ snprintf(line,sizeof line,"L%d",i); TrAppend(&t,line,TR_INFO,80); }
  munit_assert_int(TrLiveCount(&t), ==, TR_MAX_LINES);
  munit_assert_string_equal(TrGet(&t,0)->text, "L5");   /* oldest 5 dropped */
  return MUNIT_OK;
}
static MunitResult kind_tag(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  TrAppend(&t, "hi", TR_USER, 80);
  munit_assert_int(TrGet(&t,0)->kind, ==, TR_USER);
  return MUNIT_OK;
}
static MunitResult out_of_range(const MunitParameter p[], void *f){
  (void)p;(void)f;
  Transcript t; TrInit(&t);
  munit_assert_ptr_null((void*)TrGet(&t,0));
  TrAppend(&t,"x",TR_INFO,80);
  munit_assert_ptr_null((void*)TrGet(&t,1));
  munit_assert_ptr_null((void*)TrGet(&t,-1));
  return MUNIT_OK;
}
static MunitTest tests[] = {
  {"/wrap", wrap, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/newline", newline, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/longword", longword, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/ring", ring, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/kind", kind_tag, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/out-of-range", out_of_range, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL,NULL,NULL,NULL,MUNIT_TEST_OPTION_NONE,NULL}
};
static const MunitSuite suite = {"/transcript", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
int main(int argc, char **argv){ return munit_suite_main(&suite, NULL, argc, argv); }
