// ABOUTME: Typed-record protocol constants and frame encoder for the SE TCP wire format.
// ABOUTME: Frame layout: [type:1][length:2 big-endian][payload:length bytes].

// Record type bytes. Keep byte-identical with src/wire.h on the SE.
export const RT = {
  // proxy -> SE
  TEXT: 0x01, VERB: 0x02, TOOL: 0x03, ASK: 0x04, DONE: 0x05, INFO: 0x06, ERR: 0x07,
  // SE -> proxy
  HELLO: 0x10, PROMPT: 0x11, PERM: 0x12, STOP: 0x13, NEW: 0x14, RESUME: 0x15,
} as const;

export const PROTOCOL_VERSION = 1;

export function encodeFrame(type: number, payload: Buffer): Buffer {
  if (payload.length > 0xffff) throw new Error(`payload too large: ${payload.length}`);
  const head = Buffer.alloc(3);
  head.writeUInt8(type, 0);
  head.writeUInt16BE(payload.length, 1);
  return Buffer.concat([head, payload]);
}
