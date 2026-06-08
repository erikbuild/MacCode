// ABOUTME: Wraps the Claude Agent SDK in a multi-turn streaming session and
// ABOUTME: maps SDK output to internal RelayEvents so the rest of the proxy is SDK-agnostic.
import { query } from "@anthropic-ai/claude-agent-sdk";
import type {
  Query,
  SDKMessage,
  SDKResultError,
  SDKUserMessage,
  PermissionResult,
} from "@anthropic-ai/claude-agent-sdk";
import type { RelayEvent } from "./events";
import { log } from "./log";

export interface PermissionAsk {
  toolName: string;
  description: string;
}
export type PermissionFn = (ask: PermissionAsk) => Promise<boolean>;

export interface SessionOpts {
  cwd: string;
  model?: string;
  onEvent: (ev: RelayEvent) => void;
  askPermission: PermissionFn;
}

// Push-driven async queue used as the streaming prompt source. Consumers await
// the next user message; producers push text or close the stream.
class PromptQueue implements AsyncIterable<SDKUserMessage> {
  private pending: SDKUserMessage[] = [];
  private waiting: ((result: IteratorResult<SDKUserMessage>) => void) | null = null;
  private closed = false;

  push(text: string) {
    if (this.closed) return;
    const message: SDKUserMessage = {
      type: "user",
      message: { role: "user", content: text },
      parent_tool_use_id: null,
    };
    if (this.waiting) {
      const resolve = this.waiting;
      this.waiting = null;
      resolve({ value: message, done: false });
    } else {
      this.pending.push(message);
    }
  }

  close() {
    if (this.closed) return;
    this.closed = true;
    if (this.waiting) {
      const resolve = this.waiting;
      this.waiting = null;
      resolve({ value: undefined, done: true });
    }
  }

  [Symbol.asyncIterator](): AsyncIterator<SDKUserMessage> {
    return {
      next: (): Promise<IteratorResult<SDKUserMessage>> => {
        const next = this.pending.shift();
        if (next) return Promise.resolve({ value: next, done: false });
        if (this.closed) return Promise.resolve({ value: undefined, done: true });
        return new Promise((resolve) => {
          this.waiting = resolve;
        });
      },
    };
  }
}

function asRecord(input: unknown): Record<string, unknown> {
  return typeof input === "object" && input !== null ? (input as Record<string, unknown>) : {};
}

function describeTool(name: string, rawInput: unknown): string {
  const input = asRecord(rawInput);
  if (name === "Bash" && typeof input.command === "string") return `Run: ${input.command}`;
  const target = (input.file_path ?? input.path ?? input.pattern) as string | undefined;
  return target ? `${name}: ${target}` : name;
}

function toolTarget(name: string, rawInput: unknown): string | undefined {
  const input = asRecord(rawInput);
  if (name === "Bash" && typeof input.command === "string") return input.command.split(/\s+/)[0];
  return (input.file_path ?? input.path ?? input.pattern) as string | undefined;
}

// Short, human-readable message for an errored turn (SDKResultError).
function describeResultError(result: SDKResultError): string {
  const detail = result.errors?.find((e) => e.length > 0);
  return detail ? `${result.subtype}: ${detail}` : result.subtype;
}

export class Session {
  private readonly opts: SessionOpts;
  private queue = new PromptQueue();
  private query: Query | null = null;
  private runId = 0;

  constructor(opts: SessionOpts) {
    this.opts = opts;
  }

  start(mode: "new" | "resume") {
    log(`session.start (${mode})`);
    const myId = ++this.runId;
    // Tear down any prior run so a stale pump/queue can't keep emitting.
    this.queue.close();
    void this.query?.return?.(undefined);
    this.queue = new PromptQueue();
    this.query = query({
      prompt: this.queue,
      options: {
        cwd: this.opts.cwd,
        model: this.opts.model,
        permissionMode: "default",
        continue: mode === "resume",
        canUseTool: async (toolName, input): Promise<PermissionResult> => {
          const allowed = await this.opts.askPermission({
            toolName,
            description: describeTool(toolName, input),
          });
          if (allowed) return { behavior: "allow", updatedInput: input };
          return { behavior: "deny", message: "Denied by user." };
        },
      },
    });
    void this.pump(myId, this.query);
  }

  prompt(text: string) {
    this.queue.push(text);
  }

  async interrupt() {
    await this.query?.interrupt();
  }

  stop() {
    this.runId++; // invalidates the running pump's guard
    this.queue.close();
    void this.query?.return?.(undefined);
    this.query = null;
  }

  private async pump(myId: number, q: Query) {
    try {
      for await (const msg of q) {
        if (myId !== this.runId) return;
        this.handleMessage(myId, msg);
      }
    } catch (err) {
      if (myId !== this.runId) return;
      const text = err instanceof Error ? err.message : String(err);
      log(`session error: ${text}`);
      this.opts.onEvent({ kind: "error", text });
      this.opts.onEvent({ kind: "done" });
    }
  }

  private handleMessage(myId: number, msg: SDKMessage) {
    if (myId !== this.runId) return;
    if (msg.type === "assistant") {
      for (const block of msg.message.content) {
        if (block.type === "text") {
          this.opts.onEvent({ kind: "text", text: block.text });
        } else if (block.type === "tool_use") {
          log(`tool_use: ${block.name}`);
          this.opts.onEvent({ kind: "tool", name: block.name, target: toolTarget(block.name, block.input) });
        }
      }
    } else if (msg.type === "result") {
      if (msg.subtype !== "success") {
        log(`result error: ${describeResultError(msg)}`);
        this.opts.onEvent({ kind: "error", text: describeResultError(msg) });
      }
      this.opts.onEvent({ kind: "done" });
    }
  }
}
