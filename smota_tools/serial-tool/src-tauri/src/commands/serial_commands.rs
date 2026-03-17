/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : serial_commands.rs
 * @Author       : lxf
 * @Date         : 2025-02-13 10:00:00
 * @LastEditors  : lxf
 * @LastEditTime : 2025-02-13 10:00:00
 * @Brief        : 串口命令模块，封装 Tauri 命令
 */
/*---------- includes ----------*/
use crossbeam::channel;
use serde::{Serialize, Deserialize};
use std::cell::RefCell;
use std::fmt;
use std::io::{self, Read, Write};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use tauri::{AppHandle, Emitter};
/*---------- macro ----------*/
/*---------- type define ----------*/

/// 发送命令枚举
pub(crate) enum SendCommand {
    Send(String),
}

/// 串口错误类型
#[derive(Debug, PartialEq)]
pub enum SerialError {
    ConnectionFailed(String),
    WriteError(String),
    ReadError(String),
    Timeout,
    PortNotOpen,
    InvalidUtf8(Vec<u8>),
}

impl fmt::Display for SerialError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SerialError::ConnectionFailed(msg) => write!(f, "连接失败: {}", msg),
            SerialError::WriteError(msg) => write!(f, "写入错误: {}", msg),
            SerialError::ReadError(msg) => write!(f, "读取错误: {}", msg),
            SerialError::Timeout => write!(f, "操作超时"),
            SerialError::PortNotOpen => write!(f, "串口未打开"),
            SerialError::InvalidUtf8(data) => write!(f, "无效的UTF-8数据: {:02x?}", data),
        }
    }
}

impl std::error::Error for SerialError {}

impl From<SerialError> for String {
    fn from(e: SerialError) -> Self {
        e.to_string()
    }
}

/// 串口状态（使用 Tauri state 管理）
#[derive(Default)]
pub struct SerialState {
    /// 连接状态
    pub is_connected: bool,
    /// 当前连接的串口名称
    pub port_name: String,
    /// 波特率
    pub baud_rate: u32,
    /// 数据位
    pub data_bits: u8,
    /// 停止位
    pub stop_bits: u8,
    /// 校验位
    pub parity: String,
    /// 接收线程运行标志
    pub receive_running: Arc<Mutex<bool>>,
    /// 发送通道（用于向接收线程发送数据）
    pub send_tx: Option<channel::Sender<SendCommand>>,
}

/// 包装 SerialState 以实现 Send + Sync（使用 RefCell 提供内部可变性）
pub struct SerialStateState(pub RefCell<SerialState>);

unsafe impl Send for SerialStateState {}
unsafe impl Sync for SerialStateState {}

impl Default for SerialStateState {
    fn default() -> Self {
        SerialStateState(RefCell::new(SerialState {
            is_connected: false,
            port_name: String::new(),
            baud_rate: 0,
            data_bits: 8,
            stop_bits: 1,
            parity: String::new(),
            receive_running: Arc::new(Mutex::new(false)),
            send_tx: None,
        }))
    }
}

/// 串口连接状态
#[derive(Serialize, Clone, Debug)]
pub enum ConnectionStatus {
    Disconnected,
    Connected,
}

/// 串口配置参数（传递给前端）
#[derive(Serialize, Clone, Debug)]
pub struct SerialConfig {
    pub port: String,
    pub baud_rate: u32,
    pub data_bits: u8,
    pub stop_bits: u8,
    pub parity: String,
}

/// 串口连接信息（传递给前端）
#[derive(Serialize, Clone, Debug)]
pub struct ConnectionInfo {
    pub status: ConnectionStatus,
    pub config: Option<SerialConfig>,
    pub message: String,
}

/// 串口信息（传递给前端）
#[derive(Serialize, Clone)]
pub struct PortInfo {
    pub name: String,
    pub port_type: String,
}

/// 日志项（从前端接收）
#[derive(Serialize, Deserialize, Clone)]
pub struct LogItem {
    pub content: String,
    pub raw: String,
    pub time: String,
    pub timestamp: i64,
    pub type_: String,
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

/**
 * @brief 打开串口连接
 * @param port 串口名称
 * @param baud_rate 波特率
 * @param data_bits 数据位
 * @param stop_bits 停止位
 * @param parity 校验位
 * @param app Tauri 应用句柄
 * @return ConnectionInfo 连接信息
 */
#[tauri::command]
pub fn open_serial(
    port: String,
    baud_rate: u32,
    data_bits: u8,
    stop_bits: u8,
    parity: String,
    app: tauri::AppHandle,
    state: tauri::State<'_, SerialStateState>,
) -> Result<ConnectionInfo> {
    let data_bits_enum = match data_bits {
        5 => serialport::DataBits::Five,
        6 => serialport::DataBits::Six,
        7 => serialport::DataBits::Seven,
        _ => serialport::DataBits::Eight,
    };

    let stop_bits_enum = match stop_bits {
        2 => serialport::StopBits::Two,
        _ => serialport::StopBits::One,
    };

    let parity_enum = match parity.as_str() {
        "odd" => serialport::Parity::Odd,
        "even" => serialport::Parity::Even,
        _ => serialport::Parity::None,
    };

    let settings = serialport::new(&port, baud_rate)
        .data_bits(data_bits_enum)
        .stop_bits(stop_bits_enum)
        .parity(parity_enum);

    match settings.open() {
        Ok(serial_port) => {
            let mut state_guard = state.0.borrow_mut();

            // 创建发送通道
            let (send_tx, send_rx) = channel::unbounded::<SendCommand>();

            // 保存配置信息到状态
            state_guard.port_name = port.clone();
            state_guard.baud_rate = baud_rate;
            state_guard.data_bits = data_bits;
            state_guard.stop_bits = stop_bits;
            state_guard.parity = parity.clone();
            state_guard.receive_running = Arc::new(Mutex::new(true));
            state_guard.send_tx = Some(send_tx);
            state_guard.is_connected = true;

            // 克隆运行标志供线程使用
            let running_flag = state_guard.receive_running.clone();

            // 将 serial_port 和通道接收端的所有权转移给接收线程
            let app_handle = app.clone();

            // 启动接收线程
            thread::spawn(move || {
                serial_receive_thread(serial_port, app_handle, running_flag, send_rx);
            });

            println!("成功打开串口: {}", port);

            Ok(ConnectionInfo {
                status: ConnectionStatus::Connected,
                config: Some(SerialConfig {
                    port: port.clone(),
                    baud_rate,
                    data_bits,
                    stop_bits,
                    parity,
                }),
                message: String::new(),
            })
        }
        Err(e) => {
            println!("打开串口失败: {} - {}", port, e);
            Err(format!("打开串口失败: {} - {}", port, e))
        }
    }
}

/**
 * @brief 关闭串口连接
 * @return ConnectionInfo 连接信息
 */
#[tauri::command]
pub fn close_serial(state: tauri::State<'_, SerialStateState>) -> ConnectionInfo {
    // 停止接收线程
    {
        let mut state_guard = state.0.borrow_mut();
        let running_flag = state_guard.receive_running.clone();
        {
            let mut running = running_flag.lock().unwrap();
            *running = false;
        }
        state_guard.is_connected = false;
        state_guard.port_name.clear();
    }

    println!("串口已关闭");

    ConnectionInfo {
        status: ConnectionStatus::Disconnected,
        config: None,
        message: String::new(),
    }
}

/**
 * @brief 检查串口连接状态
 * @return ConnectionInfo 连接信息
 */
#[tauri::command]
pub fn check_connection_status(state: tauri::State<'_, SerialStateState>) -> ConnectionInfo {
    let state_guard = state.0.borrow();
    let connected = state_guard.is_connected;

    if connected {
        ConnectionInfo {
            status: ConnectionStatus::Connected,
            config: None,
            message: "已连接".to_string(),
        }
    } else {
        ConnectionInfo {
            status: ConnectionStatus::Disconnected,
            config: None,
            message: "未连接".to_string(),
        }
    }
}

/**
 * @brief 发送数据
 * @param data 要发送的字符串
 * @return Result<String> 发送结果
 */
#[tauri::command]
pub fn send_data(data: String, state: tauri::State<'_, SerialStateState>) -> Result<String> {
    let state_guard = state.0.borrow();

    // 检查连接状态
    if !state_guard.is_connected {
        return Err("串口未连接".to_string());
    }

    // 通过通道发送数据
    if let Some(ref send_tx) = state_guard.send_tx {
        if send_tx.send(SendCommand::Send(data)).is_ok() {
            println!("发送成功");
            return Ok(String::new());
        }
    }

    Err("发送失败".to_string())
}

/**
 * @brief 串口接收线程函数
 * @param port 串口实例（拥有所有权）
 * @param app_handle Tauri 应用句柄（用于发送事件）
 * @param running_flag 运行标志（Arc<Mutex<bool>>）
 * @param send_rx 发送通道接收端
 */
fn serial_receive_thread(
    mut port: Box<dyn serialport::SerialPort>,
    app_handle: tauri::AppHandle,
    running_flag: Arc<Mutex<bool>>,
    send_rx: channel::Receiver<SendCommand>,
) {
    let mut buffer = vec![0u8; 1024];
    // 创建一个超时通道（每1ms一个节拍，用于触发缓冲区发送）
    let ticker = channel::tick(Duration::from_millis(1));
    // 接收数据缓冲区
    let mut receive_buffer: Vec<u8> = Vec::new();
    const BUFFER_FLUSH_THRESHOLD: usize = 512;

    loop {
        // 检查是否应该停止
        let running = running_flag.lock().unwrap();
        if !*running {
            // 退出前发送剩余数据
            if !receive_buffer.is_empty() {
                let data = String::from_utf8_lossy(&receive_buffer).to_string();
                let _ = app_handle.emit("serial-data", data);
            }
            break;
        }
        drop(running);

        // 设置短超时用于检查是否有数据
        port.set_timeout(Duration::from_millis(1)).ok();

        // 使用 select! 同时监听发送通道和超时节拍
        crossbeam::select! {
            recv(send_rx) -> cmd => {
                match cmd {
                    Ok(SendCommand::Send(data)) => {
                        if let Err(e) = port.write(data.as_bytes()) {
                            println!("发送失败: {}", e);
                        }
                        if let Err(e) = port.flush() {
                            println!("刷新失败: {}", e);
                        }
                    }
                    Err(e) => {
                        println!("接收发送命令错误: {}", e);
                    }
                }
            }
            recv(ticker) -> _ => {
                // 超时，检查是否有数据可读
                match port.read(&mut buffer) {
                    Ok(bytes_read) if bytes_read > 0 => {
                        receive_buffer.extend_from_slice(&buffer[..bytes_read]);

                        // 如果缓冲区满，立即发送
                        if receive_buffer.len() >= BUFFER_FLUSH_THRESHOLD {
                            let data = String::from_utf8_lossy(&receive_buffer).to_string();
                            println!("收到数据: {}", data);
                            let _ = app_handle.emit("serial-data", data);
                            receive_buffer.clear();
                        }
                    }
                    Ok(_) => {
                        // 没有新数据，检查是否需要发送累积的数据
                        if !receive_buffer.is_empty() {
                            let data = String::from_utf8_lossy(&receive_buffer).to_string();
                            println!("收到数据: {}", data);
                            let _ = app_handle.emit("serial-data", data);
                            receive_buffer.clear();
                        }
                    }
                    Err(ref e) if e.kind() == io::ErrorKind::TimedOut => {
                        // 超时，检查是否需要发送累积的数据
                        if !receive_buffer.is_empty() {
                            let data = String::from_utf8_lossy(&receive_buffer).to_string();
                            println!("收到数据: {}", data);
                            let _ = app_handle.emit("serial-data", data);
                            receive_buffer.clear();
                        }
                    }
                    Err(e) => {
                        println!("读取串口错误: {}", e);
                        break;
                    }
                }
            }
        }
    }

    println!("接收线程已退出");
}

/**
 * @brief 手动触发接收（用于调试）
 * @param timeout_ms 超时时间（毫秒）
 */
#[tauri::command]
pub fn receive_data(timeout_ms: u64, state: tauri::State<'_, SerialStateState>) -> Result<String> {
    let state_guard = state.0.borrow();

    // 检查连接状态
    if !state_guard.is_connected {
        return Err("串口未连接".to_string());
    }

    // 获取配置信息
    let port_name = state_guard.port_name.clone();
    let baud_rate = state_guard.baud_rate;
    let data_bits = state_guard.data_bits;
    let stop_bits = state_guard.stop_bits;
    let parity = state_guard.parity.clone();

    drop(state_guard);

    // 转换配置参数
    let data_bits_enum = match data_bits {
        5 => serialport::DataBits::Five,
        6 => serialport::DataBits::Six,
        7 => serialport::DataBits::Seven,
        _ => serialport::DataBits::Eight,
    };

    let stop_bits_enum = match stop_bits {
        2 => serialport::StopBits::Two,
        _ => serialport::StopBits::One,
    };

    let parity_enum = match parity.as_str() {
        "odd" => serialport::Parity::Odd,
        "even" => serialport::Parity::Even,
        _ => serialport::Parity::None,
    };

    // 打开临时串口连接进行接收
    let settings = serialport::new(&port_name, baud_rate)
        .data_bits(data_bits_enum)
        .stop_bits(stop_bits_enum)
        .parity(parity_enum);

    match settings.open() {
        Ok(mut port) => {
            port.set_timeout(Duration::from_millis(timeout_ms)).ok();

            let mut buffer = vec![0u8; 1024];
            match port.read(&mut buffer) {
                Ok(bytes_read) => {
                    let data = String::from_utf8_lossy(&buffer[..bytes_read]).to_string();
                    if !data.is_empty() {
                        println!("收到数据: {}", data);
                    }
                    Ok(data)
                }
                Err(ref e) if e.kind() == io::ErrorKind::TimedOut => Ok(String::new()),
                Err(e) => Err(format!("接收失败: {}", e)),
            }
        }
        Err(e) => {
            Err(format!("打开串口失败: {}", e))
        }
    }
}

/**
 * @brief 生成日志内容
 * @param logs 日志列表
 * @param format 导出格式 ("ascii" 或 "hex")
 * @return String 格式化后的日志内容
 */
fn format_logs_content(logs: &[LogItem], format: &str) -> String {
    logs.iter()
        .map(|item| {
            let data = if format == "hex" {
                item.raw.chars()
                    .map(|c| format!("{:02X}", c as u8))
                    .collect::<Vec<_>>()
                    .join(" ")
            } else {
                item.content.clone()
            };
            format!("[{}] [{}] {}", item.time, item.type_.to_uppercase(), data)
        })
        .collect::<Vec<_>>()
        .join("\n")
}

/**
 * @brief 导出日志到文件
 * @param logs 日志列表
 * @param format 导出格式 ("ascii" 或 "hex")
 * @param app Tauri 应用句柄
 * @return Result<String> 导出结果
 */
#[tauri::command]
pub fn export_logs(logs: Vec<LogItem>, format: String, app: AppHandle) -> Result<String> {
    let content = format_logs_content(&logs, &format);

    // 生成默认文件名
    let file_name = format!("serial_log_{}.log", chrono::Local::now().format("%Y%m%d_%H%M%S"));

    // 使用窗口对话框
    if let Err(e) = app.emit("export-logs-dialog", (&content, &file_name, &format)) {
        println!("发送导出事件失败: {}", e);
    }

    Ok(content)
}

/*---------- variable ----------*/
/*---------- function ----------*/
/*---------- end of file ----------*/
