// Tauri commands that delegate to mellon-proto's pure-Rust parsers. Keeps
// the BLE I/O on the JS side (where the plugin lives) while the heavy
// parsing stays in the host-testable crate.

use mellon_proto::parse::{async_event, dump, keys, status};
use mellon_proto::{AsyncEvent, DumpRow, KeyRow, StatusBlock};

#[tauri::command]
pub fn parse_dump_row(line: String) -> Result<DumpRow, String> {
    dump::parse(&line).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn parse_status_block(lines: Vec<String>) -> Result<StatusBlock, String> {
    let refs: Vec<&str> = lines.iter().map(String::as_str).collect();
    status::parse(&refs).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn parse_key_row(line: String) -> Result<KeyRow, String> {
    keys::parse(&line).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn parse_async_event(body: String) -> AsyncEvent {
    async_event::parse(&body)
}
