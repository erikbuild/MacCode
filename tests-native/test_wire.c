/* ABOUTME: Native munit tests for the MacCode SE wire codec (wire.c).
   ABOUTME: Runs off-device on the host; mirrors the proxy protocol tests. */
#include "munit.h"
#include "wire.h"
#include <string.h>

static MunitResult enc(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  unsigned char out[16];
  long n = WireEncode(RT_TEXT, (const unsigned char*)"hi", 2, out);
  munit_assert_long(n, ==, 5);
  unsigned char want[] = {0x01,0x00,0x02,'h','i'};
  munit_assert_memory_equal(5, out, want);
  return MUNIT_OK;
}
static MunitResult dec_split(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  unsigned char a[] = {0x11,0x00};
  munit_assert_int(WireDecoderPush(&d,a,2,&fr), ==, 0);
  unsigned char b[] = {0x03,'a','b','c'};
  munit_assert_int(WireDecoderPush(&d,b,4,&fr), ==, 1);
  munit_assert_uint8(fr.type, ==, 0x11);
  munit_assert_int(fr.len, ==, 3);
  munit_assert_memory_equal(3, fr.payload, "abc");
  return MUNIT_OK;
}
static MunitResult dec_two(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  unsigned char b[] = {0x05,0x00,0x00, 0x13,0x00,0x00};
  munit_assert_int(WireDecoderPush(&d,b,6,&fr), ==, 1);
  munit_assert_uint8(fr.type, ==, 0x05);
  munit_assert_int(WireDecoderPush(&d,NULL,0,&fr), ==, 1);
  munit_assert_uint8(fr.type, ==, 0x13);
  return MUNIT_OK;
}
static MunitResult dec_two_payloads(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame f1, f2;
  /* Two frames packed into ONE push: TEXT "AAA" then TEXT "BBBBBB". The first frame's
     payload must stay valid after the push even though the second is buffered behind it
     (regression for the early-shift bug that clobbered f1.payload with the next frame). */
  unsigned char buf[] = {0x01,0x00,0x03,'A','A','A',  0x01,0x00,0x06,'B','B','B','B','B','B'};
  munit_assert_int(WireDecoderPush(&d, buf, (unsigned short)sizeof(buf), &f1), ==, 1);
  munit_assert_int(f1.len, ==, 3);
  munit_assert_memory_equal(3, f1.payload, "AAA");        /* was "BBB" with the bug */
  munit_assert_int(WireDecoderPush(&d, NULL, 0, &f2), ==, 1);
  munit_assert_int(f2.len, ==, 6);
  munit_assert_memory_equal(6, f2.payload, "BBBBBB");
  munit_assert_int(WireDecoderPush(&d, NULL, 0, &f1), ==, 0);   /* fully drained */
  return MUNIT_OK;
}
static MunitResult dec_payload_split(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  unsigned char h[] = {0x01,0x00,0x04,'a','b'};   /* len 4, only 2 payload bytes */
  munit_assert_int(WireDecoderPush(&d,h,5,&fr), ==, 0);
  unsigned char rest[] = {'c','d'};
  munit_assert_int(WireDecoderPush(&d,rest,2,&fr), ==, 1);
  munit_assert_int(fr.len, ==, 4);
  munit_assert_memory_equal(4, fr.payload, "abcd");
  return MUNIT_OK;
}
static MunitResult dec_too_big(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  /* declared length 0xFFFF can never fit the buffer -> protocol error (-1) */
  unsigned char h[] = {0x01,0xFF,0xFF};
  munit_assert_int(WireDecoderPush(&d,h,3,&fr), ==, -1);
  return MUNIT_OK;
}
static MunitResult dec_over_room(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  static unsigned char big[5000];
  big[0]=RT_TEXT; big[1]=0x00; big[2]=0x02;
  /* 5000 > WIRE_BUF_SIZE room -> refuse with -1, no silent drop */
  munit_assert_int(WireDecoderPush(&d, big, (unsigned short)sizeof(big), &fr), ==, -1);
  return MUNIT_OK;
}
static MunitResult dec_max_frame_ok(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  static unsigned char buf[WIRE_BUF_SIZE];      /* total == WIRE_BUF_SIZE, payload 4093 */
  unsigned short plen = (unsigned short)(WIRE_BUF_SIZE - 3);
  buf[0]=RT_TEXT; buf[1]=(unsigned char)(plen>>8); buf[2]=(unsigned char)(plen&0xff);
  munit_assert_int(WireDecoderPush(&d, buf, (unsigned short)sizeof(buf), &fr), ==, 1);
  munit_assert_int(fr.len, ==, plen);
  return MUNIT_OK;
}
static MunitResult dec_frame_too_big_by_one(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  unsigned short plen = (unsigned short)(WIRE_BUF_SIZE - 2);   /* total = BUF+1 -> can never fit */
  unsigned char h[3]; h[0]=RT_TEXT; h[1]=(unsigned char)(plen>>8); h[2]=(unsigned char)(plen&0xff);
  munit_assert_int(WireDecoderPush(&d, h, 3, &fr), ==, -1);
  return MUNIT_OK;
}
static MunitResult dec_reinit_after_error(const MunitParameter p[], void *f) {
  (void)p;(void)f;
  WireDecoder d; WireDecoderInit(&d); WireFrame fr;
  unsigned char h[3]={RT_TEXT,0xFF,0xFF};
  munit_assert_int(WireDecoderPush(&d, h, 3, &fr), ==, -1);
  WireDecoderInit(&d);                       /* reset and reuse */
  unsigned char ok[]={0x05,0x00,0x00};
  munit_assert_int(WireDecoderPush(&d, ok, 3, &fr), ==, 1);
  munit_assert_uint8(fr.type, ==, 0x05);
  return MUNIT_OK;
}
static MunitTest tests[] = {
  {"/encode", enc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-split", dec_split, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-two", dec_two, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-two-payloads", dec_two_payloads, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-payload-split", dec_payload_split, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-too-big", dec_too_big, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-over-room", dec_over_room, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-max-frame-ok", dec_max_frame_ok, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-frame-too-big-by-one", dec_frame_too_big_by_one, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/decode-reinit-after-error", dec_reinit_after_error, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL,NULL,NULL,NULL,MUNIT_TEST_OPTION_NONE,NULL}
};
static const MunitSuite suite = {"/wire", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
int main(int argc, char **argv){ return munit_suite_main(&suite, NULL, argc, argv); }
