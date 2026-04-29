/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port.c
 * @Author       : lxf
 * @Date         : 2026-04-27 11:20:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 16:10:00
 * @Brief        : STM32G0 平台 smOTA 端口注册实现
 */

/*---------- includes ----------*/
#include "smota.h"
#include "smota_port.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
static struct smota_hal g_smota_hal = {
    .flash = &g_smota_flash_driver,
    .comm = &g_smota_comm_driver,
    .crypto = NULL,
    .system = &g_smota_system_driver,
    .identity = &g_smota_identity_driver,
    .boot = &g_smota_boot_driver,
};

static uint8_t g_port_initialized = 0U;

/*---------- function ----------*/
/**
 * @brief  初始化 STM32G0 的 smOTA 端口
 * @return 0=成功, <0=失败
 */
int smota_port_init(void)
{
    int ret;

    if (g_port_initialized != 0U) {
        return 0;
    }

    ret = smota_hal_register(&g_smota_hal);
    if (ret < 0) {
        return ret;
    }

    smota_port_flash_reset_write_ctx(0U);

    g_port_initialized = 1U;
    return 0;
}

/**
 * @brief  去初始化 STM32G0 的 smOTA 端口
 * @return 0=成功, <0=失败
 */
int smota_port_deinit(void)
{
    int ret;

    ret = smota_port_flash_flush_staged_write();
    if (ret < 0) {
        return ret;
    }

    ret = smota_hal_unregister();
    if (ret < 0) {
        return ret;
    }

    g_port_initialized = 0U;
    smota_port_flash_reset_write_ctx(0U);
    return 0;
}

/*---------- end of file ----------*/
