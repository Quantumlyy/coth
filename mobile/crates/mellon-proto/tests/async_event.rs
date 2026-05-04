use mellon_proto::parse::async_event::{self, AsyncEvent};

#[test]
fn classifies_known_async_events() {
    assert_eq!(
        async_event::parse("KEYSET seen at t=00:00:01.000 addr=0x01 scbk=AABB"),
        AsyncEvent::KeysetSeen {
            t: "00:00:01.000".into(),
            addr: 1,
            scbk: "AABB".into()
        }
    );
    assert_eq!(
        async_event::parse("MATCH after 42 candidates: scbk=DEADBEEF"),
        AsyncEvent::Match { candidates: 42, detail: "scbk=DEADBEEF".into() }
    );
    assert_eq!(
        async_event::parse("ARMED expired"),
        AsyncEvent::ArmedExpired
    );
    assert!(matches!(
        async_event::parse("brand new event nobody knows about"),
        AsyncEvent::Unknown { .. }
    ));
}
