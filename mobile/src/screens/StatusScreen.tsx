import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { dispatch, onPendingChange } from "../session";
import type { StatusBlock } from "../types";

interface Props {
  connected: boolean;
}

export default function StatusScreen({ connected }: Props) {
  const [block, setBlock] = useState<StatusBlock | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState(false);

  useEffect(() => onPendingChange(setPending), []);

  async function refresh() {
    setBusy(true);
    setError(null);
    try {
      const r = await dispatch("status\n", {
        expectsTerminator: false,
        quietMs: 400,
        timeoutMs: 4_000,
      });
      if (!r.ok) {
        setError(r.err ?? "unknown error");
        return;
      }
      const parsed = await invoke<StatusBlock>("parse_status_block", {
        lines: r.lines,
      });
      setBlock(parsed);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="screen">
      <h2>Status</h2>
      <button onClick={refresh} disabled={!connected || busy || pending}>
        {busy ? "refreshing…" : "Refresh"}
      </button>
      {error && <p className="error">err: {error}</p>}
      {block && (
        <dl className="kv">
          <dt>build</dt><dd>{block.build}</dd>
          <dt>uptime</dt><dd>{block.uptime}</dd>
          <dt>mode</dt><dd>{block.mode}</dd>
          <dt>frames</dt>
          <dd>
            total {block.frames_total} · bad_crc {block.frames_bad_crc} · resync{" "}
            {block.frames_resync} · oversize {block.frames_oversize}
          </dd>
          <dt>polls</dt>
          <dd>
            n {block.polls_n} · avg {block.polls_avg_ms}ms · min{" "}
            {block.polls_min_ms}ms · max {block.polls_max_ms}ms
          </dd>
          <dt>libosdp</dt><dd>{block.libosdp_present ? "present" : "absent"}</dd>
        </dl>
      )}
    </section>
  );
}
