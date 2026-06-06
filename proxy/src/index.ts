// ABOUTME: CLI entry point for the maccode-relay proxy server.
// ABOUTME: Parses --port/--host/--project/--model/--echo flags and starts either the echo or relay server.
import net from "node:net";
import path from "node:path";
import { createServer } from "./server";
import { Session } from "./session";
import { FrameDecoder, encodeFrame } from "./protocol";

function arg(name: string, def?: string): string | undefined {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 ? process.argv[i + 1] : def;
}

const port = Number(arg("port", "4242"));
if (!Number.isInteger(port) || port < 0 || port > 65535) {
  throw new Error(`invalid --port: ${arg("port", "4242")}`);
}
// Default binds all interfaces so the Basilisk II guest (via the slirp gateway
// 10.0.2.2) can reach the host. Trusted-LAN use only; override with --host.
const host = arg("host", "0.0.0.0")!;
const project = path.resolve(arg("project", process.cwd())!);
const model = arg("model");
const echo = process.argv.includes("--echo");

if (echo) {
  // SE bring-up target: echo every received frame back unchanged.
  const srv = net.createServer((sock) => {
    const dec = new FrameDecoder();
    sock.on("data", (c) => { for (const f of dec.push(c)) sock.write(encodeFrame(f.type, f.payload)); });
  });
  srv.listen(port, host, () => console.log(`maccode-relay ECHO on ${host}:${port}`));
} else {
  const srv = createServer({
    cwdLabel: project.replace(process.env.HOME ?? "", "~"),
    makeSession: (onEvent, askPermission) => new Session({ cwd: project, model, onEvent, askPermission }),
  });
  srv.listen(port, host, () => console.log(`maccode-relay on ${host}:${port} · project ${project}`));
}
