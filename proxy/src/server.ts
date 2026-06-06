// ABOUTME: TCP server — single-client connection management, frame routing,
// ABOUTME: turn/verb orchestration, and permission ask/resolve round-trips.
import net from "node:net";
import { FrameDecoder, PROTOCOL_VERSION } from "./protocol";
import { eventToFrames, verbFrame, clearVerbFrame, askFrame, infoFrame, parseClientFrame, type ClientAction } from "./translate";
import { verbForTurn } from "./verbs";
import type { RelayEvent } from "./events";
import type { PermissionAsk, PermissionFn } from "./session";

export interface SessionLike {
  start(mode: "new" | "resume"): void;
  prompt(text: string): void;
  interrupt(): Promise<void>;
  stop(): void;
}

export interface ServerDeps {
  makeSession: (onEvent: (ev: RelayEvent) => void, askPermission: PermissionFn) => SessionLike;
  cwdLabel: string;
}

export function createServer(deps: ServerDeps): net.Server {
  let active: net.Socket | null = null;

  return net.createServer((sock) => {
    if (active) {
      sock.write(eventToFrames({ kind: "error", text: "MacCode proxy is busy (one client only)." }));
      sock.end();
      return;
    }
    active = sock;

    const dec = new FrameDecoder();
    let turn = 0;
    let askId = 1;
    const pending = new Map<number, (allow: boolean) => void>();

    const send = (buf: Buffer) => { if (!sock.destroyed) sock.write(buf); };
    const drop = (why: string) => { send(eventToFrames({ kind: "error", text: why })); sock.destroy(); };

    const onEvent = (ev: RelayEvent) => {
      send(eventToFrames(ev));
      if (ev.kind === "done") send(clearVerbFrame());
    };

    const askPermission: PermissionFn = (ask: PermissionAsk) =>
      new Promise<boolean>((resolve) => {
        const id = askId++;
        pending.set(id, resolve);
        send(askFrame(id, ask.description));
      });

    const session = deps.makeSession(onEvent, askPermission);

    sock.on("data", (chunk) => {
      for (const f of dec.push(chunk)) {
        let a: ClientAction;
        try {
          a = parseClientFrame(f);
        } catch {
          drop("Malformed frame; closing connection.");
          return;
        }
        switch (a.kind) {
          case "hello":
            if (a.version !== PROTOCOL_VERSION) {
              drop(`Unsupported protocol version ${a.version} (proxy speaks ${PROTOCOL_VERSION}).`);
              return;
            }
            session.start("new");
            send(infoFrame(`Project: ${deps.cwdLabel}`));
            break;
          case "prompt":
            send(verbFrame(verbForTurn(turn++)));
            session.prompt(a.text);
            break;
          case "perm": {
            const r = pending.get(a.id);
            if (r) { pending.delete(a.id); r(a.allow); }
            break;
          }
          case "stop":
            void session.interrupt();
            break;
          case "new":
            session.start("new");
            send(infoFrame("New conversation."));
            break;
          case "resume":
            session.start("resume");
            send(infoFrame("Resumed most recent conversation."));
            break;
          case "unknown":
            drop("Unknown record; closing connection.");
            return;
        }
      }
    });

    let cleanedUp = false;
    const cleanup = () => {
      if (cleanedUp) return;
      cleanedUp = true;
      session.stop();
      for (const [, r] of pending) r(false);
      pending.clear();
      if (active === sock) active = null;
    };
    sock.on("close", cleanup);
    sock.on("error", cleanup);
  });
}
