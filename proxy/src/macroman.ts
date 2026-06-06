// ABOUTME: UTF-8 <-> Mac Roman transcoding for the MacCode proxy.
// ABOUTME: The SE renders Mac Roman; outbound text degrades unmappable chars to '?'.

// Mac Roman high range (0x80–0xFF) as Unicode code points, index 0 == 0x80.
const HIGH = "ÄÅÇÉÑÖÜáàâäãåçéèêëíìîïñóòôöõúùûü†°¢£§•¶ß®©™´¨≠ÆØ∞±≤≥¥µ∂∑∏π∫ªºΩæø¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛ˇ";

const toMap = new Map<string, number>();
for (let i = 0; i < HIGH.length; i++) toMap.set(HIGH[i], 0x80 + i);

export function toMacRoman(s: string): Buffer {
  const out = Buffer.alloc([...s].length);
  let i = 0;
  for (const ch of s) {
    const cp = ch.codePointAt(0)!;
    if (cp < 0x80) { out[i++] = cp; continue; }
    const mac = toMap.get(ch);
    out[i++] = mac !== undefined ? mac : 0x3f; // '?'
  }
  return out.subarray(0, i);
}

export function fromMacRoman(buf: Buffer): string {
  let s = "";
  for (const b of buf) s += b < 0x80 ? String.fromCharCode(b) : HIGH[b - 0x80];
  return s;
}
