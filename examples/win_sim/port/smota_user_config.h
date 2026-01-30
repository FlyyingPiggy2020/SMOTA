/**
 * @file    smota_user_config.h
 * @brief   smOTA 用户自定义配置文件
 * @version v1.0
 * @date    2026-01-26
 *
 * @details 本文件为 STM32G0B1 系列 MCU 的配置示例
 *          通过编译选项 -DSMOTA_USER_CONFIG_FILE=\"smota_user_config.h\" 引入
 *
 * @note    只需定义需要覆盖的配置项，未定义项将使用默认值
 *
 * @note    STM32G0B1 规格：
 *          - Flash: 256KB (双 Bank)
 *          - RAM: 144KB
 *          - Flash 页大小: 2KB
 */

#ifndef SMOTA_USER_CONFIG_H
#define SMOTA_USER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 1. 升级模式配置
 *============================================================================*/

/**
 * @brief 升级模式
 * @note   STM32G0B1 支持双 Bank 硬件交换，推荐使用模式 0
 */
#define SMOTA_MODE 0  // 双 Bank 硬件交换模式

/*==============================================================================
 * 2. 可靠性配置
 *============================================================================*/

/**
 * @brief 来源可靠性（ECDSA 签名验证）
 * @note   推荐启用，防止固件被伪造
 */
#define SMOTA_RELIABILITY_SOURCE 0

/**
 * @brief 过程可靠性（AES 加密传输）
 * @note   根据安全需求选择
 */
#define SMOTA_RELIABILITY_TRANSMISSION 0

/**
 * @brief 版本可靠性（防回滚）
 * @note   推荐启用，防止固件版本回退
 */
#define SMOTA_RELIABILITY_VERSION 0

/*==============================================================================
 * 3. Flash 布局配置（STM32G0B1）
 *============================================================================*/

/**
 * @brief Flash 基地址
 */
#define SMOTA_FLASH_BASE_ADDR 0x08000000

/**
 * @brief Flash 总容量
 * @note   STM32G0B1 为 256KB
 */
#define SMOTA_FLASH_SIZE 0x40000  // 256KB

/**
 * @brief Bootloader 占用空间
 */
#define SMOTA_BOOTLOADER_SIZE 0x2000  // 8KB

/**
 * @brief 应用程序区大小
 * @note   双 Bank 模式下，这是单个 Bank 的大小
 *         Flash = 256KB, Bootloader = 8KB
 *         每个 Bank = (256KB - 8KB) / 2 = 124KB
 */
#define SMOTA_APP_SIZE 0x1F000  // 124KB (单个 Bank，对齐到 2KB 页边界)

/**
 * @brief Flash 页大小
 * @note   STM32G0 系列: 2KB
 */
#define SMOTA_FLASH_PAGE_SIZE 0x800  // 2KB

/*==============================================================================
 * 4. 固件包配置
 *============================================================================*/

/**
 * @brief 最大数据包大小
 * @note   根据传输协议和可用 RAM 调整
 */
#define SMOTA_PACKET_MAX_SIZE 1024  // 字节

/*==============================================================================
 * 5. 缓冲区配置
 *============================================================================*/

/**
 * @brief 工作缓冲区大小
 */
#define SMOTA_WORK_BUF_SIZE 2048  // 字节

/**
 * @brief 解密缓冲区大小
 */
#define SMOTA_DECRYPT_BUF_SIZE 1024  // 字节

/*==============================================================================
 * 6. 调试配置
 *============================================================================*/

/**
 * @brief 启用调试输出
 */
#define SMOTA_ENABLE_DEBUG 1

/**
 * @brief 调试输出函数
 */
#include <stdio.h>
#define SMOTA_DEBUG_PRINTF(...) printf("[smOTA] " __VA_ARGS__)

/*==============================================================================
 * 7. 超时配置
 *============================================================================*/

#define SMOTA_PACKET_TIMEOUT_MS 5000   // 5秒
#define SMOTA_VERIFY_TIMEOUT_MS 30000  // 30秒

#ifdef __cplusplus
}
#endif

#endif // SMOTA_USER_CONFIG_H
