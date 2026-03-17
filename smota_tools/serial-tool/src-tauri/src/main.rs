/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : main.rs
 * @Author       : lxf
 * @Date         : 2025-02-11 10:00:00
 * @LastEditors  : lxf
 * @LastEditTime : 2025-02-11 10:00:00
 * @Brief        : Tauri 应用入口
 */
// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;

use commands::serial_commands::SerialStateState;

fn main() {
    tauri::Builder::default()
        .manage(SerialStateState::default())
        .invoke_handler(tauri::generate_handler![
            commands::serial_commands::list_ports,
            commands::serial_commands::open_serial,
            commands::serial_commands::close_serial,
            commands::serial_commands::check_connection_status,
            commands::serial_commands::send_data,
            commands::serial_commands::receive_data,
            commands::serial_commands::export_logs
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
