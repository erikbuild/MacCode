// ABOUTME: Integration tests for the TCP server — connection handling, frame routing,
// ABOUTME: permission round-trips, malformed frame drops, and single-client enforcement.
import { describe, it, expect, afterEach } from "vitest";
import net from "node:net";
import { createServer, type SessionLike, type ServerDeps } from "../src/server";
import { RT, FrameDecoder, encodeFrame } from "../src/protocol";
import type { RelayEvent } from "../src/events";
import type { PermissionFn } from "../src/session";

class FakeSession implements SessionLike {
  onEvent!: (ev: RelayEvent) => void;
  ask!: PermissionFn;
  started: ("new" | "resume")[] = [];
  prompts: string[] = [];
  interrupted = 0;
  stopped = 0;
  start(mode: "new" | "resume") { this.started.push(mode); }
  prompt(t: string) { this.prompts.push(t); }
  async interrupt() { this.interrupted++; }
  stop() { this.stopped++; }
}

function frame(type: number, payload = Buffer.alloc(0)) { return encodeFrame(type, payload); }
const HELLO = frame(RT.HELLO, Buffer.from([0x00, 0x01]));
const wait = (ms = 30) => new Promise(r => setTimeout(r, ms));

let srv: net.Server;
afterEach(() => new Promise<void>(r => srv?.close(() => r())));

function startServer(fake: FakeSession) {
  const deps: ServerDeps = {
    makeSession: (onEvent, ask) => { fake.onEvent = onEvent; fake.ask = ask; return fake; },
    cwdLabel: "~/proj",
  };
  srv = createServer(deps);
  return new Promise<number>(res => srv.listen(0, "127.0.0.1", () => res((srv.address() as net.AddressInfo).port)));
}

function connect(port: number) {
  const sock = net.connect(port, "127.0.0.1");
  const dec = new FrameDecoder();
  const frames: { type: number; payload: Buffer }[] = [];
  const state = { closed: false };
  sock.on("data", d => frames.push(...dec.push(d)));
  sock.on("close", () => { state.closed = true; });
  return { sock, frames, state };
}

describe("server", () => {
  it("on HELLO, starts a session and replies INFO", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames } = connect(port); await wait();
    sock.write(HELLO); await wait();
    expect(fake.started).toEqual(["new"]);
    expect(frames.some(f => f.type === RT.INFO)).toBe(true);
    sock.end();
  });

  it("forwards PROMPT, emits VERB then relays text + done (with verb-clear)", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames } = connect(port); await wait();
    sock.write(HELLO); sock.write(frame(RT.PROMPT, Buffer.from("hi"))); await wait();
    expect(fake.prompts).toEqual(["hi"]);
    fake.onEvent({ kind: "text", text: "hello back" });
    fake.onEvent({ kind: "done" });
    await wait();
    const types = frames.map(f => f.type);
    expect(types).toContain(RT.VERB);
    expect(types).toContain(RT.TEXT);
    expect(types).toContain(RT.DONE);
    // a done is followed by an empty VERB (clear)
    const doneIdx = frames.findIndex(f => f.type === RT.DONE);
    const after = frames.slice(doneIdx + 1);
    expect(after.some(f => f.type === RT.VERB && f.payload.length === 0)).toBe(true);
    sock.end();
  });

  it("permission round-trip: ASK is sent, PERM resolves the callback", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames } = connect(port); await wait();
    sock.write(HELLO); await wait();
    const decision = fake.ask({ toolName: "Bash", description: "Run: ls" });
    await wait();
    const ask = frames.find(f => f.type === RT.ASK)!;
    expect(ask).toBeTruthy();
    const id = ask.payload.readUInt32BE(0);
    const perm = Buffer.alloc(5); perm.writeUInt32BE(id, 0); perm.writeUInt8(1, 4);
    sock.write(frame(RT.PERM, perm));
    expect(await decision).toBe(true);
    sock.end();
  });

  it("STOP triggers interrupt(); NEW and RESUME re-start the session", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock } = connect(port); await wait();
    sock.write(HELLO); await wait();
    sock.write(frame(RT.STOP)); await wait();
    expect(fake.interrupted).toBe(1);
    sock.write(frame(RT.NEW)); await wait();
    sock.write(frame(RT.RESUME)); await wait();
    expect(fake.started).toEqual(["new", "new", "resume"]); // hello + new + resume
    sock.end();
  });

  it("relays an error event as an ERR frame", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames } = connect(port); await wait();
    sock.write(HELLO); await wait();
    fake.onEvent({ kind: "error", text: "boom" });
    await wait();
    const err = frames.find(f => f.type === RT.ERR && f.payload.toString() === "boom");
    expect(err).toBeTruthy();
    sock.end();
  });

  it("rejects a second client with ERR and closes it", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const a = connect(port); await wait();
    a.sock.write(HELLO); await wait();
    const b = connect(port); await wait(50);
    expect(b.frames.some(f => f.type === RT.ERR)).toBe(true);
    expect(b.state.closed).toBe(true);
    a.sock.end();
  });

  it("drops the connection on a malformed (too-short) frame", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames, state } = connect(port); await wait();
    sock.write(HELLO); await wait();
    // PERM requires >=5 payload bytes; send 2 -> parseClientFrame throws -> drop
    sock.write(frame(RT.PERM, Buffer.from([0x00, 0x00]))); await wait(50);
    expect(frames.some(f => f.type === RT.ERR)).toBe(true);
    expect(state.closed).toBe(true);
  });

  it("drops the connection on an unknown record type", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames, state } = connect(port); await wait();
    sock.write(HELLO); await wait();
    sock.write(frame(0x7f)); await wait(50); // unrecognized type
    expect(frames.some(f => f.type === RT.ERR)).toBe(true);
    expect(state.closed).toBe(true);
  });

  it("rejects a HELLO with an unsupported protocol version", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, frames, state } = connect(port); await wait();
    sock.write(frame(RT.HELLO, Buffer.from([0x00, 0x02]))); await wait(50); // version 2
    expect(fake.started).toEqual([]);                      // never started
    expect(frames.some(f => f.type === RT.ERR)).toBe(true);
    expect(state.closed).toBe(true);
  });

  it("aborts the rest of a chunk after a malformed frame", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const { sock, state } = connect(port); await wait();
    // one chunk: valid HELLO + malformed PERM (2 bytes) + a PROMPT that must NOT be processed
    const chunk = Buffer.concat([HELLO, frame(RT.PERM, Buffer.from([0x00, 0x00])), frame(RT.PROMPT, Buffer.from("late"))]);
    sock.write(chunk); await wait(50);
    expect(fake.started).toEqual(["new"]);  // HELLO processed
    expect(fake.prompts).toEqual([]);       // PROMPT after the malformed frame was aborted
    expect(state.closed).toBe(true);
  });

  it("cleans up (session.stop + frees the slot) when the client disconnects", async () => {
    const fake = new FakeSession(); const port = await startServer(fake);
    const a = connect(port); await wait();
    a.sock.write(HELLO); await wait();
    a.sock.destroy(); await wait(50);
    expect(fake.stopped).toBeGreaterThanOrEqual(1);
    // a new client should now be accepted (slot freed)
    const b = connect(port); await wait(50);
    expect(b.frames.some(f => f.type === RT.ERR)).toBe(false);
    b.sock.end();
  });
});
