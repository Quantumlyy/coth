use mellon_proto::{Line, LineFramer};

#[test]
fn full_dump_session_through_framer() {
    let mut f = LineFramer::new();
    let stream = b"\
[t=00:01:23.456] dir=cp2pd addr=0x00 cmd=0x60 len=8 flags=0  53 00 08 00 00 60 ?? ??\n\
[t=00:01:23.470] dir=pd2cp addr=0x00 cmd=0x40 len=6 flags=0  53 80 06 00 40 7C\n\
ok 2 records\n";
    let lines = f.push(stream);
    assert_eq!(lines.len(), 3);
    assert!(matches!(lines[0], Line::Sync { .. }));
    assert!(matches!(lines[1], Line::Sync { .. }));
    assert_eq!(lines[2], Line::Ok { detail: "2 records".into() });
}

#[test]
fn async_during_command_response() {
    let mut f = LineFramer::new();
    let stream = b"build:    0.1.0\n* KEYSET seen at t=00:00:01.000 addr=0x01 scbk=A\nuptime:   00:00:01\n";
    let lines = f.push(stream);
    assert!(matches!(lines[0], Line::Sync { .. }));
    assert!(matches!(lines[1], Line::Async { .. }));
    assert!(matches!(lines[2], Line::Sync { .. }));
}
