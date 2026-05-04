mod commands;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_blec::init())
        .invoke_handler(tauri::generate_handler![
            commands::parse_dump_row,
            commands::parse_status_block,
            commands::parse_key_row,
            commands::parse_async_event,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
