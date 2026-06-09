import { describe, it, expect } from "vitest";
import { toMacRoman, fromMacRoman } from "../src/macroman";

describe("toMacRoman", () => {
  it("passes ASCII through unchanged", () => {
    expect([...toMacRoman("Hello, world!")]).toEqual([...Buffer.from("Hello, world!", "ascii")]);
  });
  it("maps common typography to Mac Roman code points", () => {
    expect([...toMacRoman("—")]).toEqual([0xd1]); // em dash
    expect([...toMacRoman("•")]).toEqual([0xa5]); // bullet
    expect([...toMacRoman("…")]).toEqual([0xc9]); // ellipsis
    expect([...toMacRoman("“”")]).toEqual([0xd2, 0xd3]);
    expect([...toMacRoman("‘’")]).toEqual([0xd4, 0xd5]);
  });
  it("degrades unmappable characters to '?'", () => {
    expect([...toMacRoman("→")]).toEqual([0x3f]);
  });
  it("degrades an astral (surrogate-pair) char to a single '?' byte with no stray bytes", () => {
    expect([...toMacRoman("a😀b")]).toEqual([0x61, 0x3f, 0x62]);
  });
  it("maps un-encodable characters to '?' (0x3F), never NUL or dropped", () => {
    const out = toMacRoman("a🎉b中c");
    expect(out).not.toContain(0x00);          // no NUL (would truncate the SE C string)
    expect(out.toString("latin1")).toBe("a?b?c");  // each un-mappable char -> '?'
  });
});

describe("fromMacRoman", () => {
  it("round-trips ASCII", () => {
    expect(fromMacRoman(Buffer.from("> fix it", "ascii"))).toBe("> fix it");
  });
  it("maps a Mac Roman bullet back to UTF-8", () => {
    expect(fromMacRoman(Buffer.from([0xa5]))).toBe("•");
  });
  it("covers the full high range (0xFF maps to a non-empty char)", () => {
    expect(typeof fromMacRoman(Buffer.from([0xff]))).toBe("string");
    expect(fromMacRoman(Buffer.from([0xff]))).not.toBe("");
  });
});
