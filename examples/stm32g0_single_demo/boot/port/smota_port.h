/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port.h
 * @Author       : lxf
 * @Date         : 2026-04-27 11:20:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 11:20:00
 * @Brief        : STM32G0 平台 smOTA HAL 端口声明
 */

#ifndef SMOTA_PORT_H
#define SMOTA_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
/**
 * @brief  初始化 STM32G0 的 smOTA 端口
 * @return 0=成功, <0=失败
 */
int smota_port_init(void);

/**
 * @brief  去初始化 STM32G0 的 smOTA 端口
 * @return 0=成功, <0=失败
 */
int smota_port_deinit(void);

/**
 * @brief  检查 App 镜像是否有效
 * @return 1=有效, 0=无效
 */
uint8_t smota_port_is_app_valid(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_PORT_H */
