// ABOUTME: Tests for translate.ts — relay event → wire frame encoding.
// ABOUTME: Verifies Mac Roman byte output, frame types, and helper constructors.
import { describe, it, expect } from "vitest";
import { eventToFrames, toolLine, verbFrame, clearVerbFrame, askFrame, parseClientFrame } from "../src/translate";
import { RT, FrameDecoder } from "../src/protocol";

function decode(bufs: Buffer[]) {
  const d = new FrameDecoder();
  return bufs.flatMap(b => d.push(b));
}

describe("toolLine", () => {
  it("renders a bullet + name + target", () => {
    expect(toolLine("Edit", "main.c")).toBe("• Edit main.c");
  });
  it("omits target when absent", () => {
    expect(toolLine("Bash")).toBe("• Bash");
  });
});

describe("eventToFrames", () => {
  it("encodes a text event as a TEXT frame in Mac Roman", () => {
    const frames = decode([eventToFrames({ kind: "text", text: "café —" })]);
    expect(frames[0].type).toBe(RT.TEXT);
    expect([...frames[0].payload]).toEqual([0x63, 0x61, 0x66, 0x8e, 0x20, 0xd1]); // c a f é(0x8e) ' ' —(0xD1)
  });
  it("encodes done as an empty DONE frame", () => {
    const frames = decode([eventToFrames({ kind: "done" })]);
    expect(frames[0].type).toBe(RT.DONE);
    expect(frames[0].payload.length).toBe(0);
  });
  it("encodes a tool event as a TOOL frame", () => {
    const frames = decode([eventToFrames({ kind: "tool", name: "Read", target: "app.h" })]);
    expect(frames[0].type).toBe(RT.TOOL);
    expect(frames[0].payload.toString("latin1")).toBe("• Read app.h".replace("•", "\xa5"));
  });
  it("encodes an info event as an INFO frame", () => {
    const frames = decode([eventToFrames({ kind: "info", text: "ready" })]);
    expect(frames[0].type).toBe(RT.INFO);
    expect(frames[0].payload.toString()).toBe("ready");
  });
  it("encodes an error event as an ERR frame", () => {
    const frames = decode([eventToFrames({ kind: "error", text: "boom" })]);
    expect(frames[0].type).toBe(RT.ERR);
    expect(frames[0].payload.toString()).toBe("boom");
  });
});

describe("verb and ask frames", () => {
  it("verb frame carries the verb; clear is empty", () => {
    const d = new FrameDecoder();
    expect(d.push(verbFrame("Forging"))[0].payload.toString()).toBe("Forging");
    expect(d.push(clearVerbFrame())[0].payload.length).toBe(0);
  });
  it("ask frame is [id BE][text]", () => {
    const d = new FrameDecoder();
    const f = d.push(askFrame(7, "Run: npm test"))[0];
    expect(f.type).toBe(RT.ASK);
    expect(f.payload.readUInt32BE(0)).toBe(7);
    expect(f.payload.subarray(4).toString()).toBe("Run: npm test");
  });
  it("ask frame round-trips a full 32-bit id", () => {
    const d = new FrameDecoder();
    const f = d.push(askFrame(0xffffffff, "x"))[0];
    expect(f.payload.readUInt32BE(0)).toBe(0xffffffff);
  });
});

describe("parseClientFrame", () => {
  it("parses a PROMPT frame to a prompt action (Mac Roman -> UTF-8)", () => {
    const action = parseClientFrame({ type: RT.PROMPT, payload: Buffer.from([0xa5, 0x20, 0x68, 0x69]) });
    expect(action).toEqual({ kind: "prompt", text: "• hi" }); // 0xa5 -> bullet
  });
  it("parses a PERM allow frame", () => {
    const p = Buffer.alloc(5); p.writeUInt32BE(7, 0); p.writeUInt8(1, 4);
    expect(parseClientFrame({ type: RT.PERM, payload: p })).toEqual({ kind: "perm", id: 7, allow: true });
  });
  it("parses a PERM deny frame", () => {
    const p = Buffer.alloc(5); p.writeUInt32BE(9, 0); p.writeUInt8(0, 4);
    expect(parseClientFrame({ type: RT.PERM, payload: p })).toEqual({ kind: "perm", id: 9, allow: false });
  });
  it("parses STOP / NEW / RESUME / HELLO", () => {
    expect(parseClientFrame({ type: RT.STOP, payload: Buffer.alloc(0) })).toEqual({ kind: "stop" });
    expect(parseClientFrame({ type: RT.NEW, payload: Buffer.alloc(0) })).toEqual({ kind: "new" });
    expect(parseClientFrame({ type: RT.RESUME, payload: Buffer.alloc(0) })).toEqual({ kind: "resume" });
    const h = Buffer.alloc(2); h.writeUInt16BE(1, 0);
    expect(parseClientFrame({ type: RT.HELLO, payload: h })).toEqual({ kind: "hello", version: 1 });
  });
  it("returns unknown for an unrecognized type byte", () => {
    expect(parseClientFrame({ type: 0x7f, payload: Buffer.alloc(0) })).toEqual({ kind: "unknown", type: 0x7f });
  });
  it("throws on a short/malformed payload (caller drops the connection per spec §8)", () => {
    expect(() => parseClientFrame({ type: RT.HELLO, payload: Buffer.alloc(0) })).toThrow();
    expect(() => parseClientFrame({ type: RT.PERM, payload: Buffer.alloc(2) })).toThrow();
  });
});
