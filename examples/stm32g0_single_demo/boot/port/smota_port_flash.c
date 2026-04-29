/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_flash.c
 * @Author       : lxf
 * @Date         : 2026-04-28 16:10:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 16:10:00
 * @Brief        : STM32G0 平台 smOTA Flash 驱动实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "smota_port_internal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/
struct smota_port_flash_write_ctx {
    uint32_t next_addr;
    uint32_t staged_addr;
    uint8_t staged_data[8];
    uint8_t staged_len;
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int flash_init(void);
static int flash_deinit(void);
static int flash_read(uint32_t addr, uint8_t *data, uint32_t size);
static int flash_write(uint32_t addr, const uint8_t *data, uint32_t size);
static int flash_erase(uint32_t addr, uint32_t size);
static int flash_lock(void);
static int flash_unlock(void);
static uint32_t smota_port_addr_to_phys(uint32_t addr);
static uint32_t smota_port_phys_to_bank(uint32_t addr);
static uint32_t smota_port_phys_to_page(uint32_t addr);

/*---------- variable ----------*/
struct smota_flash_driver g_smota_flash_driver = {
    .init = flash_init,
    .deinit = flash_deinit,
    .read = flash_read,
    .write = flash_write,
    .erase = flash_erase,
    .flash_lock = flash_lock,
    .flash_unlock = flash_unlock,
};

static struct smota_port_flash_write_ctx g_flash_write_ctx = {
    .next_addr = 0U,
    .staged_addr = 0U,
    .staged_data = {0},
    .staged_len = 0U,
};

/*---------- function ----------*/
/**
 * @brief  将逻辑 OTA 地址转换为物理 Flash 地址
 * @param  addr: 逻辑偏移
 * @return 物理地址
 */
static uint32_t smota_port_addr_to_phys(uint32_t addr)
{
    return SMOTA_PORT_STORAGE_BASE + addr;
}

/**
 * @brief  根据物理地址计算所在 Bank
 * @param  addr: 物理地址
 * @return Flash Bank
 */
static uint32_t smota_port_phys_to_bank(uint32_t addr)
{
#if defined(FLASH_BANK_2)
    if (addr >= (FLASH_BASE + FLASH_BANK_SIZE)) {
        return FLASH_BANK_2;
    }
#endif
    return FLASH_BANK_1;
}

/**
 * @brief  根据物理地址计算页号
 * @param  addr: 物理地址
 * @return 页号
 */
static uint32_t smota_port_phys_to_page(uint32_t addr)
{
    uint32_t bank_base = FLASH_BASE;

#if defined(FLASH_BANK_2)
    if (smota_port_phys_to_bank(addr) == FLASH_BANK_2) {
        bank_base = FLASH_BASE + FLASH_BANK_SIZE;
    }
#endif

    return (addr - bank_base) / SMOTA_FLASH_PAGE_SIZE;
}

/**
 * @brief  按页擦除物理 Flash 区域
 * @param  addr: 起始物理地址
 * @param  size: 擦除长度
 * @return 0=成功, <0=失败
 */
int smota_port_flash_erase_pages(uint32_t addr, uint32_t size)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0U;
    uint32_t start_addr;
    uint32_t end_addr;

    if (size == 0U) {
        return 0;
    }

    start_addr = addr & ~(SMOTA_FLASH_PAGE_SIZE - 1U);
    end_addr = (addr + size + SMOTA_FLASH_PAGE_SIZE - 1U) & ~(SMOTA_FLASH_PAGE_SIZE - 1U);

    while (start_addr < end_addr) {
        uint32_t bank = smota_port_phys_to_bank(start_addr);
        uint32_t bank_end = FLASH_BASE + FLASH_BANK_SIZE;
        uint32_t erase_end = end_addr;

#if defined(FLASH_BANK_2)
        if (bank == FLASH_BANK_2) {
            bank_end = FLASH_BASE + FLASH_SIZE;
        }
#endif

        if (erase_end > bank_end) {
            erase_end = bank_end;
        }

        memset(&erase_init, 0, sizeof(erase_init));
        erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
        erase_init.Banks = bank;
        erase_init.Page = smota_port_phys_to_page(start_addr);
        erase_init.NbPages = (erase_end - start_addr) / SMOTA_FLASH_PAGE_SIZE;

        if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
            return -1;
        }

        start_addr = erase_end;
    }

    return 0;
}

/**
 * @brief  以双字粒度写入 Flash
 * @param  addr: 8 字节对齐地址
 * @param  data: 8 字节数据
 * @return 0=成功, <0=失败
 */
int smota_port_flash_program_doubleword(uint32_t addr, const uint8_t data[8])
{
    uint64_t value = 0ULL;

    if ((addr & 0x7U) != 0U || data == NULL) {
        return -1;
    }

    memcpy(&value, data, sizeof(value));
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, value) != HAL_OK) {
        return -2;
    }

    return 0;
}

/**
 * @brief  刷新未满 8 字节的尾部缓存
 * @return 0=成功, <0=失败
 */
int smota_port_flash_flush_staged_write(void)
{
    int ret;

    if (g_flash_write_ctx.staged_len == 0U) {
        return 0;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }

    ret = smota_port_flash_program_doubleword(
        smota_port_addr_to_phys(g_flash_write_ctx.staged_addr),
        g_flash_write_ctx.staged_data);
    (void)HAL_FLASH_Lock();

    if (ret < 0) {
        return ret;
    }

    memset(g_flash_write_ctx.staged_data, 0xFF, sizeof(g_flash_write_ctx.staged_data));
    g_flash_write_ctx.staged_len = 0U;

    return 0;
}

/**
 * @brief  重置顺序写入上下文
 * @param  next_addr: 下一次期望写入偏移
 */
void smota_port_flash_reset_write_ctx(uint32_t next_addr)
{
    g_flash_write_ctx.next_addr = next_addr;
    g_flash_write_ctx.staged_addr = next_addr;
    memset(g_flash_write_ctx.staged_data, 0xFF, sizeof(g_flash_write_ctx.staged_data));
    g_flash_write_ctx.staged_len = 0U;
}

/**
 * @brief  Flash 初始化
 * @return 0=成功, <0=失败
 */
static int flash_init(void)
{
    smota_port_flash_reset_write_ctx(0U);
    return 0;
}

/**
 * @brief  Flash 去初始化
 * @return 0=成功, <0=失败
 */
static int flash_deinit(void)
{
    return smota_port_flash_flush_staged_write();
}

/**
 * @brief  从下载区读取数据
 * @param  addr: 逻辑偏移
 * @param  data: 输出缓冲区
 * @param  size: 读取长度
 * @return 实际读取字节数，<0=失败
 */
static int flash_read(uint32_t addr, uint8_t *data, uint32_t size)
{
    int ret;

    if (data == NULL) {
        return -1;
    }

    if (addr > SMOTA_APP_SIZE || size > (SMOTA_APP_SIZE - addr)) {
        return -2;
    }

    ret = smota_port_flash_flush_staged_write();
    if (ret < 0) {
        return ret;
    }

    memcpy(data, (const void *)smota_port_addr_to_phys(addr), size);
    return (int)size;
}

/**
 * @brief  顺序写入下载区
 * @param  addr: 逻辑偏移
 * @param  data: 输入数据
 * @param  size: 写入长度
 * @return 实际写入字节数，<0=失败
 */
static int flash_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    uint32_t current_addr;
    uint32_t remaining;
    const uint8_t *src;
    int ret;

    if (data == NULL) {
        return -1;
    }

    if (addr > SMOTA_APP_SIZE || size > (SMOTA_APP_SIZE - addr)) {
        return -2;
    }

    if (addr != g_flash_write_ctx.next_addr) {
        return -3;
    }

    current_addr = addr;
    remaining = size;
    src = data;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -5;
    }

    while (remaining > 0U) {
        if (g_flash_write_ctx.staged_len > 0U) {
            while (remaining > 0U && g_flash_write_ctx.staged_len < 8U) {
                g_flash_write_ctx.staged_data[g_flash_write_ctx.staged_len] = *src;
                g_flash_write_ctx.staged_len++;
                g_flash_write_ctx.next_addr++;
                current_addr++;
                src++;
                remaining--;
            }

            if (g_flash_write_ctx.staged_len == 8U) {
                ret = smota_port_flash_program_doubleword(
                    smota_port_addr_to_phys(g_flash_write_ctx.staged_addr),
                    g_flash_write_ctx.staged_data);
                if (ret < 0) {
                    (void)HAL_FLASH_Lock();
                    return ret;
                }

                memset(g_flash_write_ctx.staged_data, 0xFF, sizeof(g_flash_write_ctx.staged_data));
                g_flash_write_ctx.staged_len = 0U;
                g_flash_write_ctx.staged_addr = current_addr;
            }

            continue;
        }

        if ((current_addr & 0x7U) != 0U) {
            (void)HAL_FLASH_Lock();
            return -4;
        }

        if (remaining >= 8U) {
            ret = smota_port_flash_program_doubleword(smota_port_addr_to_phys(current_addr), src);
            if (ret < 0) {
                (void)HAL_FLASH_Lock();
                return ret;
            }

            g_flash_write_ctx.next_addr += 8U;
            current_addr += 8U;
            src += 8U;
            remaining -= 8U;
            continue;
        }

        g_flash_write_ctx.staged_addr = current_addr;
        memset(g_flash_write_ctx.staged_data, 0xFF, sizeof(g_flash_write_ctx.staged_data));
        memcpy(g_flash_write_ctx.staged_data, src, remaining);
        g_flash_write_ctx.staged_len = (uint8_t)remaining;
        g_flash_write_ctx.next_addr += remaining;
        remaining = 0U;
    }

    (void)HAL_FLASH_Lock();
    return (int)size;
}

/**
 * @brief  擦除下载区
 * @param  addr: 逻辑偏移
 * @param  size: 擦除长度
 * @return 0=成功, <0=失败
 */
static int flash_erase(uint32_t addr, uint32_t size)
{
    int ret;

    if (addr > SMOTA_APP_SIZE || size > (SMOTA_APP_SIZE - addr)) {
        return -1;
    }

    smota_port_flash_reset_write_ctx(addr);

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -2;
    }

    ret = smota_port_flash_erase_pages(smota_port_addr_to_phys(addr), size);
    (void)HAL_FLASH_Lock();

    return ret;
}

/**
 * @brief  Flash 上锁
 * @return 0=成功, <0=失败
 */
static int flash_lock(void)
{
    return (HAL_FLASH_Lock() == HAL_OK) ? 0 : -1;
}

/**
 * @brief  Flash 解锁
 * @return 0=成功, <0=失败
 */
static int flash_unlock(void)
{
    return (HAL_FLASH_Unlock() == HAL_OK) ? 0 : -1;
}

/*---------- end of file ----------*/
