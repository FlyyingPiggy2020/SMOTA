/*!
 * @file serialTypes.ts
 * @brief 串口通信模块 TypeScript 类型定义
 * @author SMOTA Tool
 * @date 2025-01-31
 */

/// 串口配置参数
export interface SerialConfig {
    /// 波特率（默认 115200）
    baud_rate: number;
    /// 数据位 (5, 6, 7, 8，默认 8)
    data_bits: number;
    /// 停止位 (1, 2，默认 1)
    stop_bits: number;
    /// 校验位 (none, odd, even，默认 none)
    parity: 'none' | 'odd' | 'even';
    /// 流控 (none, rts/cts, dtr/dsr，默认 none)
    flow_control: 'none' | 'rts/cts' | 'dtr/dsr';
    /// 超时时间（毫秒，默认 1000）
    timeout: number;
}

/// 串口默认配置
export const DEFAULT_SERIAL_CONFIG: SerialConfig = {
    baud_rate: 115200,
    data_bits: 8,
    stop_bits: 1,
    parity: 'none',
    flow_control: 'none',
    timeout: 1000,
};

/// 串口信息
export interface SerialPortInfo {
    /// 端口名称（如 "COM3"）
    port_name: string;
    /// 设备类型（如 USB、PCI）
    port_type: string;
    /// 是否已连接
    is_open: boolean;
}

/// 连接状态
export interface ConnectionStatus {
    /// 是否已连接
    is_connected: boolean;
    /// 当前端口名
    port_name: string;
    /// 当前配置
    config: SerialConfig;
}

/// 接收数据项
export interface ReceivedData {
    /// 数据内容
    data: string;
    /// 时间戳
    timestamp: string;
    /// 是否为 Hex 模式
    is_hex: boolean;
}

/// 发送历史项
export interface SendHistory {
    /// 发送的数据
    data: string;
    /// 发送时间
    timestamp: string;
    /// 是否为 Hex 模式
    is_hex: boolean;
}

/// 串口命令返回值类型
export type GetSerialPortsResult = SerialPortInfo[];
export type OpenSerialPortResult = boolean;
export type CloseSerialPortResult = boolean;
export type GetConnectionStatusResult = ConnectionStatus;
export type SendDataResult = number;
export type ReceiveDataResult = string;
export type FlushBufferResult = boolean;
