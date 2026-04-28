/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_boot.h
 * @Author       : lxf
 * @Date         : 2026-04-28 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 10:00:00
 * @Brief        : smOTA Boot 入口请求定义
 */

#ifndef SMOTA_BOOT_H
#define SMOTA_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>

/*---------- macro ----------*/
#ifndef SMOTA_BOOT_REQUEST_ADDR
#define SMOTA_BOOT_REQUEST_ADDR          0x20000000U
#endif

#define SMOTA_BOOT_REQUEST_MAGIC         0x53424F54UL

/*---------- type define ----------*/
struct smota_boot_request {
    uint32_t magic;
    uint32_t magic_inv;
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
/**
 * @brief  请求下次复位进入 Boot
 */
static inline void smota_boot_request_set(void)
{
    volatile struct smota_boot_request *request =
        (volatile struct smota_boot_request *)SMOTA_BOOT_REQUEST_ADDR;

    request->magic = SMOTA_BOOT_REQUEST_MAGIC;
    request->magic_inv = ~SMOTA_BOOT_REQUEST_MAGIC;
}

/**
 * @brief  清除进入 Boot 请求
 */
static inline void smota_boot_request_clear(void)
{
    volatile struct smota_boot_request *request =
        (volatile struct smota_boot_request *)SMOTA_BOOT_REQUEST_ADDR;

    request->magic = 0U;
    request->magic_inv = 0U;
}

/**
 * @brief  检查是否请求进入 Boot
 * @return 1=请求进入 Boot, 0=无请求
 */
static inline uint8_t smota_boot_request_is_set(void)
{
    volatile const struct smota_boot_request *request =
        (volatile const struct smota_boot_request *)SMOTA_BOOT_REQUEST_ADDR;

    return (request->magic == SMOTA_BOOT_REQUEST_MAGIC &&
            request->magic_inv == ~SMOTA_BOOT_REQUEST_MAGIC) ? 1U : 0U;
}

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_BOOT_H */
