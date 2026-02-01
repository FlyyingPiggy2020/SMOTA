/*!
 * @file serialService.ts
 * @brief 串口通信服务 - 前端调用后端串口命令的封装
 * @author SMOTA Tool
 * @date 2025-01-31
 */

import { invoke } from "@tauri-apps/api/core";
import type {
    SerialConfig,
    SerialPortInfo,
    ConnectionStatus,
} from "../types/serialTypes";
import { DEFAULT_SERIAL_CONFIG } from "../types/serialTypes";

/// 串口通信服务类
class SerialService {
    /// 当前连接状态
    private status: ConnectionStatus = {
        is_connected: false,
        port_name: "",
        config: { ...DEFAULT_SERIAL_CONFIG },
    };

    /// 获取可用串口列表
    async getPortList(): Promise<SerialPortInfo[]> {
        try {
            const result = await invoke<SerialPortInfo[]>("get_serial_ports");
            return result;
        } catch (error) {
            console.error("获取串口列表失败:", error);
            throw error;
        }
    }

    /// 打开串口
    async open(portName: string, config?: Partial<SerialConfig>): Promise<boolean> {
        try {
            const finalConfig: SerialConfig = {
                ...DEFAULT_SERIAL_CONFIG,
                ...config,
            };

            const result = await invoke<boolean>("open_serial_port", {
                portName,
                config: finalConfig,
            });

            if (result) {
                this.status.is_connected = true;
                this.status.port_name = portName;
                this.status.config = finalConfig;
            }

            return result;
        } catch (error) {
            console.error("打开串口失败:", error);
            throw error;
        }
    }

    /// 关闭串口
    async close(): Promise<boolean> {
        try {
            const result = await invoke<boolean>("close_serial_port");

            if (result) {
                this.status.is_connected = false;
                this.status.port_name = "";
            }

            return result;
        } catch (error) {
            console.error("关闭串口失败:", error);
            throw error;
        }
    }

    /// 获取连接状态
    async getStatus(): Promise<ConnectionStatus> {
        try {
            const result = await invoke<ConnectionStatus>("get_connection_status");
            this.status = result;
            return result;
        } catch (error) {
            console.error("获取连接状态失败:", error);
            throw error;
        }
    }

    /// 发送数据
    async send(data: string): Promise<number> {
        try {
            if (!this.status.is_connected) {
                throw new Error("串口未连接");
            }

            const result = await invoke<number>("send_data", { data });
            return result;
        } catch (error) {
            console.error("发送数据失败:", error);
            throw error;
        }
    }

    /// 接收数据
    async receive(maxBytes?: number): Promise<string> {
        try {
            if (!this.status.is_connected) {
                throw new Error("串口未连接");
            }

            const result = await invoke<string>("receive_data", { maxBytes });
            return result;
        } catch (error) {
            console.error("接收数据失败:", error);
            throw error;
        }
    }

    /// 刷新缓冲区
    async flush(): Promise<boolean> {
        try {
            const result = await invoke<boolean>("flush_buffer");
            return result;
        } catch (error) {
            console.error("刷新缓冲区失败:", error);
            throw error;
        }
    }

    /// 检查串口可用性（用于热插拔检测）
    async checkAvailability(): Promise<boolean> {
        try {
            const result = await invoke<boolean>("check_port_availability");
            return result;
        } catch (error) {
            console.error("检查串口可用性失败:", error);
            return true; // 保守返回可用
        }
    }

    /// 检查是否已连接
    isConnected(): boolean {
        return this.status.is_connected;
    }

    /// 获取当前端口名
    getPortName(): string {
        return this.status.port_name;
    }

    /// 获取当前配置
    getConfig(): SerialConfig {
        return { ...this.status.config };
    }
}

/// 导出单例
export const serialService = new SerialService();
