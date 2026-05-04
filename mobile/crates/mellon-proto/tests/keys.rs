use mellon_proto::parse::keys;

#[test]
fn parses_golden_keys_fixtures() {
    let golden = include_str!("golden/keys.txt");
    let rows: Vec<_> = golden
        .lines()
        .filter(|l| !l.is_empty())
        .map(|l| keys::parse(l).expect("parse keys line"))
        .collect();
    assert_eq!(rows.len(), 3);
    assert_eq!(rows[0].addr, 0x00);
    assert!(rows[0].sc_active);
    assert!(rows[0].scbk_known);
    assert_eq!(rows[1].addr, 0x01);
    assert!(!rows[1].sc_active);
    assert!(!rows[1].scbk_known);
    assert_eq!(rows[2].addr, 0x7f);
}
