/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_hal.h
 * @Author       : lxf
 * @Date         : 2026-01-29 14:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 14:00:00
 * @Brief        : smOTA HAL 抽象层接口定义
 * @details      定义了 smOTA 与硬件平台无关的 HAL 接口
 *              用户需根据具体平台实现这些接口函数
 */

#ifndef SMOTA_HAL_H
#define SMOTA_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include <stddef.h>

/*---------- macro ----------*/

/**
 * @brief  HAL 版本号
 */
#define SMOTA_HAL_VERSION_MAJOR  1
#define SMOTA_HAL_VERSION_MINOR  0
#define SMOTA_HAL_VERSION_PATCH  0

/*---------- type define ----------*/

/**
 * @brief  Flash 操作驱动接口
 * @details 提供 Flash 的读写擦除操作
 */
struct smota_flash_driver {
    /**
     * @brief  初始化 Flash 驱动
     * @return 0=成功, <0=失败
     */
    int (*init)(void);

    /**
     * @brief  去初始化 Flash 驱动
     * @return 0=成功, <0=失败
     */
    int (*deinit)(void);

    /**
     * @brief  读取 Flash 数据
     * @param  addr: Flash 地址（绝对地址，如 0x08010000）
     * @param  data: 数据缓冲区
     * @param  size: 读取字节数
     * @return 实际读取字节数，<0=失败
     */
    int (*read)(uint32_t addr, uint8_t *data, uint32_t size);

    /**
     * @brief  写入 Flash 数据
     * @param  addr: Flash 地址（绝对地址，如 0x08010000）
     * @param  data: 数据缓冲区
     * @param  size: 写入字节数
     * @return 实际写入字节数，<0=失败
     * @note   写入前需确保目标区域已擦除
     */
    int (*write)(uint32_t addr, const uint8_t *data, uint32_t size);

    /**
     * @brief  擦除 Flash 区域
     * @param  addr: Flash 起始地址（绝对地址）
     * @param  size: 擦除字节数（需对齐到页/扇区）
     * @return 0=成功, <0=失败
     * @note   地址和大小应对齐到页边界
     */
    int (*erase)(uint32_t addr, uint32_t size);

    /**
     * @brief  上锁 Flash 写保护
     * @return 0=成功, <0=失败
     */
    int (*flash_lock)(void);

    /**
     * @brief  解锁 Flash 写保护
     * @return 0=成功, <0=失败
     */
    int (*flash_unlock)(void);
};

/**
 * @brief  通信驱动接口
 * @details 支持多种通信方式（UART/CAN/BLE等）
 */
struct smota_comm_driver {
    /**
     * @brief  初始化通信接口
     * @return 0=成功, <0=失败
     */
    int (*init)(void);

    /**
     * @brief  去初始化通信接口
     * @return 0=成功, <0=失败
     */
    int (*deinit)(void);

    /**
     * @brief  发送数据
     * @param  data: 待发送数据
     * @param  size: 数据长度
     * @return 实际发送字节数，<0=失败
     */
    int (*send)(const uint8_t *data, uint32_t size);

    /**
     * @brief  接收数据
     * @param  data: 接收缓冲区
     * @param  size: 期望接收字节数
     * @param  timeout: 超时时间（毫秒）
     * @return 实际接收字节数，<0=失败/超时
     */
    int (*receive)(uint8_t *data, uint32_t size, uint32_t timeout);
};

/**
 * @brief  加密算法驱动接口
 * @details 提供 SHA-256/AES/ECDSA 等加密算法
 */
struct smota_crypto_driver {
    /* ========== SHA-256 ========== */

    /**
     * @brief  初始化 SHA-256 上下文
     * @return 上下文指针，NULL=失败
     */
    void *(*sha256_init)(void);

    /**
     * @brief  更新 SHA-256 计算
     * @param  ctx: 上下文指针
     * @param  data: 待计算数据
     * @param  size: 数据长度
     * @return 0=成功, <0=失败
     */
    int (*sha256_update)(void *ctx, const uint8_t *data, uint32_t size);

    /**
     * @brief  完成 SHA-256 计算
     * @param  ctx: 上下文指针
     * @param  hash: 输出哈希值（32字节）
     * @return 0=成功, <0=失败
     */
    int (*sha256_final)(void *ctx, uint8_t hash[32]);

    /* ========== AES-128-CTR ========== */

    /**
     * @brief  初始化 AES-128-CTR 上下文
     * @param  key: 密钥（16字节）
     * @param  iv: 初始化向量（16字节）
     * @return 上下文指针，NULL=失败
     */
    void *(*aes_init)(const uint8_t *key, const uint8_t *iv);

    /**
     * @brief  AES 加密/解密（CTR模式）
     * @param  ctx: 上下文指针
     * @param  input: 输入数据
     * @param  output: 输出数据
     * @param  size: 数据长度
     * @return 0=成功, <0=失败
     * @note   CTR模式下加密和解密使用同一函数
     */
    int (*aes_crypt)(void *ctx, const uint8_t *input, uint8_t *output, uint32_t size);

    /* ========== ECDSA-P256 ========== */

    /**
     * @brief  验证 ECDSA-P256 签名
     * @param  hash: 消息哈希（32字节）
     * @param  sig_r: 签名 r 分量（32字节）
     * @param  sig_s: 签名 s 分量（32字节）
     * @param  pub_key: 公钥（64字节：x + y）
     * @return 0=验证成功, <0=验证失败
     */
    int (*ecdsa_verify)(const uint8_t *hash,
                        const uint8_t *sig_r,
                        const uint8_t *sig_s,
                        const uint8_t *pub_key);
};

/**
 * @brief  系统相关接口
 * @details 提供系统时间、延时、复位等功能
 */
struct smota_system_driver {
    /**
     * @brief  获取系统滴答计数（毫秒）
     * @return 系统运行时间（毫秒）
     */
    uint64_t (*get_tick_ms)(void);

    /**
     * @brief  系统复位
     * @note   此函数不会返回
     */
    void (*system_reset)(void);
};

/**
 * @brief  smOTA HAL 综合接口
 * @details 包含所有驱动接口，通过此结构体注册平台实现
 */
struct smota_hal {
    struct smota_flash_driver   *flash;
    struct smota_comm_driver    *comm;
    struct smota_crypto_driver  *crypto;
    struct smota_system_driver  *system;
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief  注册并初始化 HAL 接口
 * @param  hal: HAL 结构体指针
 * @return 0=成功, <0=失败
 * @note   必须在使用任何 HAL 功能前调用
 */
int smota_hal_register(const struct smota_hal *hal);

/**
 * @brief  获取已注册的 HAL 接口
 * @return HAL 结构体指针，NULL=未初始化
 */
const struct smota_hal *smota_hal_get(void);

/**
 * @brief  去注册并清理 HAL 接口
 * @return 0=成功, <0=失败
 */
int smota_hal_unregister(void);

/**
 * @brief  检查 HAL 是否已初始化
 * @return 1=已初始化, 0=未初始化
 */
int smota_hal_is_initialized(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_HAL_H
