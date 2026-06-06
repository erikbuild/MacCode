import { describe, it, expect } from "vitest";
import { RT, encodeFrame, FrameDecoder } from "../src/protocol";

describe("encodeFrame", () => {
  it("frames a TEXT record as [type][len BE][payload]", () => {
    const out = encodeFrame(RT.TEXT, Buffer.from("hi"));
    expect([...out]).toEqual([0x01, 0x00, 0x02, 0x68, 0x69]); // 'h','i'
  });

  it("frames an empty DONE record", () => {
    const out = encodeFrame(RT.DONE, Buffer.alloc(0));
    expect([...out]).toEqual([0x05, 0x00, 0x00]);
  });

  it("rejects payloads larger than 65535", () => {
    expect(() => encodeFrame(RT.TEXT, Buffer.alloc(65536))).toThrow();
  });
});

describe("RT byte values (cross-language contract — keep in sync with SE wire.h)", () => {
  it("matches the canonical record-type bytes", () => {
    expect(RT).toEqual({
      TEXT: 0x01, VERB: 0x02, TOOL: 0x03, ASK: 0x04, DONE: 0x05, INFO: 0x06, ERR: 0x07,
      HELLO: 0x10, PROMPT: 0x11, PERM: 0x12, STOP: 0x13, NEW: 0x14, RESUME: 0x15,
    });
  });
});

describe("FrameDecoder", () => {
  it("decodes a frame split across two chunks", () => {
    const d = new FrameDecoder();
    expect(d.push(Buffer.from([0x11, 0x00]))).toEqual([]);          // partial header
    const frames = d.push(Buffer.from([0x03, 0x61, 0x62, 0x63]));   // len=3 "abc"
    expect(frames).toHaveLength(1);
    expect(frames[0].type).toBe(0x11);
    expect(frames[0].payload.toString()).toBe("abc");
  });

  it("decodes two frames in one chunk", () => {
    const d = new FrameDecoder();
    const buf = Buffer.from([0x05, 0x00, 0x00, 0x13, 0x00, 0x00]); // DONE, STOP
    const frames = d.push(buf);
    expect(frames.map(f => f.type)).toEqual([0x05, 0x13]);
  });

  it("buffers a payload split across chunks", () => {
    const d = new FrameDecoder();
    expect(d.push(Buffer.from([0x01, 0x00, 0x04, 0x61, 0x62]))).toEqual([]); // TEXT len=4, only "ab" so far
    const frames = d.push(Buffer.from([0x63, 0x64]));                        // remaining "cd"
    expect(frames).toHaveLength(1);
    expect(frames[0].type).toBe(0x01);
    expect(frames[0].payload.toString()).toBe("abcd");
  });

  it("returns a complete frame and retains a partial trailing frame", () => {
    const d = new FrameDecoder();
    // full DONE (05 00 00) + start of a TEXT frame (01 00 02 'x'), missing one payload byte
    const first = d.push(Buffer.from([0x05, 0x00, 0x00, 0x01, 0x00, 0x02, 0x78]));
    expect(first.map(f => f.type)).toEqual([0x05]);
    const second = d.push(Buffer.from([0x79])); // 'y' completes payload "xy"
    expect(second).toHaveLength(1);
    expect(second[0].type).toBe(0x01);
    expect(second[0].payload.toString()).toBe("xy");
  });
});
