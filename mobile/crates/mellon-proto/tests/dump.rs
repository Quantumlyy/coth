use mellon_proto::parse::dump;
use mellon_proto::Direction;

#[test]
fn parses_golden_dump_fixtures() {
    let golden = include_str!("golden/dump.txt");
    let rows: Vec<_> = golden
        .lines()
        .filter(|l| !l.is_empty())
        .map(|l| dump::parse(l).expect("parse dump line"))
        .collect();
    assert_eq!(rows.len(), 4);

    assert_eq!(rows[0].dir, Direction::Cp2Pd);
    assert_eq!(rows[0].t_ms, 83_456);
    assert_eq!(rows[0].cmd, 0x60);
    assert!(!rows[0].encrypted());

    assert_eq!(rows[1].dir, Direction::Pd2Cp);
    assert_eq!(rows[1].cmd, 0x40);

    assert_eq!(rows[2].cmd, 0x76);
    assert!(rows[2].encrypted(), "flags=1 means encrypted");

    assert_eq!(rows[3].dir, Direction::Unknown);
    assert!(rows[3].oversize(), "flags=16 means oversize");
}
