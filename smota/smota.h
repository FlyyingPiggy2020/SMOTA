/**
 * @file    smota.h
 * @brief   smOTA 统一头文件
 *
 * @details 使用说明：
 *          1. 将 smota/ 文件夹复制到你的项目
 *          2. 将 smota 加入头文件搜索路径：
 *          3. 根据你的平台，将对应源文件添加到编译：
 *          4. 在你的代码中 #include "smota.h"
 *
 * @example 最小集成示例：
 *          @code
 *          #include "smota.h"
 *
 *          @endcode
 */

#ifndef SMOTA_H
#define SMOTA_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 1. 配置文件（必须首先包含）
 *============================================================================*/
#include <stdbool.h>
#include <stddef.h>
#include "smota_core/inc/smota_config.h"

/*==============================================================================
 * 2. 核心库（平台无关）
 *============================================================================*/
#include "smota_hal/smota_hal.h"

/*==============================================================================
 * 3. 核心模块头文件
 *============================================================================*/
#include "smota_core/inc/smota_types.h"
#include "smota_core/inc/smota_state.h"
#include "smota_core/inc/smota_packet.h"
#include "smota_core/inc/smota_verify.h"
#include "smota_core/inc/smota_flash.h"

/*==============================================================================
 * 4. 加密模块（根据配置条件包含）
 *============================================================================*/
#if SMOTA_RELIABILITY_CONTENT || SMOTA_RELIABILITY_SOURCE || SMOTA_RELIABILITY_TRANSMISSION
#include "smota_crypto.h"
#endif

/*==============================================================================
 * 5. Bootloader（仅 Bootloader 项目需要）
 *============================================================================*/
#ifdef SMOTA_BOOTLOADER_BUILD
#include "bootloader.h"
#endif

/*==============================================================================
 * 6. 公共 API 声明
 *============================================================================*/

/**
 * @brief       初始化 OTA 模块
 * @return      smota_err_t 错误码
 */
smota_err_t smota_init(void);

/**
 * @brief       去初始化 OTA 模块
 */
void smota_deinit(void);

/**
 * @brief       OTA 主轮询函数
 * @note        需要在主循环中周期性调用，建议每1ms调用一次
 */
void smota_poll(void);

/**
 * @brief       启动 OTA 升级（主动触发）
 * @return      smota_err_t 错误码
 */
smota_err_t smota_start(void);

/**
 * @brief       中止当前 OTA 升级
 * @return      smota_err_t 错误码
 */
smota_err_t smota_abort(void);

/**
 * @brief       获取 OTA 升级进度
 * @return      uint8_t 进度百分比 (0-100)
 */
uint8_t smota_get_progress(void);

/**
 * @brief       获取当前 OTA 状态
 * @return      smota_state_t 当前状态
 */
smota_state_t smota_get_state(void);

/**
 * @brief       获取最近一次错误码
 * @return      smota_err_t 错误码
 */
smota_err_t smota_get_error(void);

/**
 * @brief       获取错误码对应的字符串描述
 * @return      const char* 错误描述字符串
 */
const char *smota_get_error_string(void);

/**
 * @brief       检查 OTA 是否正在运行中
 * @return      bool true=运行中, false=空闲
 */
bool smota_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // SMOTA_H
