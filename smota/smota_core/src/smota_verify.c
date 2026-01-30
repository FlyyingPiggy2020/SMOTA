/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_verify.c
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 11:30:00
 * @Brief        : smOTA 验签实现
 */

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "smota_verify.h"
#include "../smota_hal/smota_hal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/

/*---------- function ----------*/

/**
 * @brief       开始 SHA-256 计算
 * @param[out]  ctx: SHA-256 上下文指针
 * @return      0=成功, <0=失败
 */
int smota_sha256_start(struct smota_sha256_ctx *ctx)
{
    const struct smota_hal *hal;
    const struct smota_crypto_driver *crypto;

    if (ctx == NULL) {
        return -1;
    }

    /* 获取 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->crypto == NULL) {
        return -2;
    }

    crypto = hal->crypto;

    /* 初始化 HAL 上下文 */
    ctx->hal_ctx = crypto->sha256_init();
    if (ctx->hal_ctx == NULL) {
        return -3;
    }

    ctx->total_size = 0;

    return 0;
}

/**
 * @brief       更新 SHA-256 计算
 * @param[in]   ctx: SHA-256 上下文指针
 * @param[in]   data: 待计算数据
 * @param[in]   size: 数据长度
 * @return      0=成功, <0=失败
 */
int smota_sha256_update(struct smota_sha256_ctx *ctx, const uint8_t *data, uint32_t size)
{
    const struct smota_hal *hal;
    const struct smota_crypto_driver *crypto;
    int ret;

    if (ctx == NULL || ctx->hal_ctx == NULL) {
        return -1;
    }

    if (data == NULL || size == 0) {
        return 0;  /* 空数据，直接返回 */
    }

    /* 获取 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->crypto == NULL) {
        return -2;
    }

    crypto = hal->crypto;

    /* 调用 HAL 更新 */
    ret = crypto->sha256_update(ctx->hal_ctx, data, size);
    if (ret < 0) {
        return -3;
    }

    ctx->total_size += size;

    return 0;
}

/**
 * @brief       完成 SHA-256 计算
 * @param[in]   ctx: SHA-256 上下文指针
 * @param[out]  hash: 输出哈希值（32字节）
 * @return      0=成功, <0=失败
 */
int smota_sha256_final(struct smota_sha256_ctx *ctx, uint8_t hash[32])
{
    const struct smota_hal *hal;
    const struct smota_crypto_driver *crypto;
    int ret;

    if (ctx == NULL || ctx->hal_ctx == NULL || hash == NULL) {
        return -1;
    }

    /* 获取 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->crypto == NULL) {
        return -2;
    }

    crypto = hal->crypto;

    /* 调用 HAL 完成 */
    ret = crypto->sha256_final(ctx->hal_ctx, hash);
    if (ret < 0) {
        return -3;
    }

    return 0;
}

/**
 * @brief       验证版本号（防回滚）
 * @param[in]   current_version: 当前版本号[major, minor, patch]
 * @param[in]   new_version: 新版本号[major, minor, patch]
 * @return      true=允许升级, false=拒绝（新版本低于当前版本）
 */
bool smota_verify_version(const uint8_t current_version[3], const uint8_t new_version[3])
{
    /* 主版本比较 */
    if (new_version[0] > current_version[0]) {
        return true;
    }
    if (new_version[0] < current_version[0]) {
        return false;
    }

    /* 次版本比较 */
    if (new_version[1] > current_version[1]) {
        return true;
    }
    if (new_version[1] < current_version[1]) {
        return false;
    }

    /* 补丁版本比较 */
    if (new_version[2] >= current_version[2]) {
        return true;
    }

    return false;
}

/**
 * @brief       比较两个哈希值是否相等
 * @param[in]   hash1: 哈希值1（32字节）
 * @param[in]   hash2: 哈希值2（32字节）
 * @return      true=相等, false=不相等
 */
bool smota_verify_hash_equal(const uint8_t hash1[32], const uint8_t hash2[32])
{
    if (hash1 == NULL || hash2 == NULL) {
        return false;
    }

    return (memcmp(hash1, hash2, 32) == 0);
}

/**
 * @brief       快速计算数据的 SHA-256 哈希
 * @param[in]   data: 待计算数据
 * @param[in]   size: 数据长度
 * @param[out]  hash: 输出哈希值（32字节）
 * @return      0=成功, <0=失败
 */
int smota_sha256_compute(const uint8_t *data, uint32_t size, uint8_t hash[32])
{
    struct smota_sha256_ctx ctx;
    int ret;

    ret = smota_sha256_start(&ctx);
    if (ret < 0) {
        return ret;
    }

    ret = smota_sha256_update(&ctx, data, size);
    if (ret < 0) {
        return ret;
    }

    ret = smota_sha256_final(&ctx, hash);

    return ret;
}

/*---------- end of file ----------*/
