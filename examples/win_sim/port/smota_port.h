/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_port.h
 * @Author       : lxf
 * @Date         : 2026-01-29 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 18:20:00
 * @Brief        : smOTA Windows 模拟平台 HAL 驱动函数声明
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

/*---------- Flash 驱动函数 ----------*/

int flash_init(void);
int flash_deinit(void);
int flash_read(uint32_t addr, uint8_t *data, uint32_t size);
int flash_write(uint32_t addr, const uint8_t *data, uint32_t size);
int flash_erase(uint32_t addr, uint32_t size);
int flash_lock(void);
int flash_unlock(void);

/*---------- 通信驱动函数 ----------*/

int comm_init(void);
int comm_deinit(void);
int comm_send(const uint8_t *data, uint32_t size);
int comm_receive(uint8_t *data, uint32_t size, uint32_t timeout);

/*---------- 系统驱动函数 ----------*/

uint64_t system_get_tick_ms(void);
void system_reset(void);

/*---------- TinyCrypt 加密驱动端口函数 ----------*/

/**
 * @brief  TinyCrypt SHA256 初始化 (端口封装)
 * @return 上下文指针，NULL=失败
 */
void *tc_port_sha256_init(void);

/**
 * @brief  TinyCrypt SHA256 更新 (端口封装)
 * @param  ctx: 上下文指针
 * @param  data: 待计算数据
 * @param  size: 数据长度
 * @return 0=成功, <0=失败
 */
int tc_port_sha256_update(void *ctx, const uint8_t *data, uint32_t size);

/**
 * @brief  TinyCrypt SHA256 完成 (端口封装)
 * @param  ctx: 上下文指针
 * @param  hash: 输出哈希值（32字节）
 * @return 0=成功, <0=失败
 */
int tc_port_sha256_final(void *ctx, uint8_t hash[32]);

/**
 * @brief  TinyCrypt AES-128-CTR 初始化 (端口封装)
 * @param  key: 密钥（16字节）
 * @param  iv: 初始化向量（16字节）
 * @return 上下文指针，NULL=失败
 */
void *tc_port_aes_init(const uint8_t *key, const uint8_t *iv);

/**
 * @brief  TinyCrypt AES-128-CTR 加密/解密 (端口封装)
 * @param  ctx: 上下文指针
 * @param  input: 输入数据
 * @param  output: 输出数据
 * @param  size: 数据长度
 * @return 0=成功, <0=失败
 */
int tc_port_aes_crypt(void *ctx, const uint8_t *input, uint8_t *output, uint32_t size);

/**
 * @brief  TinyCrypt ECDSA-P256 签名验证 (端口封装)
 * @param  hash: 消息哈希（32字节）
 * @param  sig_r: 签名 r 分量（32字节）
 * @param  sig_s: 签名 s 分量（32字节）
 * @param  pub_key: 公钥（64字节：x + y）
 * @return 0=验证成功, <0=验证失败
 */
int tc_port_ecdsa_verify(const uint8_t *hash,
                         const uint8_t *sig_r,
                         const uint8_t *sig_s,
                         const uint8_t *pub_key);

/*---------- KDF 密钥派生函数 ----------*/

/**
 * @brief  基于 HMAC-SHA256 的密钥派生函数
 * @details 算法: HMAC-SHA256(master_key, uid || context)
 * @param  master_key: 主密钥 (32字节)
 * @param  uid: 设备唯一标识
 * @param  uid_len: UID 长度
 * @param  context: 上下文字符串 (如 "smOTA_Enc_v1")
 * @param  output: 输出的派生密钥 (32字节)
 * @return 0=成功, <0=失败
 */
int smota_kdf_derive(const uint8_t *master_key,
                     const uint8_t *uid, uint32_t uid_len,
                     const char *context,
                     uint8_t output[32]);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_PORT_H
