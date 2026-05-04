// TS mirrors of mellon-proto serde DTOs. Keep in sync with crates/mellon-proto.

export type Direction = "cp2pd" | "pd2cp" | "unknown";

export interface DumpRow {
  t_ms: number;
  dir: Direction;
  addr: number;
  cmd: number;
  len: number;
  flags: number;
  bytes_hex: string;
}

export interface StatusBlock {
  build: string;
  uptime: string;
  mode: string;
  frames_total: number;
  frames_bad_crc: number;
  frames_resync: number;
  frames_oversize: number;
  polls_n: number;
  polls_avg_ms: number;
  polls_min_ms: number;
  polls_max_ms: number;
  libosdp_present: boolean;
}

export interface KeyRow {
  addr: number;
  sc_active: boolean;
  scbk_known: boolean;
}

export type AsyncEvent =
  | { kind: "keyset_seen"; t: string; addr: number; scbk: string }
  | { kind: "match"; candidates: number; detail: string }
  | { kind: "armed_expired" }
  | { kind: "unknown"; text: string };

export type Mode = "idle" | "sniff" | "keyset_watch" | "weak_keys";

export type Line =
  | { kind: "sync"; text: string }
  | { kind: "async"; text: string }
  | { kind: "ok"; detail: string }
  | { kind: "err"; reason: string };
