// Frame-dump line parser.
//
// Format (docs/ble_protocol.md):
//   [t=HH:MM:SS.mmm] dir=<cp2pd|pd2cp|?> addr=0xNN cmd=0xHH len=N flags=F  HH HH …
//
// flags is a bitmask: 1=encrypted, 2=bad_crc, 4=bad_mac, 8=truncated, 16=oversize.

use crate::ParseError;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Direction {
    // serde's snake_case turns `Cp2Pd` into `cp2_pd`; the firmware and the
    // TS layer both use `cp2pd` / `pd2cp` (no internal underscore), so pin
    // the wire form explicitly.
    #[serde(rename = "cp2pd")]
    Cp2Pd,
    #[serde(rename = "pd2cp")]
    Pd2Cp,
    Unknown,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct DumpRow {
    pub t_ms: u64,
    pub dir: Direction,
    pub addr: u8,
    pub cmd: u8,
    pub len: u32,
    pub flags: u8,
    pub bytes_hex: String,
}

impl DumpRow {
    pub fn encrypted(&self) -> bool { self.flags & 0x01 != 0 }
    pub fn bad_crc(&self) -> bool   { self.flags & 0x02 != 0 }
    pub fn bad_mac(&self) -> bool   { self.flags & 0x04 != 0 }
    pub fn truncated(&self) -> bool { self.flags & 0x08 != 0 }
    pub fn oversize(&self) -> bool  { self.flags & 0x10 != 0 }
}

pub fn parse(line: &str) -> Result<DumpRow, ParseError> {
    let rest = line
        .strip_prefix("[t=")
        .ok_or_else(|| ParseError::Malformed(line.to_string()))?;
    let (ts, rest) = rest
        .split_once("] ")
        .ok_or_else(|| ParseError::Malformed(line.to_string()))?;

    let t_ms = parse_timestamp(ts)?;

    let dir_s = take_kv(rest, "dir=")?;
    let (dir_val, rest) = split_at_space(dir_s);
    let dir = match dir_val {
        "cp2pd" => Direction::Cp2Pd,
        "pd2cp" => Direction::Pd2Cp,
        "?" => Direction::Unknown,
        _ => return Err(ParseError::Malformed(line.to_string())),
    };

    let addr_s = take_kv(rest, "addr=0x")?;
    let (addr_val, rest) = split_at_space(addr_s);
    let addr = u8::from_str_radix(addr_val, 16)
        .map_err(|_| ParseError::BadInt { field: "addr", value: addr_val.into() })?;

    let cmd_s = take_kv(rest, "cmd=0x")?;
    let (cmd_val, rest) = split_at_space(cmd_s);
    let cmd = u8::from_str_radix(cmd_val, 16)
        .map_err(|_| ParseError::BadInt { field: "cmd", value: cmd_val.into() })?;

    let len_s = take_kv(rest, "len=")?;
    let (len_val, rest) = split_at_space(len_s);
    let len: u32 = len_val
        .parse()
        .map_err(|_| ParseError::BadInt { field: "len", value: len_val.into() })?;

    let flags_s = take_kv(rest, "flags=")?;
    let (flags_val, rest) = split_at_space(flags_s);
    let flags: u8 = flags_val
        .parse()
        .map_err(|_| ParseError::BadInt { field: "flags", value: flags_val.into() })?;

    // After flags the firmware emits two spaces, then the hex byte stream.
    let bytes_hex = rest.trim_start().to_string();

    Ok(DumpRow { t_ms, dir, addr, cmd, len, flags, bytes_hex })
}

fn parse_timestamp(ts: &str) -> Result<u64, ParseError> {
    // HH:MM:SS.mmm
    let (hms, ms) = ts
        .split_once('.')
        .ok_or_else(|| ParseError::Malformed(ts.into()))?;
    let parts: Vec<&str> = hms.split(':').collect();
    if parts.len() != 3 {
        return Err(ParseError::Malformed(ts.into()));
    }
    let h: u64 = parts[0].parse().map_err(|_| ParseError::BadInt { field: "t.h", value: parts[0].into() })?;
    let m: u64 = parts[1].parse().map_err(|_| ParseError::BadInt { field: "t.m", value: parts[1].into() })?;
    let s: u64 = parts[2].parse().map_err(|_| ParseError::BadInt { field: "t.s", value: parts[2].into() })?;
    let ms_n: u64 = ms.parse().map_err(|_| ParseError::BadInt { field: "t.ms", value: ms.into() })?;
    Ok(((h * 3600 + m * 60 + s) * 1000) + ms_n)
}

/// Expects `prefix` to match the start of `s` (or one leading space + prefix).
fn take_kv<'a>(s: &'a str, prefix: &str) -> Result<&'a str, ParseError> {
    let s = s.trim_start();
    s.strip_prefix(prefix)
        .ok_or_else(|| ParseError::Malformed(format!("expected '{}' in: {}", prefix, s)))
}

fn split_at_space(s: &str) -> (&str, &str) {
    match s.find(' ') {
        Some(i) => (&s[..i], &s[i..]),
        None => (s, ""),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_canonical_line() {
        let row = parse("[t=00:01:23.456] dir=cp2pd addr=0x00 cmd=0x60 len=8 flags=0  53 00 08 00 00 60 ?? ??").unwrap();
        assert_eq!(row.t_ms, 83_456);
        assert_eq!(row.dir, Direction::Cp2Pd);
        assert_eq!(row.addr, 0x00);
        assert_eq!(row.cmd, 0x60);
        assert_eq!(row.len, 8);
        assert_eq!(row.flags, 0);
        assert_eq!(row.bytes_hex, "53 00 08 00 00 60 ?? ??");
    }

    #[test]
    fn flag_helpers() {
        let mut row = parse("[t=00:00:00.000] dir=pd2cp addr=0x01 cmd=0x40 len=6 flags=3  AA BB").unwrap();
        assert!(row.encrypted());
        assert!(row.bad_crc());
        assert!(!row.bad_mac());
        row.flags = 16;
        assert!(row.oversize());
    }

    #[test]
    fn rejects_garbage() {
        assert!(parse("totally not a dump line").is_err());
    }
}
