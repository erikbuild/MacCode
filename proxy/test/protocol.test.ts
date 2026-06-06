import { describe, it, expect } from "vitest";
import { RT, encodeFrame } from "../src/protocol";

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
