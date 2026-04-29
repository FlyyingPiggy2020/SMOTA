/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_app_info.h
 * @Author       : lxf
 * @Date         : 2026-04-27 15:20:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 15:20:00
 * @Brief        : smOTA 应用元数据定义
 */

#ifndef SMOTA_APP_INFO_H
#define SMOTA_APP_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include <string.h>
#include "smota_config.h"

/*---------- macro ----------*/
#define SMOTA_APP_INFO_MAGIC             0x41505056UL
#define SMOTA_APP_INFO_PROJECT_ID_LEN    16U
#define SMOTA_APP_INFO_SECTION           ".smota_app_version"
#define SMOTA_APP_INFO_SYMBOL            g_smota_app_version_info

#ifndef SMOTA_APP_PROJECT_ID
#define SMOTA_APP_PROJECT_ID             "SMOTA_APP"
#endif

#ifndef SMOTA_APP_VERSION_MAJOR
#define SMOTA_APP_VERSION_MAJOR          1U
#endif

#ifndef SMOTA_APP_VERSION_MINOR
#define SMOTA_APP_VERSION_MINOR          0U
#endif

#ifndef SMOTA_APP_VERSION_PATCH
#define SMOTA_APP_VERSION_PATCH          0U
#endif

#define SMOTA_APP_INFO_DEFINE() \
    __attribute__((used, section(SMOTA_APP_INFO_SECTION))) \
    const struct smota_app_info SMOTA_APP_INFO_SYMBOL = { \
        .magic = SMOTA_APP_INFO_MAGIC, \
        .fw_version_major = SMOTA_APP_VERSION_MAJOR, \
        .fw_version_minor = SMOTA_APP_VERSION_MINOR, \
        .fw_version_patch = SMOTA_APP_VERSION_PATCH, \
        .project_id = SMOTA_APP_PROJECT_ID, \
    }

/*---------- type define ----------*/
struct smota_app_info {
    uint32_t magic;
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    uint8_t fw_version_patch;
    uint8_t project_id[SMOTA_APP_INFO_PROJECT_ID_LEN];
};

/*---------- variable prototype ----------*/
extern const struct smota_app_info SMOTA_APP_INFO_SYMBOL;

/*---------- function prototype ----------*/
/**
 * @brief  获取应用固化版本号
 * @param  version: 输出缓冲区，长度 4 字节
 */
static inline void smota_app_info_copy_version(uint8_t version[4])
{
    if (version == NULL) {
        return;
    }

    memset(version, 0, 4U);
    version[0] = SMOTA_APP_INFO_SYMBOL.fw_version_major;
    version[1] = SMOTA_APP_INFO_SYMBOL.fw_version_minor;
    version[2] = SMOTA_APP_INFO_SYMBOL.fw_version_patch;
}

/**
 * @brief  获取应用固化项目名称 ID
 * @param  project_id: 输出缓冲区，长度 16 字节
 */
static inline void smota_app_info_copy_project_id(uint8_t project_id[SMOTA_APP_INFO_PROJECT_ID_LEN])
{
    if (project_id == NULL) {
        return;
    }

    memcpy(project_id,
           SMOTA_APP_INFO_SYMBOL.project_id,
           sizeof(SMOTA_APP_INFO_SYMBOL.project_id));
}

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_APP_INFO_H */
