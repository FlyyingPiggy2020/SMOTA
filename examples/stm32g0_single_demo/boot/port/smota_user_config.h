/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_user_config.h
 * @Author       : lxf
 * @Date         : 2026-04-27 11:20:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 11:20:00
 * @Brief        : STM32G0 单分区 Boot Demo smOTA 用户配置
 */

#ifndef SMOTA_USER_CONFIG_H
#define SMOTA_USER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/

/*---------- macro ----------*/
#define SMOTA_MODE                     2
#define SMOTA_RELIABILITY_SOURCE       0
#define SMOTA_RELIABILITY_TRANSMISSION 0
#define SMOTA_RELIABILITY_VERSION      0

#define SMOTA_FLASH_BASE_ADDR          0x08000000U
#define SMOTA_FLASH_SIZE               0x00040000U
#define SMOTA_BOOTLOADER_SIZE          0x0000A000U
#define SMOTA_APP_SIZE                 0x00036000U
#define SMOTA_FLASH_PAGE_SIZE          0x00000800U

#define SMOTA_MAX_MTU_SIZE             512U
#define SMOTA_WORK_BUF_SIZE            1024U
#define SMOTA_DECRYPT_BUF_SIZE         512U

#define SMOTA_ENABLE_DEBUG             0

#define SMOTA_PACKET_TIMEOUT_MS        5000U
#define SMOTA_VERIFY_TIMEOUT_MS        30000U

#define SMOTA_APP_PROJECT_ID           "STM32G0SS_APP"
#define SMOTA_APP_VERSION_MAJOR        0U
#define SMOTA_APP_VERSION_MINOR        0U
#define SMOTA_APP_VERSION_PATCH        0U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_USER_CONFIG_H */
