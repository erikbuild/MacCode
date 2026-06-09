// ABOUTME: Tests for runShell — streaming a host shell command back as RelayEvents.
// ABOUTME: Covers stdout streaming, non-zero exit reporting, and the leading info line.
import { describe, it, expect } from "vitest";
import { runShell } from "../src/shellrun";
import type { RelayEvent } from "../src/events";

function collect(cmd: string, cwd: string): Promise<RelayEvent[]> {
  return new Promise((resolve) => {
    const evs: RelayEvent[] = [];
    runShell(cmd, cwd, (ev) => { evs.push(ev); if (ev.kind === "done") resolve(evs); });
  });
}

// Collects every event over a fixed window (doesn't stop at the first "done") so we can
// assert no stray terminal events arrive after completion.
function collectFor(cmd: string, cwd: string, ms: number): Promise<RelayEvent[]> {
  return new Promise((resolve) => {
    const evs: RelayEvent[] = [];
    runShell(cmd, cwd, (ev) => { evs.push(ev); });
    setTimeout(() => resolve(evs), ms);
  });
}

describe("runShell", () => {
  it("streams stdout then a done event", async () => {
    const evs = await collect("echo hello", process.cwd());
    const text = evs.filter(e => e.kind === "text").map(e => (e as Extract<RelayEvent, {kind:"text"}>).text).join("");
    expect(text).toContain("hello");
    expect(evs[evs.length - 1].kind).toBe("done");
  });
  it("reports a non-zero exit as an error event then done", async () => {
    const evs = await collect("exit 3", process.cwd());
    expect(evs.some(e => e.kind === "error")).toBe(true);
    expect(evs[evs.length - 1].kind).toBe("done");
  });
  it("echoes the command as an info line first", async () => {
    const evs = await collect("true", process.cwd());
    expect(evs[0]).toEqual({ kind: "info", text: "$ true" });
  });
  it("emits exactly one done even when the spawn fails (bad cwd)", async () => {
    const evs = await collectFor("echo hi", "/no/such/dir/maccode-test", 200);
    expect(evs.filter(e => e.kind === "done").length).toBe(1);
    expect(evs.some(e => e.kind === "error")).toBe(true);
  });
});
