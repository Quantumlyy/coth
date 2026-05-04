import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { dispatch, onPendingChange } from "../session";
import type { Direction, DumpRow } from "../types";

interface Props {
  connected: boolean;
}

const DIR_LABEL: Record<Direction, string> = {
  cp2pd: "CP→PD",
  pd2cp: "PD→CP",
  unknown: "?",
};

function flagsLabel(row: DumpRow): string {
  const f = row.flags;
  const tags: string[] = [];
  if (f & 0x01) tags.push("enc");
  if (f & 0x02) tags.push("bad_crc");
  if (f & 0x04) tags.push("bad_mac");
  if (f & 0x08) tags.push("trunc");
  if (f & 0x10) tags.push("oversize");
  return tags.length ? tags.join(" ") : "—";
}

function formatTime(ms: number): string {
  const s = Math.floor(ms / 1000);
  const hh = String(Math.floor(s / 3600)).padStart(2, "0");
  const mm = String(Math.floor((s / 60) % 60)).padStart(2, "0");
  const ss = String(s % 60).padStart(2, "0");
  const fr = String(ms % 1000).padStart(3, "0");
  return `${hh}:${mm}:${ss}.${fr}`;
}

export default function DumpScreen({ connected }: Props) {
  const [count, setCount] = useState(20);
  const [rows, setRows] = useState<DumpRow[]>([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState(false);

  useEffect(() => onPendingChange(setPending), []);

  async function runFetch(command: string) {
    setBusy(true);
    setError(null);
    try {
      const r = await dispatch(command, {
        expectsTerminator: true,
        timeoutMs: 10_000,
      });
      if (!r.ok) {
        setError(r.err ?? "unknown error");
        return;
      }
      // The terminator line may have been folded in as `ok N records`;
      // skip it and parse the rest.
      const dataLines = r.lines.filter((l) => !l.startsWith("ok "));
      const parsed: DumpRow[] = [];
      for (const line of dataLines) {
        try {
          parsed.push(await invoke<DumpRow>("parse_dump_row", { line }));
        } catch {
          // ignore individual unparsable lines — show what we got
        }
      }
      setRows(parsed);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="screen">
      <h2>Dump</h2>
      <div className="dump-controls">
        <label>
          <span className="muted">count</span>
          <input
            type="number"
            min={1}
            max={1000}
            value={count}
            onChange={(e) => setCount(Number(e.target.value) || 1)}
          />
        </label>
        <button
          disabled={!connected || busy || pending}
          onClick={() => runFetch(`dump ${count}\n`)}
        >
          {busy ? "fetching…" : "Recent"}
        </button>
        <button
          disabled={!connected || busy || pending}
          onClick={() => runFetch("preserved\n")}
        >
          Preserved
        </button>
      </div>
      {error && <p className="error">err: {error}</p>}

      <ol className="dump-list">
        {rows.map((row, i) => (
          <li key={i}>
            <header>
              <span className="t">{formatTime(row.t_ms)}</span>
              <span className="dir">{DIR_LABEL[row.dir]}</span>
              <span>addr 0x{row.addr.toString(16).padStart(2, "0")}</span>
              <span>cmd 0x{row.cmd.toString(16).padStart(2, "0")}</span>
              <span>len {row.len}</span>
              <span className="flags">{flagsLabel(row)}</span>
            </header>
            <code>{row.bytes_hex}</code>
          </li>
        ))}
      </ol>
      {rows.length === 0 && !busy && (
        <p className="muted">no rows yet — pick Recent or Preserved.</p>
      )}
    </section>
  );
}
