// Per-PD key state row parser.
//
// Format (src/ble/nus_console.c cmd_keys):
//   addr=0xNN sc=y/n scbk=captured/-

use crate::ParseError;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct KeyRow {
    pub addr: u8,
    pub sc_active: bool,
    pub scbk_known: bool,
}

pub fn parse(line: &str) -> Result<KeyRow, ParseError> {
    let rest = line
        .strip_prefix("addr=0x")
        .ok_or_else(|| ParseError::Malformed(line.to_string()))?;
    let (addr_s, rest) = rest
        .split_once(' ')
        .ok_or_else(|| ParseError::Malformed(line.to_string()))?;
    let addr = u8::from_str_radix(addr_s, 16)
        .map_err(|_| ParseError::BadInt { field: "addr", value: addr_s.into() })?;

    let rest = rest
        .strip_prefix("sc=")
        .ok_or(ParseError::MissingField("sc"))?;
    let (sc_s, rest) = rest
        .split_once(' ')
        .ok_or_else(|| ParseError::Malformed(line.to_string()))?;
    let sc_active = match sc_s {
        "y" => true,
        "n" => false,
        _ => return Err(ParseError::Malformed(line.to_string())),
    };

    let scbk_s = rest
        .strip_prefix("scbk=")
        .ok_or(ParseError::MissingField("scbk"))?;
    let scbk_known = match scbk_s.trim() {
        "captured" => true,
        "-" => false,
        _ => return Err(ParseError::Malformed(line.to_string())),
    };

    Ok(KeyRow { addr, sc_active, scbk_known })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_captured() {
        let r = parse("addr=0x00 sc=y scbk=captured").unwrap();
        assert_eq!(r, KeyRow { addr: 0, sc_active: true, scbk_known: true });
    }

    #[test]
    fn parses_unknown() {
        let r = parse("addr=0x7f sc=n scbk=-").unwrap();
        assert_eq!(r, KeyRow { addr: 0x7f, sc_active: false, scbk_known: false });
    }

    #[test]
    fn rejects_garbage() {
        assert!(parse("nope").is_err());
        assert!(parse("addr=0x00 sc=maybe scbk=-").is_err());
    }
}
