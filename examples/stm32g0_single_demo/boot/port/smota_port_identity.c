/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_identity.c
 * @Author       : lxf
 * @Date         : 2026-04-28 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 18:00:00
 * @Brief        : STM32G0 平台 smOTA 固件身份驱动实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "smota_app_info.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/
#define SMOTA_PORT_BOOT_PROJECT_ID "SMOTA_BOOT"

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int identity_get_default_info(struct smota_firmware_info *info);
static int identity_get_running_info(struct smota_firmware_info *info);

/*---------- variable ----------*/
struct smota_identity_driver g_smota_identity_driver = {
    .get_default_info = identity_get_default_info,
    .get_running_info = identity_get_running_info,
};

/*---------- function ----------*/
/**
 * @brief  读取版本元数据
 * @return App 信息指针，NULL=无效
 */
const struct smota_app_info *smota_port_get_app_info(void)
{
    const struct smota_app_info *app_info =
        (const struct smota_app_info *)SMOTA_PORT_APP_INFO_ADDR;

    if (app_info->magic != SMOTA_APP_INFO_MAGIC) {
        return NULL;
    }

    return app_info;
}

/**
 * @brief  获取空 Boot 默认身份
 * @param  info: 固件身份输出
 * @return 0=成功, <0=失败
 */
static int identity_get_default_info(struct smota_firmware_info *info)
{
    if (info == NULL) {
        return -1;
    }

    memset(info, 0, sizeof(*info));
    memcpy(info->project_id,
           SMOTA_PORT_BOOT_PROJECT_ID,
           sizeof(SMOTA_PORT_BOOT_PROJECT_ID) - 1U);

    return 0;
}

/**
 * @brief  获取当前运行 App 身份
 * @param  info: 固件身份输出
 * @return 0=成功, <0=无有效 App 信息
 */
static int identity_get_running_info(struct smota_firmware_info *info)
{
    const struct smota_app_info *app_info;

    if (info == NULL) {
        return -1;
    }

    app_info = smota_port_get_app_info();
    if (app_info == NULL) {
        return -2;
    }

    memset(info, 0, sizeof(*info));
    info->version[0] = app_info->fw_version_major;
    info->version[1] = app_info->fw_version_minor;
    info->version[2] = app_info->fw_version_patch;
    memcpy(info->project_id, app_info->project_id, sizeof(info->project_id));

    return 0;
}

/*---------- end of file ----------*/
