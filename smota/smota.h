/**
 * @file    smota.h
 * @brief   smOTA 统一头文件
 *
 * @details 使用说明：
 *          1. 将 smota/ 文件夹复制到你的项目
 *          2. 将 smota 加入头文件搜索路径：
 *          3. 根据你的平台，将对应源文件添加到编译：
 *             - smota/smota_core/src/*.c
 *             - smota/smota_hal/ports/xxx/*.c (选择一个平台)
 *             - smota/smota_crypto/src/*.c (启用加密功能时)
 *          4. 在你的代码中 #include "smota.h"
 *
 * @example 最小集成示例：
 *          @code
 *          #include "smota.h"
 *
 *          int main(void) {
 *              OTA_Init(&my_hal, work_buf, sizeof(work_buf));
 *              OTA_DownloadStart(&header);
 *              // ...
 *          }
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
#include "smota_core/inc/smota_types.h"
#include "smota_core/inc/smota_state.h"
#include "smota_core/inc/smota_packet.h"
#include "smota_core/inc/smota_verify.h"
#include "smota_core/inc/smota_hal.h"

/*==============================================================================
 * 3. 加密模块（根据配置条件包含）
 *============================================================================*/
#if SMOTA_RELIABILITY_CONTENT || SMOTA_RELIABILITY_SOURCE || SMOTA_RELIABILITY_TRANSMISSION
#include "smota_crypto.h"
#endif

/*==============================================================================
 * 4. Bootloader（仅 Bootloader 项目需要）
 *============================================================================*/
#ifdef SMOTA_BOOTLOADER_BUILD
#include "bootloader.h"
#endif

/*==============================================================================
 * 5. 公共 API 声明
 *============================================================================*/

/**
 * @brief 初始化 smOTA 库
 * @param hal       HAL 接口函数表
 * @param work_buf  工作缓冲区（至少 2KB）
 * @param buf_size  缓冲区大小
 * @return          0=成功, 负值=错误码
 */
int OTA_Init(const OTA_Hardware_Interface_t *hal,
             uint8_t *work_buf,
             uint32_t buf_size);

/**
 * @brief 去初始化 smOTA 库
 */
void OTA_Deinit(void);

/**
 * @brief 开始下载新固件
 * @param header  固件包头部信息（已预解析）
 * @return        0=成功, 负值=错误码
 */
int OTA_DownloadStart(const OTA_PackageHeader_t *header);

/**
 * @brief 写入下载的数据包
 * @param data    数据包内容
 * @param offset  在固件包中的偏移
 * @param len     数据长度
 * @param crc     数据包 CRC-16
 * @return        已处理的字节数，负值=错误码
 */
int OTA_DownloadWrite(const uint8_t *data, uint32_t offset, uint32_t len, uint16_t crc);

/**
 * @brief 完成下载，开始验签
 * @return  0=成功, 负值=错误码
 */
int OTA_DownloadFinish(void);

/**
 * @brief 获取当前 OTA 状态
 * @return  当前状态
 */
OTA_State_t OTA_GetState(void);

/**
 * @brief 获取升级进度 (0-100)
 * @return  进度百分比
 */
uint8_t OTA_GetProgress(void);

/**
 * @brief 获取当前运行的固件版本
 * @param version  输出版本信息
 */
void OTA_GetCurrentVersion(FirmwareVersion_t *version);

/**
 * @brief 获取新固件版本
 * @param version  输出版本信息
 */
void OTA_GetNewVersion(FirmwareVersion_t *version);

/**
 * @brief 激活新固件（设置标志位并重启）
 * @note   此函数不会返回（系统复位）
 */
void OTA_Activate(void) __attribute__((noreturn));

/**
 * @brief 手动触发回滚到旧版本
 * @return  0=成功, 负值=错误码
 */
int OTA_Rollback(void);

/**
 * @brief 设置事件回调
 * @param callback  回调函数指针
 */
typedef void (*OTA_EventCallback_t)(OTA_Event_t event, void *data);

void OTA_SetCallback(OTA_EventCallback_t callback);

#ifdef __cplusplus
}
#endif

#endif // SMOTA_H
