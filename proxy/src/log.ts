// ABOUTME: Minimal timestamped stderr logger for the proxy — connection lifecycle,
// ABOUTME: frame traffic, and session/SDK errors, so failures are visible while debugging.

// Stay silent under vitest so test output remains pristine; log everywhere else.
const SILENT = !!process.env.VITEST || process.env.NODE_ENV === "test";

export function log(...args: unknown[]): void {
  if (SILENT) return;
  const t = new Date().toISOString().slice(11, 23); // HH:MM:SS.mmm
  console.error(`[${t}]`, ...args);
}
