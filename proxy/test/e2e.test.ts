// ABOUTME: Opt-in end-to-end test — drives a real Session through createServer over a real TCP socket.
// ABOUTME: Gated behind MACCODE_E2E=1; skipped by default so normal `npm test` needs no auth.
import { describe, it, expect } from "vitest";
import net from "node:net";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { createServer } from "../src/server";
import { Session } from "../src/session";
import { RT, FrameDecoder, encodeFrame } from "../src/protocol";

const run = process.env.MACCODE_E2E === "1" ? describe : describe.skip;

run("e2e: real Agent SDK round-trip", () => {
  it("answers a trivial prompt and reaches DONE", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "maccode-e2e-"));
    const srv = createServer({
      cwdLabel: dir,
      makeSession: (onEvent, ask) => new Session({ cwd: dir, onEvent, askPermission: ask }),
    });
    const port: number = await new Promise((r) =>
      srv.listen(0, "127.0.0.1", () => r((srv.address() as net.AddressInfo).port))
    );
    const sock = net.connect(port, "127.0.0.1");
    const dec = new FrameDecoder();
    const got: number[] = [];
    sock.on("data", (d) => dec.push(d).forEach((f) => got.push(f.type)));
    sock.write(encodeFrame(RT.HELLO, Buffer.from([0, 1])));
    sock.write(encodeFrame(RT.PROMPT, Buffer.from("Reply with the single word: pong")));
    await new Promise<void>((res) => {
      const t = setInterval(() => { if (got.includes(RT.DONE)) { clearInterval(t); res(); } }, 200);
    });
    expect(got).toContain(RT.TEXT);
    expect(got).toContain(RT.DONE);
    sock.end();
    srv.close();
  }, 60_000);
});
