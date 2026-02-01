/*!
 * @file serial_port.rs
 * @brief 串口通信模块 - 提供串口列表获取、打开、关闭、发送、接收功能
 * @author SMOTA Tool
 * @date 2025-01-31
 */

use serialport::{available_ports, SerialPort};
use std::sync::Mutex;
use tauri::State;

/// 串口配置参数
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct SerialConfig {
    /// 波特率
    pub baud_rate: u32,
    /// 数据位 (5, 6, 7, 8)
    pub data_bits: u8,
    /// 停止位 (1, 2)
    pub stop_bits: u8,
    /// 校验位 (none, odd, even)
    pub parity: String,
    /// 流控 (none, rts/cts, dtr/dsr)
    pub flow_control: String,
    /// 超时时间（毫秒）
    pub timeout: u64,
}

impl Default for SerialConfig {
    fn default() -> Self {
        Self {
            baud_rate: 115200,
            data_bits: 8,
            stop_bits: 1,
            parity: "none".to_string(),
            flow_control: "none".to_string(),
            timeout: 1000,
        }
    }
}

/// 串口信息结构
#[derive(Debug, Clone, serde::Serialize)]
pub struct SerialPortInfo {
    /// 端口名称（如 COM3）
    pub port_name: String,
    /// 设备类型
    pub port_type: String,
    /// 是否已打开
    pub is_open: bool,
}

/// 获取设备类型的友好名称
fn get_port_type_name(port_type: &serialport::SerialPortType) -> String {
    match port_type {
        serialport::SerialPortType::UsbPort(info) => {
            let manufacturer = info.manufacturer.as_deref().unwrap_or("Unknown");
            let product = info.product.as_deref().unwrap_or("Unknown");
            format!("USB - {} - {}", manufacturer, product)
        }
        serialport::SerialPortType::PciPort => "PCI".to_string(),
        serialport::SerialPortType::BluetoothPort => "Bluetooth".to_string(),
        serialport::SerialPortType::Unknown => "Unknown".to_string(),
    }
}

/// 串口连接状态
pub struct SerialConnection {
    /// 串口句柄
    pub port: Option<Box<dyn SerialPort>>,
    /// 当前配置
    pub config: SerialConfig,
    /// 是否已连接
    pub is_connected: bool,
    /// 端口名称
    pub port_name: String,
}

impl Default for SerialConnection {
    fn default() -> Self {
        Self {
            port: None,
            config: SerialConfig::default(),
            is_connected: false,
            port_name: String::new(),
        }
    }
}

/// 全局串口状态
pub struct SerialState(pub Mutex<SerialConnection>);

/// 获取系统可用串口列表
///
/// @return 串口信息列表
/// @note 该命令可在前端通过 invoke 调用
#[tauri::command]
pub fn get_serial_ports(state: State<SerialState>) -> Result<Vec<SerialPortInfo>, String> {
    eprintln!("[Serial] 开始获取串口列表...");
    let mut result = Vec::new();
    
    match available_ports() {
        Ok(ports) => {
            eprintln!("[Serial] 发现 {} 个串口", ports.len());
            for port in ports {
                let port_name = port.port_name.clone();  // 克隆避免 move
                let port_type_name = get_port_type_name(&port.port_type);
                eprintln!("[Serial] 找到串口: {} ({})", port_name, port_type_name);
                let port_info = SerialPortInfo {
                    port_name: port_name.clone(),
                    port_type: port_type_name,
                    is_open: {
                        // 检查当前连接的串口
                        let guard = state.0.lock().map_err(|e| e.to_string())?;
                        guard.is_connected && guard.port_name == port_name
                    },
                };
                result.push(port_info);
            }
            Ok(result)
        }
        Err(e) => {
            eprintln!("[Serial] 获取串口列表失败: {}", e);
            Err(format!("获取串口列表失败: {}", e))
        }
    }
}

/// 打开串口
///
/// @param port_name 端口名称（如 "COM3"）
/// @param config 串口配置
/// @return 是否打开成功
/// @note 调用前应先调用 get_serial_ports 获取可用串口列表
#[tauri::command]
pub fn open_serial_port(
    port_name: String,
    config: SerialConfig,
    state: State<SerialState>,
) -> Result<bool, String> {
    // 检查串口是否已打开
    {
        let guard = state.0.lock().map_err(|e| e.to_string())?;
        if guard.is_connected {
            return Err("串口已打开，请先关闭".to_string());
        }
    }

    // 解析配置
    let data_bits = match config.data_bits {
        5 => serialport::DataBits::Five,
        6 => serialport::DataBits::Six,
        7 => serialport::DataBits::Seven,
        _ => serialport::DataBits::Eight,
    };

    let stop_bits = match config.stop_bits {
        1 => serialport::StopBits::One,
        2 => serialport::StopBits::Two,
        _ => serialport::StopBits::One,
    };

    let parity = match config.parity.to_lowercase().as_str() {
        "odd" => serialport::Parity::Odd,
        "even" => serialport::Parity::Even,
        _ => serialport::Parity::None,
    };

    let flow_control = match config.flow_control.to_lowercase().as_str() {
        // serialport 4.x: FlowControl 是简单的枚举
        // 目前只实现 None，RTS/CTS 可能在某些平台不支持
        _ => serialport::FlowControl::None,
    };

    // 打开串口
    match serialport::new(&port_name, config.baud_rate)
        .data_bits(data_bits)
        .stop_bits(stop_bits)
        .parity(parity)
        .flow_control(flow_control)
        .timeout(std::time::Duration::from_millis(config.timeout))
        .open()
    {
        Ok(serial_port) => {
            let mut guard = state.0.lock().map_err(|e| e.to_string())?;
            guard.port = Some(serial_port);
            guard.config = config;
            guard.is_connected = true;
            guard.port_name = port_name.clone();
            Ok(true)
        }
        Err(e) => Err(format!("打开串口失败: {}", e)),
    }
}

/// 关闭串口
///
/// @return 是否关闭成功
/// @note 关闭后释放所有资源
#[tauri::command]
pub fn close_serial_port(state: State<SerialState>) -> Result<bool, String> {
    let mut guard = state.0.lock().map_err(|e| e.to_string())?;

    if !guard.is_connected {
        return Ok(true); // 已经关闭，无需重复关闭
    }

    // 关闭串口（drop 会自动关闭）
    guard.port = None;
    guard.is_connected = false;
    guard.port_name.clear();

    Ok(true)
}

/// 获取串口连接状态
///
/// @return 是否已连接、当前端口名、当前配置
#[tauri::command]
pub fn get_connection_status(
    state: State<SerialState>,
) -> Result<serde_json::Value, String> {
    let guard = state.0.lock().map_err(|e| e.to_string())?;

    Ok(serde_json::json!({
        "is_connected": guard.is_connected,
        "port_name": guard.port_name,
        "config": {
            "baud_rate": guard.config.baud_rate,
            "data_bits": guard.config.data_bits,
            "stop_bits": guard.config.stop_bits,
            "parity": guard.config.parity,
            "flow_control": guard.config.flow_control,
            "timeout": guard.config.timeout,
        }
    }))
}

/// 发送数据
///
/// @param data 要发送的数据（UTF-8 字符串）
/// @return 发送的字节数
/// @note 数据会按原样发送，不会自动添加换行符
#[tauri::command]
pub fn send_data(data: String, state: State<SerialState>) -> Result<usize, String> {
    let mut guard = state.0.lock().map_err(|e| e.to_string())?;

    if !guard.is_connected {
        return Err("串口未连接".to_string());
    }

    if guard.port.is_none() {
        return Err("串口句柄不存在".to_string());
    }

    let bytes = data.as_bytes();
    match guard.port.as_mut().unwrap().write(bytes) {
        Ok(n) => Ok(n),
        Err(e) => Err(format!("发送数据失败: {}", e)),
    }
}

/// 接收数据
///
/// @param max_bytes 最大读取字节数（默认 1024）
/// @return 接收到的数据（Base64 编码）
/// @note 如果没有数据可读，返回空字符串
/// @note 返回 Base64 编码的数据，前端负责解码显示
#[tauri::command]
pub fn receive_data(
    max_bytes: Option<u32>,
    state: State<SerialState>,
) -> Result<String, String> {
    let mut guard = state.0.lock().map_err(|e| e.to_string())?;

    if !guard.is_connected {
        return Err("串口未连接".to_string());
    }

    if guard.port.is_none() {
        return Err("串口句柄不存在".to_string());
    }

    let mut buffer = vec![0u8; max_bytes.unwrap_or(1024) as usize];

    match guard.port.as_mut().unwrap().read(buffer.as_mut_slice()) {
        Ok(n) => {
            if n == 0 {
                Ok(String::new())
            } else {
                // 只取有效数据
                buffer.truncate(n);
                // 返回 Base64 编码，前端可选择显示为 ASCII 或 Hex
                Ok(base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &buffer))
            }
        }
        Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {
            Ok(String::new()) // 超时，返回空
        }
        Err(e) => Err(format!("接收数据失败: {}", e)),
    }
}

/// 刷新接收缓冲区
///
/// @return 是否成功
/// @note 用于清除串口缓冲区中残留的数据
#[tauri::command]
pub fn flush_buffer(state: State<SerialState>) -> Result<bool, String> {
    let mut guard = state.0.lock().map_err(|e| e.to_string())?;

    if !guard.is_connected || guard.port.is_none() {
        return Ok(false);
    }

    match guard.port.as_mut().unwrap().clear(serialport::ClearBuffer::All) {
        Ok(()) => Ok(true),
        Err(e) => Err(format!("刷新缓冲区失败: {}", e)),
    }
}

/// 检查串口连接状态（用于热插拔检测）
///
/// @return 串口是否仍然可用
/// @note 如果当前连接的串口不再可用，返回 false
#[tauri::command]
pub fn check_port_availability(state: State<SerialState>) -> Result<bool, String> {
    let guard = state.0.lock().map_err(|e| e.to_string())?;

    if !guard.is_connected {
        return Ok(true); // 未连接，认为可用
    }

    // 检查端口是否仍在可用列表中
    match available_ports() {
        Ok(ports) => {
            let port_exists = ports.iter().any(|p| p.port_name == guard.port_name);
            Ok(port_exists)
        }
        Err(_) => Ok(true), // 无法获取列表，保守起见返回可用
    }
}
