// ABOUTME: Internal, SDK-agnostic relay events for the MacCode proxy.
// ABOUTME: session.ts maps SDK output to these; translate.ts maps these to wire frames.
export type RelayEvent =
  | { kind: "text"; text: string }                  // one assistant text block
  | { kind: "tool"; name: string; target?: string } // a tool-use the agent performed
  | { kind: "done" }                                 // end of turn
  | { kind: "info"; text: string }
  | { kind: "error"; text: string };
