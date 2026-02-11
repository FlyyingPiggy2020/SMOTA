/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : serial_commands.rs
 * @Author       : lxf
 * @Date         : 2025-02-11 10:00:00
 * @LastEditors  : lxf
 * @LastEditTime : 2025-02-11 10:00:00
 * @Brief        : 串口命令模块，封装 Tauri 命令
 */
/*---------- includes ----------*/
use serde::Serialize;
/*---------- macro ----------*/
/*---------- type define ----------*/

/// 串口信息（传递给前端）
#[derive(Serialize, Clone)]
pub struct PortInfo {
    pub name: String,
    pub port_type: String,
}

/// Tauri 命令结果类型
type Result<T> = std::result::Result<T, String>;

/*---------- variable prototype ----------*/
/*---------- function prototype ----------*/

/**
 * @brief 将 SerialPortType 转换为字符串描述
 */
fn port_type_to_string(port_type: &serialport::SerialPortType) -> String {
    match port_type {
        serialport::SerialPortType::UsbPort(info) => {
            if let Some(manufacturer) = &info.manufacturer {
                manufacturer.clone()
            } else {
                format!("USB (VID={}, PID={})", info.vid, info.pid)
            }
        }
        serialport::SerialPortType::PciPort => "PCI".to_string(),
        serialport::SerialPortType::BluetoothPort => "Bluetooth".to_string(),
        serialport::SerialPortType::Unknown => "Unknown".to_string(),
    }
}

/**
 * @brief 打印串口详细信息
 */
fn print_port_detail(name: &String, port_type: &serialport::SerialPortType) {
    match port_type {
        serialport::SerialPortType::UsbPort(info) => {
            println!(
                "  {}: USB - VID={:?}, PID={:?}, Serial={:?}, Manufacturer={:?}, Product={:?}",
                name, info.vid, info.pid, info.serial_number, info.manufacturer, info.product
            );
        }
        serialport::SerialPortType::PciPort => {
            println!("  {}: PCI", name);
        }
        serialport::SerialPortType::BluetoothPort => {
            println!("  {}: Bluetooth", name);
        }
        serialport::SerialPortType::Unknown => {
            println!("  {}: Unknown", name);
        }
    }
}

/**
 * @brief 列出所有可用串口端口
 * @return Vec<PortInfo> 串口信息列表
 */
#[tauri::command]
pub fn list_ports() -> Result<Vec<PortInfo>> {
    println!("=== Available Serial Ports ===");

    let ports: Vec<PortInfo> = serialport::available_ports()
        .map_err(|e| e.to_string())?
        .into_iter()
        .map(|p| {
            // 打印串口详细信息（调试用）
            print_port_detail(&p.port_name, &p.port_type);

            PortInfo {
                name: p.port_name,
                port_type: port_type_to_string(&p.port_type),
            }
        })
        .collect();

    println!("Total ports found: {}\n", ports.len());
    Ok(ports)
}

/*---------- variable ----------*/
/*---------- function ----------*/

/*---------- end of file ----------*/
