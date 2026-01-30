/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_flash.h
 * @Author       : lxf
 * @Date         : 2026-01-30 12:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 12:00:00
 * @Brief        : smOTA Flash 操作封装
 */

#ifndef SMOTA_FLASH_H
#define SMOTA_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include "smota_types.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief       写入数据到备份区
 * @param[in]   src: 源数据指针
 * @param[in]   size: 写入大小
 * @return      实际写入字节数, <0=失败
 * @note        自动处理跨页写入
 */
int smota_flash_write_backup(const uint8_t *src, uint32_t size);

/**
 * @brief       擦除备份区
 * @param[in]   size: 擦除大小
 * @return      0=成功, <0=失败
 * @note        按页擦除，支持进度回调
 */
int smota_flash_erase_backup(uint32_t size);

/**
 * @brief       固件拷贝（双槽位模式）
 * @param[in]   src_addr: 源地址（备份区）
 * @param[in]   dst_addr: 目标地址（应用区）
 * @param[in]   size: 拷贝大小
 * @return      0=成功, <0=失败
 * @note        边拷贝边校验
 */
int smota_flash_copy_firmware(uint32_t src_addr, uint32_t dst_addr, uint32_t size);

/**
 * @brief       读取固件版本
 * @param[out]  version: 版本号输出 [major, minor, patch]
 * @return      0=成功, <0=失败
 */
int smota_flash_read_version(uint8_t version[3]);

/**
 * @brief       写入固件版本
 * @param[in]   version: 版本号 [major, minor, patch]
 * @return      0=成功, <0=失败
 */
int smota_flash_write_version(const uint8_t version[3]);

/**
 * @brief       检查 Flash 是否为空（全部为 0xFF）
 * @param[in]   addr: 起始地址
 * @param[in]   size: 检查大小
 * @return      true=空, false=非空
 */
bool smota_flash_is_erased(uint32_t addr, uint32_t size);

/**
 * @brief       获取备份区起始地址
 * @return      备份区起始地址
 */
uint32_t smota_flash_backup_addr(void);

/**
 * @brief       获取应用区起始地址
 * @return      应用区起始地址
 */
uint32_t smota_flash_app_addr(void);

/**
 * @brief       获取备份区大小
 * @return      备份区大小
 */
uint32_t smota_flash_backup_size(void);

/**
 * @brief       获取应用区大小
 * @return      应用区大小
 */
uint32_t smota_flash_app_size(void);

#ifdef __cplusplus
}
#endif

#endif // SMOTA_FLASH_H
