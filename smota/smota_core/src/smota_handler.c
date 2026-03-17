/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_handler.c
 * @Author       : lxf
 * @Date         : 2026-01-30 10:45:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-03-17 14:30:00
 * @Brief        : smOTA 协议处理函数实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "../../smota.h"
#include "../inc/smota_packet.h"
#include "../inc/smota_state.h"
#include "../inc/smota_config.h"

/*---------- macro ----------*/
#define SMOTA_VERIFY_READ_CHUNK_SIZE 256U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
static uint8_t g_resp_buffer[256];
static void *g_resp_buffer_used __attribute__((unused)) = g_resp_buffer;

/*---------- function ----------*/

/**
 * @brief       处理握手请求 (0x01)
 * @param[in]   req: 握手请求结构体
 * @param[out]  resp: 握手响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_handshake_req(const struct smota_handshake_req *req,
                                       struct smota_handshake_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
#if SMOTA_RELIABILITY_VERSION
    uint8_t requested_version[3];
#endif
    int i;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_INVALID_STATE;
    }

    ctx = smota_ctx_get();

    for (i = 0; i < 16; i++) {
        if (req->project_id[i] != 0U) {
            break;
        }
    }
    (void)i;

#if SMOTA_RELIABILITY_VERSION
    requested_version[0] = req->fw_version_major;
    requested_version[1] = req->fw_version_minor;
    requested_version[2] = req->fw_version_patch;
    if (!smota_verify_version(ctx->current_version, requested_version)) {
        resp->error_code = SMOTA_ERR_VERSION_MISMATCH;
        return SMOTA_ERR_VERSION;
    }
#endif

    if (req->firmware_size > SMOTA_FLASH_SIZE) {
        resp->error_code = SMOTA_ERR_FLASH_INSUFFICIENT;
        return SMOTA_ERR_SPACE;
    }

    ctx->firmware_size = req->firmware_size;
    ctx->received_size = 0;
    ctx->firmware_version[0] = req->fw_version_major;
    ctx->firmware_version[1] = req->fw_version_minor;
    ctx->firmware_version[2] = req->fw_version_patch;
    ctx->firmware_version[3] = 0;
    ctx->timeout_ms = req->block_timeout;
    ctx->reset_pending = 0;
    memset(ctx->expected_hash, 0, sizeof(ctx->expected_hash));
    memset(ctx->signature_r, 0, sizeof(ctx->signature_r));
    memset(ctx->signature_s, 0, sizeof(ctx->signature_s));

    resp->error_code = 0;
    resp->next_offset = 0;
    resp->max_packet_size = 256;
    resp->mtu_size = 512;
    resp->flash_free_size = SMOTA_FLASH_SIZE;
    resp->block_timeout = req->block_timeout;
    resp->install_timeout = req->install_timeout;
    resp->capabilities = SMOTA_CAP_ANTI_ROLLBACK;

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

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_HANDSHAKE) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || hal->crypto == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_INVALID_STATE;
    }

    memcpy(ctx->expected_hash, req->sha256_hash, sizeof(ctx->expected_hash));
    memcpy(ctx->signature_r, req->signature_r, sizeof(ctx->signature_r));
    memcpy(ctx->signature_s, req->signature_s, sizeof(ctx->signature_s));
    ctx->received_size = 0;

    ret = hal->flash->erase(0, ctx->firmware_size);
    if (ret < 0) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        return SMOTA_ERR_FLASH;
    }

    resp->error_code = 0;
    smota_state_set(SMOTA_STATE_HEADER_INFO);

    return SMOTA_ERR_OK;
}

/**
 * @brief       处理数据块请求 (0x03)
 * @param[in]   req: 数据块请求结构体
 * @param[in]   data: 数据块内容指针
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

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_HEADER_INFO &&
        smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    if (req->offset != ctx->received_size) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_PARAM;
    }

    if ((req->offset + req->length) > ctx->firmware_size) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_LENGTH;
    }

    ret = hal->flash->write(req->offset, data, req->length);
    if (ret != req->length) {
        resp->error_code = SMOTA_ERR_FLASH_WRITE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_FLASH;
    }

    ctx->received_size += req->length;

    resp->error_code = 0;
    resp->received_offset = ctx->received_size;

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
    uint8_t buffer[SMOTA_VERIFY_READ_CHUNK_SIZE];
    uint32_t offset;
    int ret;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    if (req->total_size != ctx->firmware_size) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_VERSION;
    }

    if (ctx->received_size != ctx->firmware_size) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_LENGTH;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || hal->crypto == NULL) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_INVALID_STATE;
    }

    sha256_ctx = hal->crypto->sha256_init();
    if (sha256_ctx == NULL) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_FLASH;
    }

    offset = 0;
    while (offset < ctx->received_size) {
        uint32_t chunk_size = sizeof(buffer);
        if ((ctx->received_size - offset) < chunk_size) {
            chunk_size = ctx->received_size - offset;
        }

        ret = hal->flash->read(offset, buffer, chunk_size);
        if (ret != (int)chunk_size) {
            resp->error_code = SMOTA_ERR_INSTALL_FLASH_READ;
            return SMOTA_ERR_FLASH;
        }

        ret = hal->crypto->sha256_update(sha256_ctx, buffer, chunk_size);
        if (ret < 0) {
            resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
            return SMOTA_ERR_FLASH;
        }

        offset += chunk_size;
    }

    ret = hal->crypto->sha256_final(sha256_ctx, hash);
    if (ret < 0) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_FLASH;
    }

    if (memcmp(hash, ctx->expected_hash, sizeof(ctx->expected_hash)) != 0) {
        resp->error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
        return SMOTA_ERR_VERSION;
    }

    resp->error_code = 0;
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
    struct smota_ctx *ctx;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    if (smota_state_get() != SMOTA_STATE_COMPLETE) {
        resp->error_code = SMOTA_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    ctx = smota_ctx_get();

    resp->error_code = 0;
    resp->estimated_time_s = 5;

    smota_state_set(SMOTA_STATE_INSTALL);
    ctx->reset_pending = 1;

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
    struct smota_ctx *ctx;

    if (resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    (void)req;
    ctx = smota_ctx_get();

    resp->error_code = 0;
    resp->fw_version_major = ctx->current_version[0];
    resp->fw_version_minor = ctx->current_version[1];
    resp->fw_version_patch = ctx->current_version[2];

    return SMOTA_ERR_OK;
}

/*---------- end of file ----------*/
