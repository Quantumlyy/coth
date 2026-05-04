// Command builders. Each returns a String terminated with `\n`, ready to
// hand to `bt_nus_send` on the firmware side. All grammar comes from
// docs/ble_protocol.md and the dispatcher in src/ble/nus_console.c.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Mode {
    Idle,
    Sniff,
    KeysetWatch,
    WeakKeys,
}

impl Mode {
    pub fn as_wire(self) -> &'static str {
        match self {
            Mode::Idle => "idle",
            Mode::Sniff => "sniff",
            Mode::KeysetWatch => "keyset_watch",
            Mode::WeakKeys => "weak_keys",
        }
    }

    pub fn from_wire(s: &str) -> Option<Self> {
        match s {
            "idle" => Some(Mode::Idle),
            "sniff" => Some(Mode::Sniff),
            "keyset_watch" => Some(Mode::KeysetWatch),
            "weak_keys" => Some(Mode::WeakKeys),
            _ => None,
        }
    }
}

pub fn help() -> String {
    "help\n".into()
}

pub fn status() -> String {
    "status\n".into()
}

pub fn mode(m: Mode) -> String {
    format!("mode {}\n", m.as_wire())
}

pub fn dump(n: u32) -> String {
    format!("dump {}\n", n)
}

pub fn preserved() -> String {
    "preserved\n".into()
}

pub fn capture(on: bool) -> String {
    format!("capture {}\n", if on { "on" } else { "off" })
}

pub fn wipe() -> String {
    "wipe\n".into()
}

pub fn keys() -> String {
    "keys\n".into()
}

pub fn attack_weak_keys() -> String {
    "attack weak_keys\n".into()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mode_round_trip() {
        for m in [Mode::Idle, Mode::Sniff, Mode::KeysetWatch, Mode::WeakKeys] {
            assert_eq!(Mode::from_wire(m.as_wire()), Some(m));
        }
        assert_eq!(Mode::from_wire("unknown"), None);
    }

    #[test]
    fn builders_terminate_with_newline() {
        assert_eq!(help(), "help\n");
        assert_eq!(status(), "status\n");
        assert_eq!(mode(Mode::KeysetWatch), "mode keyset_watch\n");
        assert_eq!(dump(10), "dump 10\n");
        assert_eq!(preserved(), "preserved\n");
        assert_eq!(capture(true), "capture on\n");
        assert_eq!(capture(false), "capture off\n");
        assert_eq!(wipe(), "wipe\n");
        assert_eq!(keys(), "keys\n");
        assert_eq!(attack_weak_keys(), "attack weak_keys\n");
    }
}
