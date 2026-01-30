/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_flash.c
 * @Author       : lxf
 * @Date         : 2026-01-30 12:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 12:00:00
 * @Brief        : smOTA Flash 操作封装实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "smota_flash.h"
#include "smota_config.h"
#include "../smota_hal/smota_hal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/**
 * @brief  Flash 操作上下文
 */
struct smota_flash_ctx {
    uint32_t write_addr;     /* 当前写入地址 */
    uint32_t erase_addr;     /* 当前擦除地址 */
    uint32_t progress;       /* 进度 */
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
/**
 * @brief  Flash 操作上下文（单例）
 */
static struct smota_flash_ctx g_flash_ctx = {
    .write_addr = 0,
    .erase_addr = 0,
    .progress = 0,
};

/**
 * @brief  备份区起始地址缓存
 */
static uint32_t g_backup_addr = 0;

/**
 * @brief  应用区起始地址缓存
 */
static uint32_t g_app_addr = 0;

/*---------- function ----------*/

/**
 * @brief       计算备份区起始地址
 * @return      备份区起始地址
 */
static uint32_t calc_backup_addr(void)
{
    if (g_backup_addr == 0) {
        g_backup_addr = SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE;
    }
    return g_backup_addr;
}

/**
 * @brief       计算应用区起始地址
 * @return      应用区起始地址
 */
static uint32_t calc_app_addr(void)
{
    if (g_app_addr == 0) {
#if SMOTA_MODE == 0  /* 双 Bank 模式 */
        g_app_addr = SMOTA_FLASH_BASE_ADDR;
#elif SMOTA_MODE == 1  /* 双槽位模式 */
        g_app_addr = SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE + SMOTA_APP_SIZE;
#else  /* 单分区模式 */
        g_app_addr = SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE;
#endif
    }
    return g_app_addr;
}

/**
 * @brief       初始化 Flash 操作模块
 * @return      0=成功, <0=失败
 */
static int flash_init(void)
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -1;
    }

    flash = hal->flash;

    if (flash->init != NULL) {
        return flash->init();
    }

    return 0;
}

/**
 * @brief       写入数据到备份区
 * @param[in]   src: 源数据指针
 * @param[in]   size: 写入大小
 * @return      实际写入字节数, <0=失败
 */
int smota_flash_write_backup(const uint8_t *src, uint32_t size)
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint32_t addr;
    uint32_t page_size;
    uint32_t page_offset;
    uint32_t page_remain;
    uint32_t written = 0;
    int ret;

    if (src == NULL || size == 0) {
        return -1;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -2;
    }

    flash = hal->flash;
    addr = calc_backup_addr() + g_flash_ctx.write_addr;
    page_size = SMOTA_FLASH_PAGE_SIZE;

    /* 解锁 Flash */
    if (flash->flash_unlock != NULL) {
        flash->flash_unlock();
    }

    while (written < size) {
        page_offset = addr % page_size;
        page_remain = page_size - page_offset;

        /* 需要擦除当前页 */
        if (page_offset == 0) {
            ret = flash->erase(addr, page_size);
            if (ret < 0) {
                goto cleanup;
            }
        }

        /* 计算本次写入大小 */
        uint32_t chunk = (size - written < page_remain) ? (size - written) : page_remain;

        /* 执行写入 */
        ret = flash->write(addr, src + written, chunk);
        if (ret != (int)chunk) {
            goto cleanup;
        }

        addr += chunk;
        written += chunk;
        g_flash_ctx.write_addr += chunk;
    }

cleanup:
    /* 上锁 Flash */
    if (flash->flash_lock != NULL) {
        flash->flash_lock();
    }

    return (written > 0) ? (int)written : ret;
}

/**
 * @brief       擦除备份区
 * @param[in]   size: 擦除大小
 * @return      0=成功, <0=失败
 */
int smota_flash_erase_backup(uint32_t size)
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint32_t addr;
    uint32_t page_size;
    uint32_t erase_size;
    uint32_t erased = 0;
    int ret;

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -1;
    }

    flash = hal->flash;
    addr = calc_backup_addr();
    page_size = SMOTA_FLASH_PAGE_SIZE;

    /* 计算需要擦除的大小（按页对齐） */
    erase_size = (size + page_size - 1) / page_size * page_size;

    /* 解锁 Flash */
    if (flash->flash_unlock != NULL) {
        flash->flash_unlock();
    }

    while (erased < erase_size) {
        ret = flash->erase(addr, page_size);
        if (ret < 0) {
            goto cleanup;
        }

        addr += page_size;
        erased += page_size;
        g_flash_ctx.erase_addr = addr - calc_backup_addr();
    }

cleanup:
    /* 上锁 Flash */
    if (flash->flash_lock != NULL) {
        flash->flash_lock();
    }

    return (erased > 0) ? 0 : ret;
}

/**
 * @brief       固件拷贝（双槽位模式）
 * @param[in]   src_addr: 源地址（备份区）
 * @param[in]   dst_addr: 目标地址（应用区）
 * @param[in]   size: 拷贝大小
 * @return      0=成功, <0=失败
 */
int smota_flash_copy_firmware(uint32_t src_addr, uint32_t dst_addr, uint32_t size)
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint32_t page_size;
    uint32_t offset = 0;
    int ret;

    /* 初始化 Flash */
    ret = flash_init();
    if (ret < 0) {
        return ret;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -2;
    }

    flash = hal->flash;
    page_size = SMOTA_FLASH_PAGE_SIZE;

    /* 分配缓冲区 */
    uint8_t buffer[SMOTA_WORK_BUF_SIZE];

    /* 解锁 Flash */
    if (flash->flash_unlock != NULL) {
        flash->flash_unlock();
    }

    while (offset < size) {
        uint32_t chunk = (size - offset < SMOTA_WORK_BUF_SIZE) ?
                         (size - offset) : SMOTA_WORK_BUF_SIZE;

        /* 计算页对齐的 chunk */
        uint32_t aligned_chunk = (chunk + page_size - 1) / page_size * page_size;

        /* 读取源数据 */
        ret = flash->read(src_addr + offset, buffer, aligned_chunk);
        if (ret != (int)aligned_chunk) {
            goto cleanup;
        }

        /* 擦除目标页 */
        ret = flash->erase(dst_addr + offset, aligned_chunk);
        if (ret < 0) {
            goto cleanup;
        }

        /* 写入目标地址 */
        ret = flash->write(dst_addr + offset, buffer, aligned_chunk);
        if (ret != (int)aligned_chunk) {
            goto cleanup;
        }

        offset += aligned_chunk;

        /* 更新进度 */
        g_flash_ctx.progress = (offset * 100) / size;
    }

cleanup:
    /* 上锁 Flash */
    if (flash->flash_lock != NULL) {
        flash->flash_lock();
    }

    return (offset >= size) ? 0 : ret;
}

/**
 * @brief       读取固件版本
 * @param[out]  version: 版本号输出 [major, minor, patch]
 * @return      0=成功, <0=失败
 */
int smota_flash_read_version(uint8_t version[3])
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint32_t addr;
    int ret;

    if (version == NULL) {
        return -1;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -2;
    }

    flash = hal->flash;

    /* 版本存储在应用区末尾 */
    addr = calc_app_addr() + SMOTA_APP_SIZE - 16;

    ret = flash->read(addr, version, 3);

    return ret;
}

/**
 * @brief       写入固件版本
 * @param[in]   version: 版本号 [major, minor, patch]
 * @return      0=成功, <0=失败
 */
int smota_flash_write_version(const uint8_t version[3])
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint32_t addr;
    int ret;

    if (version == NULL) {
        return -1;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return -2;
    }

    flash = hal->flash;

    /* 解锁 Flash */
    if (flash->flash_unlock != NULL) {
        flash->flash_unlock();
    }

    /* 版本存储在应用区末尾 */
    addr = calc_app_addr() + SMOTA_APP_SIZE - 16;

    /* 擦除并写入 */
    ret = flash->erase(addr, SMOTA_FLASH_PAGE_SIZE);
    if (ret < 0) {
        goto cleanup;
    }

    ret = flash->write(addr, version, 3);

cleanup:
    /* 上锁 Flash */
    if (flash->flash_lock != NULL) {
        flash->flash_lock();
    }

    return ret;
}

/**
 * @brief       检查 Flash 是否为空（全部为 0xFF）
 * @param[in]   addr: 起始地址
 * @param[in]   size: 检查大小
 * @return      true=空, false=非空
 */
bool smota_flash_is_erased(uint32_t addr, uint32_t size)
{
    const struct smota_hal *hal;
    const struct smota_flash_driver *flash;
    uint8_t buffer[256];
    uint32_t offset = 0;

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        return false;
    }

    flash = hal->flash;

    while (offset < size) {
        uint32_t chunk = (size - offset < 256) ? (size - offset) : 256;

        if (flash->read(addr + offset, buffer, chunk) != (int)chunk) {
            return false;
        }

        for (uint32_t i = 0; i < chunk; i++) {
            if (buffer[i] != 0xFF) {
                return false;
            }
        }

        offset += chunk;
    }

    return true;
}

/**
 * @brief       获取备份区起始地址
 * @return      备份区起始地址
 */
uint32_t smota_flash_backup_addr(void)
{
    return calc_backup_addr();
}

/**
 * @brief       获取应用区起始地址
 * @return      应用区起始地址
 */
uint32_t smota_flash_app_addr(void)
{
    return calc_app_addr();
}

/**
 * @brief       获取备份区大小
 * @return      备份区大小
 */
uint32_t smota_flash_backup_size(void)
{
#if SMOTA_MODE == 0  /* 双 Bank 模式 */
    return SMOTA_APP_SIZE;
#elif SMOTA_MODE == 1  /* 双槽位模式 */
    return SMOTA_APP_SIZE;
#else  /* 单分区模式 */
    return SMOTA_FLASH_SIZE - SMOTA_BOOTLOADER_SIZE - SMOTA_APP_SIZE;
#endif
}

/**
 * @brief       获取应用区大小
 * @return      应用区大小
 */
uint32_t smota_flash_app_size(void)
{
    return SMOTA_APP_SIZE;
}

/*---------- end of file ----------*/
