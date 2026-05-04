// Asynchronous notifications (lines that started with `*` on the wire).
//
// The framer has already stripped the leading `*` and surrounding space, so
// these parsers see only the body.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum AsyncEvent {
    KeysetSeen { t: String, addr: u8, scbk: String },
    Match { candidates: u32, detail: String },
    ArmedExpired,
    Unknown { text: String },
}

pub fn parse(body: &str) -> AsyncEvent {
    if let Some(rest) = body.strip_prefix("KEYSET seen at t=") {
        if let Some((t, after)) = rest.split_once(' ') {
            if let Some(after) = after.strip_prefix("addr=0x") {
                if let Some((addr_s, after)) = after.split_once(' ') {
                    if let Some(scbk) = after.strip_prefix("scbk=") {
                        if let Ok(addr) = u8::from_str_radix(addr_s, 16) {
                            return AsyncEvent::KeysetSeen {
                                t: t.to_string(),
                                addr,
                                scbk: scbk.to_string(),
                            };
                        }
                    }
                }
            }
        }
    }
    if let Some(rest) = body.strip_prefix("MATCH after ") {
        if let Some((cand_s, detail)) = rest.split_once(" candidates: ") {
            if let Ok(candidates) = cand_s.parse() {
                return AsyncEvent::Match { candidates, detail: detail.to_string() };
            }
        }
    }
    if body == "ARMED expired" {
        return AsyncEvent::ArmedExpired;
    }
    AsyncEvent::Unknown { text: body.to_string() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_keyset_seen() {
        let e = parse("KEYSET seen at t=00:01:23.456 addr=0x01 scbk=AABBCCDD…");
        assert_eq!(
            e,
            AsyncEvent::KeysetSeen {
                t: "00:01:23.456".into(),
                addr: 1,
                scbk: "AABBCCDD…".into(),
            }
        );
    }

    #[test]
    fn parses_match() {
        let e = parse("MATCH after 137 candidates: scbk=DEADBEEF…");
        assert_eq!(
            e,
            AsyncEvent::Match { candidates: 137, detail: "scbk=DEADBEEF…".into() }
        );
    }

    #[test]
    fn parses_armed_expired() {
        assert_eq!(parse("ARMED expired"), AsyncEvent::ArmedExpired);
    }

    #[test]
    fn falls_back_to_unknown() {
        assert_eq!(
            parse("totally novel event"),
            AsyncEvent::Unknown { text: "totally novel event".into() }
        );
    }
}
