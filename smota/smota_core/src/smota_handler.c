/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_handler.c
 * @Author       : lxf
 * @Date         : 2026-01-30 10:45:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 10:45:00
 * @Brief        : smOTA 协议处理函数实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "../../smota.h"
#include "smota_packet.h"
#include "smota_state.h"
#include "smota_config.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
/**
 * @brief  响应缓冲区（用于发送响应帧）
 */
static uint8_t g_resp_buffer[256];

/*---------- function ----------*/

/**
 * @brief       处理握手请求 (0x01)
 * @param[in]   req: 握手请求结构体
 * @param[out]  resp: 握手响应结构体（需用户填充）
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_handshake_req(const struct smota_handshake_req *req,
                                        struct smota_handshake_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    uint32_t free_size;
    int i;

    /* 参数检查 */
    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查 HAL 是否已初始化 */
    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_INVALID_STATE;
    }

    ctx = smota_ctx_get();

    /* 验证项目 ID（简单比较，取较短长度） */
    for (i = 0; i < 16; i++) {
        if (req->project_id[i] != 0) {
            break;
        }
    }
    /* TODO: 实际项目 ID 验证逻辑 */

    /* 验证版本号（防回滚） */
    /* TODO: 从设备信息获取当前版本进行比较 */

    /* 检查 Flash 空间是否足够 */
    /* 计算可用空间（简化处理，实际应从 HAL 获取） */
    free_size = 64 * 1024;  /* 假设可用 64KB */
    resp->flash_free_size = free_size;

    if (req->firmware_size > free_size) {
        resp->error_code = SMOTA_ERR_FLASH_INSUFFICIENT;
        return SMOTA_ERR_SPACE;
    }

    /* 更新上下文 */
    ctx->firmware_size = req->firmware_size;
    ctx->firmware_version[0] = req->fw_version_major;
    ctx->firmware_version[1] = req->fw_version_minor;
    ctx->firmware_version[2] = req->fw_version_patch;
    ctx->timeout_ms = req->block_timeout;

    /* 填充响应 */
    resp->error_code = 0;
    resp->next_offset = 0;  /* 断点续传偏移 */
    resp->max_packet_size = 256;  /* 最大包大小 */
    resp->mtu_size = 512;         /* MTU 大小 */
    resp->block_timeout = req->block_timeout;  /* 确认超时 */
    resp->install_timeout = req->install_timeout;
    resp->capabilities = SMOTA_CAP_ANTI_ROLLBACK;  /* 设备能力 */

    /* 切换到握手状态 */
    smota_state_set(SMOTA_STATE_HANDSHAKE);

    return SMOTA_ERR_OK;
}

/**
 * @brief       处理头部信息请求 (0x02)
 * @param[in]   req: 头部信息请求结构体
 * @param[out]  resp: 头部信息响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_header_info_req(const struct smota_header_info_req *req,
                                          struct smota_header_info_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    int ret;

    /* 参数检查 */
    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查状态 */
    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_HANDSHAKE) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 检查 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || hal->crypto == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 保存 SHA-256 哈希值 */
    memcpy(ctx->recv_buffer, req->sha256_hash, 32);

    /* 擦除 Flash 目标区域 */
    ret = hal->flash->erase(0, ctx->firmware_size);
    if (ret < 0) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_FLASH;
    }

    /* 初始化 SHA-256 上下文 */
    ctx->recv_len = 0;

    /* 填充响应 */
    resp->error_code = 0;

    /* 切换到头部信息状态 */
    smota_state_set(SMOTA_STATE_HEADER_INFO);

    return SMOTA_ERR_OK;
}

/**
 * @brief       处理数据块请求 (0x03)
 * @param[in]   req: 数据块请求结构体
 * @param[in]   data: 数据指针（指向 req 后的数据区）
 * @param[out]  resp: 数据块响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_data_block_req(const struct smota_data_block_req *req,
                                         const uint8_t *data,
                                         struct smota_data_block_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    int ret;

    /* 参数检查 */
    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查状态 */
    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_HEADER_INFO &&
        smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 检查 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 验证偏移量连续性 */
    if (req->offset != ctx->received_size) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 写入 Flash */
    ret = hal->flash->write(req->offset, data, req->length);
    if (ret != req->length) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_FLASH;
    }

    /* 更新接收进度 */
    ctx->received_size += req->length;
    ctx->recv_len = 0;

    /* 填充响应 */
    resp->error_code = 0;
    resp->received_offset = ctx->received_size;

    /* 切换到传输状态 */
    if (smota_state_get() == SMOTA_STATE_HEADER_INFO) {
        smota_state_set(SMOTA_STATE_TRANSFER);
    }

    return SMOTA_ERR_OK;
}

/**
 * @brief       处理传输完成请求 (0x04)
 * @param[in]   req: 传输完成请求结构体
 * @param[out]  resp: 传输完成响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_transfer_complete_req(const struct smota_transfer_complete_req *req,
                                                struct smota_transfer_complete_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    void *sha256_ctx;
    uint8_t hash[32];
    int ret;

    /* 参数检查 */
    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查状态 */
    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 验证总大小 */
    if (req->total_size != ctx->firmware_size) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_VERSION;
    }

    /* 检查 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->crypto == NULL) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 重新计算 SHA-256 验证 */
    sha256_ctx = hal->crypto->sha256_init();
    if (sha256_ctx == NULL) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_FLASH;
    }

    /* 读取并计算整个固件的 SHA-256 */
    /* TODO: 分块读取大文件进行哈希计算 */

    ret = hal->crypto->sha256_final(sha256_ctx, hash);
    if (ret < 0) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_FLASH;
    }

    /* 比较哈希值 */
    if (memcmp(hash, ctx->recv_buffer, 32) != 0) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_VERSION;
    }

    /* 填充响应 */
    resp->error_code = 0;

    /* 切换到完成状态 */
    smota_state_set(SMOTA_STATE_COMPLETE);

    return SMOTA_ERR_OK;
}

/**
 * @brief       处理安装请求 (0x05)
 * @param[in]   req: 安装请求结构体
 * @param[out]  resp: 安装响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_install_req(const struct smota_install_req *req,
                                      struct smota_install_resp *resp)
{
    const struct smota_hal *hal;

    /* 参数检查 */
    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查状态 */
    if (smota_state_get() != SMOTA_STATE_COMPLETE) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 检查 HAL */
    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || hal->system == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 检查电池电量（可选） */
    /* TODO: 实现电池电量检查 */

    /* 检查业务状态（可选） */
    /* TODO: 实现业务状态检查 */

    /* 设置安装标志位 */
    /* TODO: 根据 SMOTA_MODE 设置相应的标志 */

    /* 填充响应 */
    resp->error_code = 0;
    resp->estimated_time_s = 5;  /* 预估 5 秒 */

    /* 切换到安装状态 */
    smota_state_set(SMOTA_STATE_INSTALL);

    /* 执行系统复位 */
    hal->system->system_reset();

    /* 不会执行到这里 */
    return SMOTA_ERR_OK;
}

/**
 * @brief       处理激活检查请求 (0x06)
 * @param[in]   req: 激活检查请求结构体
 * @param[out]  resp: 激活检查响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_activate_check_req(const struct smota_activate_check_req *req,
                                             struct smota_activate_check_resp *resp)
{
    /* 参数检查 */
    if (resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 填充响应（实际版本号应从设备读取） */
    resp->error_code = 0;
    resp->fw_version_major = 1;  /* TODO: 从实际固件读取 */
    resp->fw_version_minor = 0;
    resp->fw_version_patch = 0;

    /* 切换到激活状态 */
    smota_state_set(SMOTA_STATE_ACTIVATE);

    return SMOTA_ERR_OK;
}

/*---------- end of file ----------*/
