use serialport::{self, DataBits, FlowControl, Parity, StopBits};
use std::fmt;
use std::io::{self, Read, Write};
use std::time::Duration;

/// 串口错误类型
#[derive(Debug)]
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

/// 串口配置
#[derive(Clone, Debug)]
pub struct SerialConfig {
    pub data_bits: DataBits,
    pub stop_bits: StopBits,
    pub parity: Parity,
    pub flow_control: FlowControl,
}

impl Default for SerialConfig {
    fn default() -> Self {
        SerialConfig {
            data_bits: DataBits::Eight,
            stop_bits: StopBits::One,
            parity: Parity::None,
            flow_control: FlowControl::None,
        }
    }
}

impl SerialConfig {
    /// 设置数据位
    pub fn data_bits(mut self, data_bits: DataBits) -> Self {
        self.data_bits = data_bits;
        self
    }

    /// 设置停止位
    pub fn stop_bits(mut self, stop_bits: StopBits) -> Self {
        self.stop_bits = stop_bits;
        self
    }

    /// 设置校验位
    pub fn parity(mut self, parity: Parity) -> Self {
        self.parity = parity;
        self
    }

    /// 设置流控
    pub fn flow_control(mut self, flow_control: FlowControl) -> Self {
        self.flow_control = flow_control;
        self
    }
}

/// 串口管理器
pub struct SerialManager {
    port: Option<Box<dyn serialport::SerialPort>>,
    port_name: String,
    baud_rate: u32,
    config: SerialConfig,
}

impl SerialManager {
    /// 创建新的串口管理器
    pub fn new(port: &str, baud_rate: u32) -> Self {
        SerialManager {
            port: None,
            port_name: port.to_string(),
            baud_rate,
            config: SerialConfig::default(),
        }
    }

    /// 打开串口连接
    pub fn connect(&mut self) -> Result<(), SerialError> {
        let settings = serialport::new(&self.port_name, self.baud_rate)
            .data_bits(self.config.data_bits)
            .stop_bits(self.config.stop_bits)
            .parity(self.config.parity)
            .flow_control(self.config.flow_control);

        match settings.open() {
            Ok(p) => {
                self.port = Some(p);
                Ok(())
            }
            Err(e) => Err(SerialError::ConnectionFailed(e.to_string())),
        }
    }

    /// 关闭串口连接
    pub fn disconnect(&mut self) {
        self.port.take();
    }

    /// 检查是否已连接
    pub fn is_connected(&self) -> bool {
        self.port.is_some()
    }

    /// 获取端口名称
    pub fn port_name(&self) -> &str {
        &self.port_name
    }

    /// 获取波特率
    pub fn baud_rate(&self) -> u32 {
        self.baud_rate
    }

    /// 设置串口配置
    pub fn set_config(&mut self, config: SerialConfig) {
        self.config = config;
    }

    /// 获取当前配置
    pub fn config(&self) -> &SerialConfig {
        &self.config
    }

    /// 设置读取超时
    pub fn set_timeout(&mut self, timeout: Duration) -> Result<(), SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;
        port.set_timeout(timeout)
            .map_err(|e| SerialError::ReadError(e.to_string()))
    }

    /// 发送字符串（自动转换并添加换行）
    pub fn send_string(&mut self, data: &str) -> Result<(), SerialError> {
        self.send_bytes(data.as_bytes())
    }

    /// 发送原始字节数据
    pub fn send_bytes(&mut self, data: &[u8]) -> Result<(), SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;

        port.write(data)
            .map_err(|e| SerialError::WriteError(e.to_string()))?;

        port.flush()
            .map_err(|e| SerialError::WriteError(e.to_string()))?;

        Ok(())
    }

    /// 接收字符串（指定超时时间，毫秒）
    pub fn receive_string(&mut self, timeout_ms: u64) -> Result<String, SerialError> {
        let bytes = self.receive_bytes(timeout_ms)?;
        String::from_utf8(bytes).map_err(|e| SerialError::InvalidUtf8(e.into_bytes()))
    }

    /// 接收原始字节数据（指定超时时间，毫秒）
    pub fn receive_bytes(&mut self, timeout_ms: u64) -> Result<Vec<u8>, SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;

        let timeout = Duration::from_millis(timeout_ms);
        port.set_timeout(timeout)
            .map_err(|e| SerialError::ReadError(e.to_string()))?;

        let mut buffer = vec![0u8; 1024];
        match port.read(&mut buffer) {
            Ok(bytes_read) => Ok(buffer[..bytes_read].to_vec()),
            Err(ref e) if e.kind() == io::ErrorKind::TimedOut => Err(SerialError::Timeout),
            Err(e) => Err(SerialError::ReadError(e.to_string())),
        }
    }

    /// 接收固定长度的字节数据
    pub fn receive_exact(&mut self, size: usize, timeout_ms: u64) -> Result<Vec<u8>, SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;

        let timeout = Duration::from_millis(timeout_ms);
        port.set_timeout(timeout)
            .map_err(|e| SerialError::ReadError(e.to_string()))?;

        let mut buffer = vec![0u8; size];
        let mut total_read = 0;

        while total_read < size {
            match port.read(&mut buffer[total_read..]) {
                Ok(0) => return Err(SerialError::Timeout),
                Ok(n) => total_read += n,
                Err(ref e) if e.kind() == io::ErrorKind::TimedOut => {
                    return Err(SerialError::Timeout)
                }
                Err(e) => return Err(SerialError::ReadError(e.to_string())),
            }
        }

        Ok(buffer)
    }

    /// 清空接收缓冲区
    pub fn flush_input(&mut self) -> Result<(), SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;
        // 读取并丢弃所有可用数据
        let mut buffer = vec![0u8; 256];
        loop {
            match port.read(&mut buffer) {
                Ok(0) => break,
                Ok(_) => continue,
                Err(ref e) if e.kind() == io::ErrorKind::TimedOut => break,
                Err(e) => return Err(SerialError::ReadError(e.to_string())),
            }
        }
        Ok(())
    }

    /// 清空发送缓冲区
    pub fn flush_output(&mut self) -> Result<(), SerialError> {
        let port = self.port.as_mut().ok_or(SerialError::PortNotOpen)?;
        port.flush()
            .map_err(|e| SerialError::WriteError(e.to_string()))
    }
}

/// 列出所有可用串口端口名称
pub fn list_ports() -> Vec<String> {
    serialport::available_ports()
        .expect("获取串口列表失败")
        .into_iter()
        .map(|p| p.port_name)
        .collect()
}

/// 列出所有可用串口的详细信息
pub fn list_ports_detailed() -> Vec<serialport::SerialPortInfo> {
    serialport::available_ports().expect("获取串口列表失败")
}

/// Drop trait 实现：自动关闭串口
impl Drop for SerialManager {
    fn drop(&mut self) {
        self.disconnect();
    }
}
