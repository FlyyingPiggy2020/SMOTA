# 串口模块开发文档

本文档说明串口通信模块的实现细节，供单片机开发人员参考。

---

## 第二阶段：串口管理模块

### 2.1 模块概述

串口管理模块使用 Rust 实现，通过 Tauri 的 IPC 机制与前端通信，提供以下功能：

| 功能 | 说明 |
|------|------|
| 获取串口列表 | 列出系统可用的串口设备 |
| 打开串口 | 根据配置打开指定串口 |
| 关闭串口 | 关闭已打开的串口 |
| 发送数据 | 向串口发送字符串数据 |
| 接收数据 | 从串口读取数据 |
| 刷新缓冲区 | 清除串口接收缓冲区 |

### 2.2 技术选型

#### 2.2.1 为什么使用 serialport 库

| 库名 | 优点 | 缺点 |
|------|------|------|
| `serialport` | 跨平台（Windows/Linux/macOS）、活跃维护、文档完善 | Windows 下依赖 libusb |
| `winapi` | Windows 原生、无额外依赖 | 仅限 Windows、API 复杂 |
| `serial` | 轻量级 | 功能有限、维护不活跃 |

**选择 serialport 的理由**：
- 跨平台兼容，方便后续移植
- 社区活跃，Bug 修复及时
- API 设计合理，易于使用

#### 2.2.2 Cargo.toml 依赖配置

```toml
[dependencies]
serialport = "7"
```

**版本说明**：使用 7.x 最新稳定版，支持：
- Windows: 使用 Windows API
- Linux: 使用 termios
- macOS: 使用 IOKit

### 2.3 Rust 实现

#### 2.3.1 项目文件结构

```
src-tauri/src/
├── main.rs              # Rust 程序入口
├── lib.rs               # Tauri 命令定义
└── serial_port.rs       # 串口通信模块
```

#### 2.3.2 串口配置结构体

**文件**: `src-tauri/src/serial_port.rs`

```rust
/// 串口配置参数
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct SerialConfig {
    pub baud_rate: u32,      // 波特率
    pub data_bits: u8,       // 数据位 (5, 6, 7, 8)
    pub stop_bits: u8,       // 停止位 (1, 2)
    pub parity: String,      // 校验位 (none, odd, even)
    pub flow_control: String,// 流控 (none, rts/cts, dtr/dsr)
    pub timeout: u64,        // 超时时间（毫秒）
}
```

**与单片机串口配置的对应关系**：

| 单片机配置 | Rust 配置 | 常用值 |
|-----------|-----------|--------|
| 波特率 | `baud_rate` | 9600, 115200 |
| 数据位 | `data_bits` | 8 |
| 停止位 | `stop_bits` | 1 |
| 校验位 | `parity` | none |
| 流控 | `flow_control` | none |

#### 2.3.3 串口命令实现

**获取串口列表**：

```rust
#[tauri::command]
pub fn get_serial_ports(state: State<SerialState>) -> Result<Vec<SerialPortInfo>, String> {
    match available_ports() {
        Ok(ports) => {
            let mut result = Vec::new();
            for port in ports {
                result.push(SerialPortInfo {
                    port_name: port.port_name,
                    port_type: format!("{:?}", port.port_type),
                    is_open: false,
                });
            }
            Ok(result)
        }
        Err(e) => Err(format!("获取串口列表失败: {}", e)),
    }
}
```

**打开串口**：

```rust
#[tauri::command]
pub fn open_serial_port(
    port_name: String,
    config: SerialConfig,
    state: State<SerialState>,
) -> Result<bool, String> {
    // 解析配置参数
    let data_bits = match config.data_bits {
        5 => serialport::DataBits::Five,
        6 => serialport::DataBits::Six,
        7 => serialport::DataBits::Seven,
        _ => serialport::DataBits::Eight,  // 默认 8 位
    };

    // 打开串口
    match serialport::new(&port_name, config.baud_rate)
        .data_bits(data_bits)
        .stop_bits(serialport::StopBits::One)
        .parity(serialport::Parity::None)
        .timeout(std::time::Duration::from_millis(config.timeout))
        .open()
    {
        Ok(serial_port) => {
            // 保存串口句柄到全局状态
            let mut guard = state.0.lock().unwrap();
            guard.port = Some(serial_port);
            guard.is_connected = true;
            Ok(true)
        }
        Err(e) => Err(format!("打开串口失败: {}", e)),
    }
}
```

**发送数据**：

```rust
#[tauri::command]
pub fn send_data(data: String, state: State<SerialState>) -> Result<usize, String> {
    let mut guard = state.0.lock().unwrap();
    let bytes = data.as_bytes();
    guard.port.as_mut().unwrap().write(bytes)
}
```

**接收数据**：

```rust
#[tauri::command]
pub fn receive_data(state: State<SerialState>) -> Result<String, String> {
    let mut guard = state.0.lock().unwrap();
    let mut buffer = vec![0u8; 1024];

    match guard.port.as_mut().unwrap().read(buffer.as_mut_slice()) {
        Ok(n) => {
            buffer.truncate(n);
            Ok(String::from_utf8_lossy(&buffer).to_string())
        }
        Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {
            Ok(String::new())  // 超时返回空
        }
        Err(e) => Err(format!("接收数据失败: {}", e)),
    }
}
```

#### 2.3.4 全局状态管理

```rust
/// 串口连接状态
pub struct SerialConnection {
    pub port: Option<Box<dyn SerialPort>>,  // 串口句柄
    pub config: SerialConfig,                // 当前配置
    pub is_connected: bool,                  // 连接状态
    pub port_name: String,                   // 端口名
}

/// 全局串口状态（使用 Mutex 保证线程安全）
pub struct SerialState(pub Mutex<SerialConnection>);
```

**为什么需要全局状态**：
- Tauri 命令是无状态的，每次调用都是独立的
- 需要通过 `State<SerialState>` 在应用生命周期内保持串口句柄
- 使用 `Mutex` 保证多线程访问安全

#### 2.3.5 注册 Tauri 命令

**文件**: `src-tauri/src/lib.rs`

```rust
use serial_port::{SerialState, init_serial_state};

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(init_serial_state())  // 注册全局状态
        .invoke_handler(tauri::generate_handler![
            get_serial_ports,
            open_serial_port,
            close_serial_port,
            send_data,
            receive_data,
            // ...
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

### 2.4 前端实现

#### 2.4.1 TypeScript 类型定义

**文件**: `src/types/serialTypes.ts`

```typescript
/// 串口配置参数
export interface SerialConfig {
    baud_rate: number;      // 波特率
    data_bits: number;      // 数据位
    stop_bits: number;      // 停止位
    parity: 'none' | 'odd' | 'even';
    flow_control: 'none' | 'rts/cts' | 'dtr/dsr';
    timeout: number;        // 超时（毫秒）
}

/// 串口信息
export interface SerialPortInfo {
    port_name: string;      // 如 "COM3"
    port_type: string;      // 如 "USB"
    is_open: boolean;
}
```

#### 2.4.2 串口服务封装

**文件**: `src/services/serialService.ts`

```typescript
class SerialService {
    /// 获取串口列表
    async getPortList(): Promise<SerialPortInfo[]> {
        return await invoke<SerialPortInfo[]>("get_serial_ports");
    }

    /// 打开串口
    async open(portName: string, config?: Partial<SerialConfig>): Promise<boolean> {
        return await invoke<boolean>("open_serial_port", {
            portName,
            config: { ...DEFAULT_CONFIG, ...config },
        });
    }

    /// 发送数据
    async send(data: string): Promise<number> {
        return await invoke<number>("send_data", { data });
    }

    /// 接收数据
    async receive(maxBytes?: number): Promise<string> {
        return await invoke<string>("receive_data", { maxBytes });
    }
}

export const serialService = new SerialService();
```

#### 2.4.3 调用后端命令

**Tauri 调用语法**：

```typescript
import { invoke } from "@tauri-apps/api/core";

// 调用 Rust 命令
const result = await invoke<ReturnType>("command_name", { param1, param2 });
```

**命令名与 Rust 函数的对应关系**：

| Rust 函数名 | 前端命令名 |
|------------|-----------|
| `get_serial_ports` | `"get_serial_ports"` |
| `open_serial_port` | `"open_serial_port"` |
| `close_serial_port` | `"close_serial_port"` |
| `send_data` | `"send_data"` |
| `receive_data` | `"receive_data"` |

### 2.5 调试方法

#### 2.5.1 本地调试 Rust 代码

```bash
# 在 serial-tool 目录下
cd src-tauri

# 运行单元测试
cargo test serial

# 调试运行
cargo run
```

#### 2.5.2 开发模式运行

```bash
# 在 serial-tool 目录下
pnpm tauri dev
```

**说明**：
- 开发模式会自动启动前端开发服务器
- 修改 Rust 代码后需要重新编译
- 修改前端代码自动热更新

#### 2.5.3 查看 Rust 日志

在 Rust 代码中添加日志：

```rust
println!("Debug info: {:?}", data);
log::info!("Serial port opened: {}", port_name);
```

日志会显示在运行 `pnpm tauri dev` 的终端中。

#### 2.5.4 常见问题排查

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 获取串口列表为空 | 串口被占用、驱动问题 | 重新插拔设备、检查设备管理器 |
| 打开串口失败 | 端口不存在、权限不足 | 以管理员身份运行、检查端口名 |
| 发送无响应 | 波特率不匹配、流控设置 | 检查单片机配置 |
| 接收数据乱码 | 编码问题、串口参数错误 | 检查编码设置、波特率、数据位 |

### 2.6 与单片机通信示例

#### 2.6.1 单片机发送数据（STM32 示例）

```c
// STM32 USART 发送字符串
void usart_send_string(char* str) {
    while (*str) {
        USART_SendData(USART1, *str++);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    }
}

// 主循环
int main(void) {
    USART1_Init(115200);  // 初始化串口，波特率 115200

    while (1) {
        usart_send_string("Hello from STM32!\r\n");
        delay_ms(1000);
    }
}
```

#### 2.6.2 单片机接收数据（STM32 示例）

```c
// USART1 中断处理
void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t data = USART_ReceiveData(USART1);
        // 处理接收到的数据
        USART_SendData(USART1, data);  // 回显
    }
}
```

#### 2.6.3 上位机操作步骤

1. **选择串口**：在下拉框中选择 COM 端口
2. **设置参数**：波特率 115200，数据位 8，停止位 1，校验位 none
3. **点击连接**：连接成功后状态变为"断开"
4. **查看接收**：上位机将显示单片机发送的数据
5. **发送数据**：在发送框输入内容，Ctrl+Enter 发送

### 2.7 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src-tauri/Cargo.toml` | 修改 | 添加 serialport 依赖 |
| `src-tauri/src/serial_port.rs` | 新建 | 串口通信模块实现 |
| `src-tauri/src/lib.rs` | 修改 | 注册串口命令 |
| `src/types/serialTypes.ts` | 新建 | TypeScript 类型定义 |
| `src/services/serialService.ts` | 新建 | 前端串口服务 |
| `src/App.vue` | 修改 | 串口工具界面 |

---

## 文档版本

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2025-01-31 | 初始版本，串口模块实现说明 |
