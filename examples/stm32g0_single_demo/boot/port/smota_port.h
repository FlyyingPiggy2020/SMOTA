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
 * @brief  读取当前运行固件版本
 * @param  version: 版本输出缓冲区，长度 4 字节
 */
void smota_port_load_running_version(uint8_t version[4]);

/**
 * @brief  读取当前运行固件的项目名称 ID
 * @param  project_id: 项目标识输出缓冲区，长度 16 字节
 */
void smota_port_load_running_project_id(uint8_t project_id[16]);

/**
 * @brief  检查 App 镜像是否有效
 * @return 1=有效, 0=无效
 */
uint8_t smota_port_is_app_valid(void);

/**
 * @brief  跳转到 App
 * @return 0=成功跳转前准备完成, <0=跳转失败
 */
int smota_port_jump_to_app(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_PORT_H */
