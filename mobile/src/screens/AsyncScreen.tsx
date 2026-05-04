import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { AsyncEvent } from "../types";

export interface AsyncRecord {
  /** ms since the page loaded — we don't have device-time here. */
  t: number;
  raw: string;
  parsed?: AsyncEvent;
}

interface Props {
  events: AsyncRecord[];
}

function describe(e: AsyncEvent | undefined, raw: string): string {
  if (!e) return raw;
  switch (e.kind) {
    case "keyset_seen":
      return `KEYSET at t=${e.t} addr=0x${e.addr.toString(16).padStart(2, "0")} scbk=${e.scbk}`;
    case "match":
      return `MATCH after ${e.candidates} candidates: ${e.detail}`;
    case "armed_expired":
      return "ARMED expired";
    case "unknown":
      return e.text;
  }
}

function formatLocalTime(ms: number): string {
  const d = new Date(ms);
  return d.toLocaleTimeString();
}

export default function AsyncScreen({ events }: Props) {
  const [enriched, setEnriched] = useState<AsyncRecord[]>([]);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const out: AsyncRecord[] = [];
      for (const ev of events) {
        if (ev.parsed) {
          out.push(ev);
        } else {
          try {
            const parsed = await invoke<AsyncEvent>("parse_async_event", {
              body: ev.raw,
            });
            out.push({ ...ev, parsed });
          } catch {
            out.push(ev);
          }
        }
      }
      if (!cancelled) setEnriched(out);
    })();
    return () => {
      cancelled = true;
    };
  }, [events]);

  return (
    <section className="screen">
      <h2>Async events</h2>
      {enriched.length === 0 && (
        <p className="muted">no async notifications yet — switch to keyset_watch or run weak_keys.</p>
      )}
      <ol className="async-list">
        {enriched.map((ev, i) => (
          <li key={i}>
            <span className="t">{formatLocalTime(ev.t)}</span>
            <span>{describe(ev.parsed, ev.raw)}</span>
          </li>
        ))}
      </ol>
    </section>
  );
}
