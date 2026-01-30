/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_types.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 09:57:46
 * @Brief        : smOTA 类型定义
 */

#ifndef SMOTA_TYPES_H
#define SMOTA_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>

/*---------- macro ----------*/

/*---------- type define ----------*/
/**
 * @brief OTA 状态枚举
 * @details 描述 OTA 升级过程中设备所处的各个阶段
 */
typedef enum {
    SMOTA_STATE_IDLE = 0,        /*!< 空闲状态，等待 OTA 请求 */
    SMOTA_STATE_HANDSHAKE,       /*!< 握手阶段，协商参数 */
    SMOTA_STATE_HEADER_INFO,     /*!< 头部信息阶段，接收固件头部 */
    SMOTA_STATE_TRANSFER,        /*!< 传输阶段，接收固件数据 */
    SMOTA_STATE_COMPLETE,        /*!< 传输完成，验证固件 */
    SMOTA_STATE_INSTALL,         /*!< 安装阶段，写入新固件 */
    SMOTA_STATE_ACTIVATE,        /*!< 激活阶段，准备切换固件 */
    SMOTA_STATE_ERROR,           /*!< 错误状态，发生错误 */
    SMOTA_STATE_MAX              /*!< 状态枚举最大值 */
} smota_state_t;

/**
 * @brief OTA 错误码枚举
 * @details 定义 OTA 过程中可能出现的错误类型
 */
typedef enum {
    SMOTA_ERR_OK = 0,            /*!< 无错误 */
    SMOTA_ERR_INVALID_STATE,     /*!< 无效状态 */
    SMOTA_ERR_INVALID_PARAM,     /*!< 无效参数 */
    SMOTA_ERR_TIMEOUT,           /*!< 操作超时 */
    SMOTA_ERR_CRC,               /*!< CRC 校验失败 */
    SMOTA_ERR_VERSION,           /*!< 版本验证失败 */
    SMOTA_ERR_SPACE,             /*!< 空间不足 */
    SMOTA_ERR_FLASH,             /*!< Flash 操作失败 */
    SMOTA_ERR_SIGNATURE,         /*!< 签名验证失败 */
    SMOTA_ERR_NOT_SUPPORTED,     /*!< 功能不支持 */
    SMOTA_ERR_BUSY,              /*!< 设备忙 */
    SMOTA_ERR_MAX                /*!< 错误码最大值 */
} smota_err_t;

/**
 * @brief OTA 上下文结构体
 * @details 存储 OTA 升级过程中的运行时状态信息
 */
struct smota_ctx {
    smota_state_t state;                     /*!< 当前状态 */
    uint32_t firmware_size;                  /*!< 固件总大小（字节） */
    uint32_t received_size;                  /*!< 已接收数据大小（字节） */
    uint8_t firmware_version[4];             /*!< 固件版本号 */
    uint32_t flash_addr;                     /*!< 目标 Flash 起始地址 */
    uint32_t timeout_ms;                     /*!< 通信超时时间（毫秒） */
    uint8_t *recv_buffer;                    /*!< 接收缓冲区指针 */
    uint32_t recv_len;                       /*!< 已接收数据长度 */
    uint32_t last_packet_time;               /*!< 最后接收数据包的时间戳 */
    uint8_t retry_count;                     /*!< 重试计数 */
};

/**
 * @brief 设备信息结构体
 * @details 存储设备的静态信息，用于 OTA 握手阶段
 */
struct smota_device_info {
    uint8_t current_version[4];              /*!< 当前运行的固件版本 */
    uint32_t project_id;                     /*!< 项目 ID */
    uint32_t capabilities;                   /*!< 设备能力标志 */
    uint32_t flash_capacity;                 /*!< Flash 总容量（字节） */
    uint32_t sector_size;                    /*!< 扇区大小（字节） */
    uint32_t block_size;                     /*!< 块大小（字节） */
    uint32_t max_firmware_size;              /*!< 最大支持固件大小（字节） */
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief       获取错误码对应的字符串描述
 * @param[in]   err     错误码
 * @return      const char* 错误描述字符串
 */
const char *smota_err_to_string(smota_err_t err);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_TYPES_H
