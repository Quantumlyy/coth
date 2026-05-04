// In-flight command bookkeeping.
//
// The firmware speaks two flavours of synchronous response:
//   1. Commands with an `ok …` / `err: …` terminator — mode, dump,
//      preserved, capture on|off, wipe, attack.
//   2. Commands that just print lines and stop — status, help, keys,
//      capture (no arg). For these we settle the response after a
//      short quiet window because the firmware does not emit a
//      terminator.

import { sendCommand } from "./ble";
import type { Line } from "./types";

export interface CommandResult {
  lines: string[];
  ok: boolean;
  err: string | null;
}

interface Pending {
  expectsTerminator: boolean;
  lines: string[];
  resolve: (r: CommandResult) => void;
  hardTimer: number;
  quietTimer: number | null;
  quietMs: number;
}

let pending: Pending | null = null;
let listeners: ((busy: boolean) => void)[] = [];

function notifyListeners() {
  const busy = pending !== null;
  for (const l of listeners) l(busy);
}

/// Subscribe to in-flight changes. Returns an unsubscribe fn.
export function onPendingChange(cb: (busy: boolean) => void): () => void {
  listeners.push(cb);
  cb(pending !== null);
  return () => {
    listeners = listeners.filter((l) => l !== cb);
  };
}

function clearPendingTimers(p: Pending) {
  window.clearTimeout(p.hardTimer);
  if (p.quietTimer !== null) window.clearTimeout(p.quietTimer);
}

function settle(result: CommandResult) {
  if (!pending) return;
  const p = pending;
  pending = null;
  clearPendingTimers(p);
  notifyListeners();
  p.resolve(result);
}

function bumpQuietTimer() {
  if (!pending || pending.expectsTerminator) return;
  if (pending.quietTimer !== null) window.clearTimeout(pending.quietTimer);
  const p = pending;
  p.quietTimer = window.setTimeout(() => {
    settle({ lines: p.lines, ok: true, err: null });
  }, p.quietMs);
}

/// Returns true if the line was consumed by a pending command.
export function consumeLine(line: Line): boolean {
  if (!pending) return false;
  if (line.kind === "async") return false;
  if (line.kind === "ok") {
    if (line.detail) pending.lines.push("ok " + line.detail);
    settle({ lines: pending.lines, ok: true, err: null });
    return true;
  }
  if (line.kind === "err") {
    settle({ lines: pending.lines, ok: false, err: line.reason });
    return true;
  }
  pending.lines.push(line.text);
  bumpQuietTimer();
  return true;
}

export interface DispatchOptions {
  /** True if the command finishes with `ok …` / `err: …`. */
  expectsTerminator: boolean;
  /** Hard upper bound for the whole command. */
  timeoutMs?: number;
  /** Quiet-window settle for non-terminator commands. */
  quietMs?: number;
}

export async function dispatch(
  command: string,
  options: DispatchOptions,
): Promise<CommandResult> {
  if (pending) throw new Error("another command is in flight");
  const timeoutMs = options.timeoutMs ?? 5_000;
  const quietMs = options.quietMs ?? 350;

  return new Promise<CommandResult>((resolve) => {
    pending = {
      expectsTerminator: options.expectsTerminator,
      lines: [],
      resolve,
      hardTimer: 0,
      quietTimer: null,
      quietMs,
    };
    pending.hardTimer = window.setTimeout(() => {
      settle({
        lines: pending?.lines ?? [],
        ok: !options.expectsTerminator,
        err: options.expectsTerminator ? "timeout" : null,
      });
    }, timeoutMs);
    notifyListeners();
    // The quiet window starts on the first response line, not on dispatch
    // itself: sendCommand can take 500 ms on the iOS auth-retry path, and
    // a quietMs shorter than that would otherwise resolve before any reply
    // reached the framer. The hard timeout still bounds no-response cases.

    sendCommand(command).catch((e) => {
      settle({ lines: [], ok: false, err: `send: ${String(e)}` });
    });
  });
}

export function isPending(): boolean {
  return pending !== null;
}
