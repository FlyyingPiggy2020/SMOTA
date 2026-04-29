/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_types.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 09:57:46
 * @Brief        : smOTA 类型定义
 */

#ifndef SMOTA_TYPES_H
#define SMOTA_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>

/*---------- macro ----------*/

/*---------- type define ----------*/
/**
 * @brief OTA 状态枚举
 * @details 描述 OTA 升级过程中设备所处的各个阶段
 */
typedef enum {
    SMOTA_STATE_IDLE = 0,        /*!< 空闲状态，等待 OTA 请求 */
    SMOTA_STATE_HANDSHAKE,       /*!< 握手阶段，协商参数 */
    SMOTA_STATE_HEADER_INFO,     /*!< 头部信息阶段，接收固件头部 */
    SMOTA_STATE_TRANSFER,        /*!< 传输阶段，接收固件数据 */
    SMOTA_STATE_COMPLETE,        /*!< 传输完成，验证固件 */
    SMOTA_STATE_INSTALL,         /*!< 安装阶段，写入新固件 */
    SMOTA_STATE_ACTIVATE,        /*!< 激活阶段，准备切换固件 */
    SMOTA_STATE_ERROR,           /*!< 错误状态，发生错误 */
    SMOTA_STATE_MAX              /*!< 状态枚举最大值 */
} smota_state_t;

/**
 * @brief OTA 错误码枚举
 * @details 定义 OTA 过程中可能出现的错误类型
 */
typedef enum {
    SMOTA_ERR_OK = 0,            /*!< 无错误 */
    SMOTA_ERR_INVALID_STATE,     /*!< 无效状态 */
    SMOTA_ERR_INVALID_PARAM,     /*!< 无效参数 */
    SMOTA_ERR_LENGTH,            /*!< 无效长度 */
    SMOTA_ERR_TIMEOUT,           /*!< 操作超时 */
    SMOTA_ERR_CRC,               /*!< CRC 校验失败 */
    SMOTA_ERR_VERSION,           /*!< 版本验证失败 */
    SMOTA_ERR_SPACE,             /*!< 空间不足 */
    SMOTA_ERR_FLASH,             /*!< Flash 操作失败 */
    SMOTA_ERR_SIGNATURE,         /*!< 签名验证失败 */
    SMOTA_ERR_NOT_SUPPORTED,     /*!< 功能不支持 */
    SMOTA_ERR_BUSY,              /*!< 设备忙 */
    SMOTA_ERR_MAX                /*!< 错误码最大值 */
} smota_err_t;

/**
 * @brief 固件身份信息
 * @details 存储固件版本与项目 ID
 */
struct smota_firmware_info {
    uint8_t version[4];                      /*!< 固件版本号 */
    uint8_t project_id[16];                  /*!< 项目名称 ID */
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief       获取错误码对应的字符串描述
 * @param[in]   err     错误码
 * @return      const char* 错误描述字符串
 */
const char *smota_err_to_string(smota_err_t err);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_TYPES_H
