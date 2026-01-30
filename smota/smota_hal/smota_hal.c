/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_hal.c
 * @Author       : lxf
 * @Date         : 2026-01-29 14:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 17:25:51
 * @Brief        : smOTA HAL 注册和获取模块
 * @details      实现 HAL 接口的注册、获取和管理功能
 */

/*---------- includes ----------*/
#include "smota_hal.h"
#include "../smota_core/inc/smota_config.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/**
 * @brief  全局 HAL 接口指针
 * @note   静态变量，仅在本文件可见
 */
static const struct smota_hal *g_smota_hal = NULL;

/*---------- function prototype ----------*/

/**
 * @brief  注册并初始化 HAL 接口
 * @param  hal: HAL 结构体指针
 * @return 0=成功, <0=失败
 */
int smota_hal_register(const struct smota_hal *hal)
{
    if (hal == NULL) {
        SMOTA_DEBUG_PRINTF("Error: HAL pointer is NULL\r\n");
        return -1;
    }

    /* 检查 Flash 驱动 */
    if (hal->flash == NULL) {
        SMOTA_DEBUG_PRINTF("Error: Flash driver is NULL\r\n");
        return -2;
    }

    /* 检查通信驱动 */
    if (hal->comm == NULL) {
        SMOTA_DEBUG_PRINTF("Error: Comm driver is NULL\r\n");
        return -3;
    }

    /* 检查系统驱动 */
    if (hal->system == NULL) {
        SMOTA_DEBUG_PRINTF("Error: System driver is NULL\r\n");
        return -4;
    }

    /* 加密驱动可选（根据配置） */
#if SMOTA_RELIABILITY_SOURCE || SMOTA_RELIABILITY_TRANSMISSION
    if (hal->crypto == NULL) {
        SMOTA_DEBUG_PRINTF("Error: Crypto driver is NULL (required by config)\r\n");
        return -5;
    }
#endif

    /* 保存 HAL 指针 */
    g_smota_hal = hal;

    SMOTA_DEBUG_PRINTF("HAL registered successfully\r\n");
    return 0;
}

/**
 * @brief  获取已注册的 HAL 接口
 * @return HAL 结构体指针，NULL=未初始化
 */
const struct smota_hal *smota_hal_get(void)
{
    return g_smota_hal;
}

/**
 * @brief  去注册并清理 HAL 接口
 * @return 0=成功, <0=失败
 */
int smota_hal_unregister(void)
{
    if (g_smota_hal == NULL) {
        SMOTA_DEBUG_PRINTF("Warning: HAL not registered\r\n");
        return -1;
    }

    g_smota_hal = NULL;
    SMOTA_DEBUG_PRINTF("HAL unregistered\r\n");
    return 0;
}

/**
 * @brief  检查 HAL 是否已初始化
 * @return 1=已初始化, 0=未初始化
 */
int smota_hal_is_initialized(void)
{
    return (g_smota_hal != NULL) ? 1 : 0;
}

/*---------- end of file ----------*/
