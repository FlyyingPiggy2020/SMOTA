// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
use crate::serial_port::SerialState;

mod serial_port;

#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

// 导出串口相关命令
use serial_port::{
    check_port_availability, close_serial_port, flush_buffer, get_connection_status,
    get_serial_ports, open_serial_port, receive_data, send_data,
};

/// 初始化串口状态
pub fn init_serial_state() -> SerialState {
    SerialState(std::sync::Mutex::new(serial_port::SerialConnection::default()))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            greet,
            // 串口命令
            get_serial_ports,
            open_serial_port,
            close_serial_port,
            get_connection_status,
            send_data,
            receive_data,
            flush_buffer,
            check_port_availability,
        ])
        .manage(init_serial_state())
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
