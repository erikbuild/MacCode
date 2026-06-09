// ABOUTME: Runs a user-invoked "!" shell command on the proxy host and streams its
// ABOUTME: stdout/stderr back as RelayEvents. Bypasses Claude; the SE user invoked it.
import { spawn } from "node:child_process";
import type { RelayEvent } from "./events";
import { log } from "./log";

// Returns a handle so the caller can kill the child if the client disconnects mid-run.
export function runShell(
  command: string,
  cwd: string,
  onEvent: (ev: RelayEvent) => void,
): { kill: () => void } {
  log(`shell: ${JSON.stringify(command)}`);
  onEvent({ kind: "info", text: `$ ${command}` });
  // A spawn failure makes Node fire BOTH 'error' and 'close'; latch so the SE gets
  // exactly one terminal 'done' regardless of which (or both) fire.
  let finished = false;
  const finish = () => { if (!finished) { finished = true; onEvent({ kind: "done" }); } };
  const child = spawn("/bin/sh", ["-c", command], { cwd });
  child.stdout.on("data", (b: Buffer) => onEvent({ kind: "text", text: b.toString("utf8") }));
  child.stderr.on("data", (b: Buffer) => onEvent({ kind: "text", text: b.toString("utf8") }));
  child.on("error", (e) => { onEvent({ kind: "error", text: e.message }); finish(); });
  child.on("close", (code) => {
    if (code && code !== 0) onEvent({ kind: "error", text: `exit ${code}` });
    finish();
  });
  return { kill: () => { try { child.kill(); } catch { /* already gone */ } } };
}
