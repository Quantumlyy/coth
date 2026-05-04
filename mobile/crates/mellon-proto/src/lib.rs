// mellon-proto — NUS line-protocol parser for the Mellon firmware.
//
// Pure-Rust, no I/O. The Tauri shell feeds raw NUS TX bytes into LineFramer
// and consumes typed responses; the same logic is host-testable with cargo
// test against fixtures lifted from docs/ble_protocol.md.
//
// Layers are filled in by the next commit; this initial commit is the empty
// crate so the workspace builds.
