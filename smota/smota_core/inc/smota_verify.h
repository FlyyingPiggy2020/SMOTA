/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_verify.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 09:57:46
 * @Brief        : smOTA 验签模块定义
 */

#ifndef SMOTA_VERIFY_H
#define SMOTA_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include "smota_types.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/**
 * @brief  SHA-256 上下文结构体（HAL 抽象）
 * @note   实际实现由 HAL crypto 驱动提供
 */
struct smota_sha256_ctx {
    void *hal_ctx;   /* HAL 上下文指针 */
    uint32_t total_size;  /* 已处理数据总大小 */
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief       开始 SHA-256 计算
 * @param[out]  ctx: SHA-256 上下文指针
 * @return      0=成功, <0=失败
 */
int smota_sha256_start(struct smota_sha256_ctx *ctx);

/**
 * @brief       更新 SHA-256 计算
 * @param[in]   ctx: SHA-256 上下文指针
 * @param[in]   data: 待计算数据
 * @param[in]   size: 数据长度
 * @return      0=成功, <0=失败
 */
int smota_sha256_update(struct smota_sha256_ctx *ctx, const uint8_t *data, uint32_t size);

/**
 * @brief       完成 SHA-256 计算
 * @param[in]   ctx: SHA-256 上下文指针
 * @param[out]  hash: 输出哈希值（32字节）
 * @return      0=成功, <0=失败
 */
int smota_sha256_final(struct smota_sha256_ctx *ctx, uint8_t hash[32]);

/**
 * @brief       验证版本号（防回滚）
 * @param[in]   current_version: 当前版本号[major, minor, patch]
 * @param[in]   new_version: 新版本号[major, minor, patch]
 * @return      true=允许升级, false=拒绝（新版本低于当前版本）
 */
bool smota_verify_version(const uint8_t current_version[3], const uint8_t new_version[3]);

/**
 * @brief       比较两个哈希值是否相等
 * @param[in]   hash1: 哈希值1（32字节）
 * @param[in]   hash2: 哈希值2（32字节）
 * @return      true=相等, false=不相等
 */
bool smota_verify_hash_equal(const uint8_t hash1[32], const uint8_t hash2[32]);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_VERIFY_H
