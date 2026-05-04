// mellon-proto — NUS line-protocol parser for the Mellon firmware.
//
// Pure-Rust, no I/O. The Tauri shell feeds raw NUS TX bytes into LineFramer
// and consumes typed responses; the same logic is host-testable with cargo
// test against fixtures lifted from docs/ble_protocol.md.

pub mod cmd;
pub mod framer;
pub mod parse;

pub use cmd::Mode;
pub use framer::{Line, LineFramer};
pub use parse::async_event::AsyncEvent;
pub use parse::dump::{Direction, DumpRow};
pub use parse::keys::KeyRow;
pub use parse::status::StatusBlock;

#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("malformed line: {0}")]
    Malformed(String),
    #[error("missing field: {0}")]
    MissingField(&'static str),
    #[error("invalid integer in field {field}: {value}")]
    BadInt { field: &'static str, value: String },
}
