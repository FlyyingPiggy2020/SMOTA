/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_internal.h
 * @Author       : lxf
 * @Date         : 2026-04-28 16:10:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 16:10:00
 * @Brief        : STM32G0 平台 smOTA 端口内部声明
 */

#ifndef SMOTA_PORT_INTERNAL_H
#define SMOTA_PORT_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include "main.h"
#include "smota.h"
#include "smota_user_config.h"

/*---------- macro ----------*/
#define SMOTA_PORT_APP_BASE_ADDR     (SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE)
#define SMOTA_PORT_STORAGE_BASE      SMOTA_PORT_APP_BASE_ADDR
#define SMOTA_PORT_APP_INFO_ADDR     (SMOTA_PORT_APP_BASE_ADDR + 0x200U)
#define SMOTA_PORT_SRAM_END_ADDR     (SRAM_BASE + 0x00024000U)

/*---------- type define ----------*/

/*---------- variable prototype ----------*/
extern struct smota_flash_driver g_smota_flash_driver;
extern struct smota_comm_driver g_smota_comm_driver;
extern struct smota_system_driver g_smota_system_driver;
extern struct smota_identity_driver g_smota_identity_driver;
extern struct smota_boot_driver g_smota_boot_driver;

/*---------- function prototype ----------*/
int smota_port_flash_flush_staged_write(void);
void smota_port_flash_reset_write_ctx(uint32_t next_addr);
int smota_port_flash_erase_pages(uint32_t addr, uint32_t size);
int smota_port_flash_program_doubleword(uint32_t addr, const uint8_t data[8]);
const struct smota_app_info *smota_port_get_app_info(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_PORT_INTERNAL_H */
