// Pins the JSON wire format that the Tauri shell hands to the TS layer.
// The TS types in mobile/src/types.ts assume these exact strings.

use mellon_proto::parse::async_event::AsyncEvent;
use mellon_proto::{Direction, DumpRow, Line, Mode};

fn dump_row(dir: Direction) -> DumpRow {
    DumpRow {
        t_ms: 0,
        dir,
        addr: 0,
        cmd: 0,
        len: 0,
        flags: 0,
        bytes_hex: String::new(),
    }
}

#[test]
fn direction_wire_format() {
    let cases = [
        (Direction::Cp2Pd, "cp2pd"),
        (Direction::Pd2Cp, "pd2cp"),
        (Direction::Unknown, "unknown"),
    ];
    for (dir, expected) in cases {
        let v = serde_json::to_value(dump_row(dir)).unwrap();
        assert_eq!(v["dir"], expected, "{:?}", dir);
    }
}

#[test]
fn mode_wire_format() {
    for (m, expected) in [
        (Mode::Idle, "idle"),
        (Mode::Sniff, "sniff"),
        (Mode::KeysetWatch, "keyset_watch"),
        (Mode::WeakKeys, "weak_keys"),
    ] {
        let v = serde_json::to_value(m).unwrap();
        assert_eq!(v, expected, "{:?}", m);
    }
}

#[test]
fn async_event_wire_format() {
    let v = serde_json::to_value(AsyncEvent::ArmedExpired).unwrap();
    assert_eq!(v["kind"], "armed_expired");

    let v = serde_json::to_value(AsyncEvent::KeysetSeen {
        t: "00:00:01.000".into(),
        addr: 1,
        scbk: "AB".into(),
    })
    .unwrap();
    assert_eq!(v["kind"], "keyset_seen");

    let v = serde_json::to_value(AsyncEvent::Match {
        candidates: 5,
        detail: "x".into(),
    })
    .unwrap();
    assert_eq!(v["kind"], "match");
}

#[test]
fn line_wire_format() {
    for (line, expected) in [
        (Line::Sync { text: "x".into() }, "sync"),
        (Line::Async { text: "x".into() }, "async"),
        (Line::Ok { detail: "".into() }, "ok"),
        (Line::Err { reason: "x".into() }, "err"),
    ] {
        let v = serde_json::to_value(&line).unwrap();
        assert_eq!(v["kind"], expected, "{:?}", line);
    }
}
