use mellon_proto::parse::status;

#[test]
fn parses_golden_status_block() {
    let golden = include_str!("golden/status.txt");
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let s = status::parse(&lines).expect("parse status block");
    assert_eq!(s.build, "0.1.0-dev (a1b2c3d)");
    assert_eq!(s.uptime, "00:14:22");
    assert_eq!(s.mode, "sniff");
    assert_eq!(s.frames_total, 2841);
    assert_eq!(s.frames_bad_crc, 2);
    assert_eq!(s.polls_n, 2823);
    assert_eq!(s.polls_avg_ms, 10);
    assert!(!s.libosdp_present);
}
