// ABOUTME: Converts relay events to encoded wire frames for the SE TCP connection.
// ABOUTME: All text payloads are transcoded to Mac Roman before framing.
import { RT, encodeFrame } from "./protocol";
import type { Frame } from "./protocol";
import { toMacRoman, fromMacRoman } from "./macroman";
import type { RelayEvent } from "./events";

export function toolLine(name: string, target?: string): string {
  return target ? `• ${name} ${target}` : `• ${name}`;
}

export function eventToFrames(ev: RelayEvent): Buffer {
  switch (ev.kind) {
    case "text":  return encodeFrame(RT.TEXT, toMacRoman(ev.text));
    case "tool":  return encodeFrame(RT.TOOL, toMacRoman(toolLine(ev.name, ev.target)));
    case "done":  return encodeFrame(RT.DONE, Buffer.alloc(0));
    case "info":  return encodeFrame(RT.INFO, toMacRoman(ev.text));
    case "error": return encodeFrame(RT.ERR, toMacRoman(ev.text));
  }
}

export function verbFrame(verb: string): Buffer {
  return encodeFrame(RT.VERB, toMacRoman(verb));
}

export function clearVerbFrame(): Buffer {
  return encodeFrame(RT.VERB, Buffer.alloc(0));
}

export function askFrame(id: number, description: string): Buffer {
  const head = Buffer.alloc(4);
  head.writeUInt32BE(id >>> 0, 0);
  return encodeFrame(RT.ASK, Buffer.concat([head, toMacRoman(description)]));
}

export function infoFrame(text: string): Buffer {
  return encodeFrame(RT.INFO, toMacRoman(text));
}

export type ClientAction =
  | { kind: "hello"; version: number }
  | { kind: "prompt"; text: string }
  | { kind: "perm"; id: number; allow: boolean }
  | { kind: "stop" }
  | { kind: "new" }
  | { kind: "resume" }
  | { kind: "unknown"; type: number };

export function parseClientFrame(f: Frame): ClientAction {
  switch (f.type) {
    case RT.HELLO:  return { kind: "hello", version: f.payload.readUInt16BE(0) };
    case RT.PROMPT: return { kind: "prompt", text: fromMacRoman(f.payload) };
    case RT.PERM:   return { kind: "perm", id: f.payload.readUInt32BE(0), allow: f.payload.readUInt8(4) === 1 };
    case RT.STOP:   return { kind: "stop" };
    case RT.NEW:    return { kind: "new" };
    case RT.RESUME: return { kind: "resume" };
    default:        return { kind: "unknown", type: f.type };
  }
}
