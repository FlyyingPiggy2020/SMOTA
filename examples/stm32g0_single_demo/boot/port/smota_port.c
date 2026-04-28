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
#include <string.h>
#include "smota.h"
#include "smota_app_info.h"
#include "smota_port.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/
#define SMOTA_PORT_BOOT_PROJECT_ID "SMOTA_BOOT"

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static void smota_port_set_default_version(uint8_t version[4]);
static void smota_port_set_default_project_id(uint8_t project_id[16]);

/*---------- variable ----------*/
static struct smota_hal g_smota_hal = {
    .flash = &g_smota_flash_driver,
    .comm = &g_smota_comm_driver,
    .crypto = NULL,
    .system = &g_smota_system_driver,
};

static uint8_t g_port_initialized = 0U;

/*---------- function ----------*/
/**
 * @brief  设置默认固件版本
 * @param  version: 版本缓冲区
 */
static void smota_port_set_default_version(uint8_t version[4])
{
    if (version == NULL) {
        return;
    }

    memset(version, 0, 4U);
}

/**
 * @brief  设置默认项目 ID
 * @param  project_id: 项目标识缓冲区
 */
static void smota_port_set_default_project_id(uint8_t project_id[16])
{
    if (project_id == NULL) {
        return;
    }

    memset(project_id, 0, 16U);
    memcpy(project_id, SMOTA_PORT_BOOT_PROJECT_ID, sizeof(SMOTA_PORT_BOOT_PROJECT_ID) - 1U);
}

/**
 * @brief  初始化 STM32G0 的 smOTA 端口
 * @return 0=成功, <0=失败
 */
int smota_port_init(void)
{
    struct smota_ctx *ctx;
    uint8_t project_id[16];
    uint8_t version[4];
    int ret;

    if (g_port_initialized != 0U) {
        return 0;
    }

    ret = smota_hal_register(&g_smota_hal);
    if (ret < 0) {
        return ret;
    }

    smota_port_flash_reset_write_ctx(0U);
    smota_port_load_running_version(version);
    smota_port_load_running_project_id(project_id);

    ctx = smota_ctx_get();
    if (ctx != NULL) {
        memcpy(ctx->current_version, version, sizeof(ctx->current_version));
        memcpy(ctx->firmware_version, version, sizeof(ctx->firmware_version));
        memcpy(ctx->current_project_id, project_id, sizeof(ctx->current_project_id));
    }

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

/**
 * @brief  读取当前运行固件版本
 * @param  version: 版本输出缓冲区，长度 4 字节
 */
void smota_port_load_running_version(uint8_t version[4])
{
    struct smota_app_info app_info;

    if (version == NULL) {
        return;
    }

    smota_port_set_default_version(version);
    if (g_smota_system_driver.get_app_info(&app_info) == 0) {
        version[0] = app_info.fw_version_major;
        version[1] = app_info.fw_version_minor;
        version[2] = app_info.fw_version_patch;
    }
}

/**
 * @brief  读取当前运行固件的项目名称 ID
 * @param  project_id: 项目标识输出缓冲区，长度 16 字节
 */
void smota_port_load_running_project_id(uint8_t project_id[16])
{
    struct smota_app_info app_info;

    if (project_id == NULL) {
        return;
    }

    smota_port_set_default_project_id(project_id);
    if (g_smota_system_driver.get_app_info(&app_info) == 0) {
        memcpy(project_id, app_info.project_id, sizeof(app_info.project_id));
    }
}

/*---------- end of file ----------*/
