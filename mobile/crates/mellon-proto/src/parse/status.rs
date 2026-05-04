// Status block parser.
//
// Format (docs/ble_protocol.md / src/ble/nus_console.c cmd_status):
//   build:    <ver> (<hash>)
//   uptime:   HH:MM:SS
//   mode:     <name>
//   frames:   total=N bad_crc=N resync=N oversize=N
//   polls:    n=N avg=Nms min=Nms max=Nms
//   libosdp:  <present|absent>

use crate::ParseError;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StatusBlock {
    pub build: String,
    pub uptime: String,
    pub mode: String,
    pub frames_total: u32,
    pub frames_bad_crc: u32,
    pub frames_resync: u32,
    pub frames_oversize: u32,
    pub polls_n: u32,
    pub polls_avg_ms: u32,
    pub polls_min_ms: u32,
    pub polls_max_ms: u32,
    pub libosdp_present: bool,
}

pub fn parse(lines: &[&str]) -> Result<StatusBlock, ParseError> {
    let mut build = None;
    let mut uptime = None;
    let mut mode = None;
    let mut frames = None;
    let mut polls = None;
    let mut libosdp = None;

    for line in lines {
        if let Some(v) = strip_prefix_value(line, "build:") {
            build = Some(v.to_string());
        } else if let Some(v) = strip_prefix_value(line, "uptime:") {
            uptime = Some(v.to_string());
        } else if let Some(v) = strip_prefix_value(line, "mode:") {
            mode = Some(v.to_string());
        } else if let Some(v) = strip_prefix_value(line, "frames:") {
            frames = Some(parse_frames(v)?);
        } else if let Some(v) = strip_prefix_value(line, "polls:") {
            polls = Some(parse_polls(v)?);
        } else if let Some(v) = strip_prefix_value(line, "libosdp:") {
            libosdp = Some(v == "present");
        }
    }

    let (ft, fb, fr, fo) = frames.ok_or(ParseError::MissingField("frames"))?;
    let (pn, pa, pmin, pmax) = polls.ok_or(ParseError::MissingField("polls"))?;
    Ok(StatusBlock {
        build: build.ok_or(ParseError::MissingField("build"))?,
        uptime: uptime.ok_or(ParseError::MissingField("uptime"))?,
        mode: mode.ok_or(ParseError::MissingField("mode"))?,
        frames_total: ft,
        frames_bad_crc: fb,
        frames_resync: fr,
        frames_oversize: fo,
        polls_n: pn,
        polls_avg_ms: pa,
        polls_min_ms: pmin,
        polls_max_ms: pmax,
        libosdp_present: libosdp.ok_or(ParseError::MissingField("libosdp"))?,
    })
}

fn strip_prefix_value<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    line.strip_prefix(key).map(|rest| rest.trim())
}

fn parse_frames(v: &str) -> Result<(u32, u32, u32, u32), ParseError> {
    let total = kv_u32(v, "total=")?;
    let bad = kv_u32(v, "bad_crc=")?;
    let res = kv_u32(v, "resync=")?;
    let over = kv_u32(v, "oversize=")?;
    Ok((total, bad, res, over))
}

fn parse_polls(v: &str) -> Result<(u32, u32, u32, u32), ParseError> {
    let n = kv_u32(v, "n=")?;
    let avg = kv_u32_with_suffix(v, "avg=", "ms")?;
    let min = kv_u32_with_suffix(v, "min=", "ms")?;
    let max = kv_u32_with_suffix(v, "max=", "ms")?;
    Ok((n, avg, min, max))
}

fn kv_u32(haystack: &str, key: &str) -> Result<u32, ParseError> {
    let after = haystack
        .find(key)
        .ok_or(ParseError::MissingField(static_key(key)))?;
    let tail = &haystack[after + key.len()..];
    let end = tail.find(' ').unwrap_or(tail.len());
    tail[..end]
        .parse()
        .map_err(|_| ParseError::BadInt { field: static_key(key), value: tail[..end].into() })
}

fn kv_u32_with_suffix(haystack: &str, key: &str, suffix: &str) -> Result<u32, ParseError> {
    let after = haystack
        .find(key)
        .ok_or(ParseError::MissingField(static_key(key)))?;
    let tail = &haystack[after + key.len()..];
    let end = tail.find(' ').unwrap_or(tail.len());
    let val = tail[..end].strip_suffix(suffix).unwrap_or(&tail[..end]);
    val.parse()
        .map_err(|_| ParseError::BadInt { field: static_key(key), value: val.into() })
}

// Map a borrowed key like "total=" to a `&'static str` for use in error variants.
// The key set is closed and small, so a match is fine.
fn static_key(k: &str) -> &'static str {
    match k {
        "total=" => "total",
        "bad_crc=" => "bad_crc",
        "resync=" => "resync",
        "oversize=" => "oversize",
        "n=" => "n",
        "avg=" => "avg",
        "min=" => "min",
        "max=" => "max",
        _ => "?",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_canonical_block() {
        let lines = [
            "build:    0.1.0-dev (a1b2c3d)",
            "uptime:   00:14:22",
            "mode:     sniff",
            "frames:   total=2841 bad_crc=2 resync=0 oversize=0",
            "polls:    n=2823 avg=10ms min=8ms max=21ms",
            "libosdp:  absent",
        ];
        let s = parse(&lines).unwrap();
        assert_eq!(s.build, "0.1.0-dev (a1b2c3d)");
        assert_eq!(s.uptime, "00:14:22");
        assert_eq!(s.mode, "sniff");
        assert_eq!(s.frames_total, 2841);
        assert_eq!(s.frames_bad_crc, 2);
        assert_eq!(s.polls_n, 2823);
        assert_eq!(s.polls_avg_ms, 10);
        assert_eq!(s.polls_min_ms, 8);
        assert_eq!(s.polls_max_ms, 21);
        assert!(!s.libosdp_present);
    }

    #[test]
    fn detects_libosdp_present() {
        let lines = [
            "build:    0.1.0",
            "uptime:   00:00:01",
            "mode:     idle",
            "frames:   total=0 bad_crc=0 resync=0 oversize=0",
            "polls:    n=0 avg=0ms min=0ms max=0ms",
            "libosdp:  present",
        ];
        assert!(parse(&lines).unwrap().libosdp_present);
    }

    #[test]
    fn errors_on_missing_field() {
        let lines = ["build:    x", "uptime:   y"];
        assert!(parse(&lines).is_err());
    }
}
