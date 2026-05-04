import { useState } from "react";
import { dispatch } from "../session";
import type { Mode } from "../types";

interface Props {
  connected: boolean;
}

const MODES: { id: Mode; label: string; note: string }[] = [
  { id: "idle", label: "idle", note: "BLE only, sniff disabled" },
  { id: "sniff", label: "sniff", note: "passive frame capture" },
  { id: "keyset_watch", label: "keyset_watch", note: "preserve KEYSET frames in NVS" },
  { id: "weak_keys", label: "weak_keys", note: "offline AES trial decrypt" },
];

export default function ModeScreen({ connected }: Props) {
  const [busy, setBusy] = useState(false);
  const [last, setLast] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  async function setMode(m: Mode) {
    setBusy(true);
    setError(null);
    try {
      const r = await dispatch(`mode ${m}\n`, { expectsTerminator: true });
      if (r.ok) {
        setLast(m);
      } else {
        setError(r.err ?? "unknown error");
      }
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="screen">
      <h2>Mode</h2>
      {!connected && <p className="muted">connect first</p>}
      <ul className="mode-list">
        {MODES.map((m) => (
          <li key={m.id}>
            <button disabled={!connected || busy} onClick={() => setMode(m.id)}>
              <strong>{m.label}</strong>
              <span className="muted">{m.note}</span>
              {last === m.id && <span className="badge">active</span>}
            </button>
          </li>
        ))}
      </ul>
      {error && <p className="error">err: {error}</p>}
    </section>
  );
}
