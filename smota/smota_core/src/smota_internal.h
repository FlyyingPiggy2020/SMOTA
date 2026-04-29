/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_internal.h
 * @Author       : lxf
 * @Date         : 2026-04-28 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 18:00:00
 * @Brief        : smOTA 核心内部状态定义
 */

#ifndef SMOTA_INTERNAL_H
#define SMOTA_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include "../inc/smota_types.h"

/*---------- macro ----------*/

/*---------- type define ----------*/
/**
 * @brief OTA 上下文结构体
 * @details 存储 OTA 升级过程中的内部运行时状态
 */
struct smota_ctx {
    smota_state_t state;                     /*!< 当前状态 */
    uint32_t firmware_size;                  /*!< 固件总大小（字节） */
    uint32_t received_size;                  /*!< 已接收数据大小（字节） */
    uint8_t firmware_version[4];             /*!< 待升级固件版本号 */
    struct smota_firmware_info current_info; /*!< 当前运行固件身份 */
    uint8_t expected_hash[32];               /*!< 固件期望 SHA-256 */
    uint8_t signature_r[32];                 /*!< ECDSA 签名 r 分量 */
    uint8_t signature_s[32];                 /*!< ECDSA 签名 s 分量 */
    uint32_t timeout_ms;                     /*!< 通信超时时间（毫秒） */
    uint32_t recv_len;                       /*!< 已接收数据长度 */
    uint32_t last_packet_time;               /*!< 最后接收数据包的时间戳 */
    uint64_t boot_window_start_time;         /*!< Boot 捕获窗口开始时间 */
    uint8_t reset_pending;                   /*!< 响应发送后执行重启 */
    uint8_t should_stay_in_boot;             /*!< 是否应停留在 Boot */
    uint32_t sync_error_count;               /*!< 同步错误计数（乱码恢复次数） */
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
/**
 * @brief       获取 OTA 内部上下文指针
 * @return      struct smota_ctx* 上下文指针
 */
struct smota_ctx *smota_ctx_get(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_INTERNAL_H */
