/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_handler.c
 * @Author       : lxf
 * @Date         : 2026-01-30 10:45:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-30 10:00:00
 * @Brief        : smOTA 协议处理函数实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "../../smota.h"
#include "../inc/smota_packet.h"
#include "../inc/smota_state.h"
#include "../inc/smota_config.h"
#include "smota_internal.h"

/*---------- macro ----------*/
#define SMOTA_VERIFY_READ_CHUNK_SIZE 256U
#define SMOTA_MAX_DATA_PAYLOAD_SIZE  256U
#define SMOTA_RESET_DELAY_MS         0U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static uint8_t smota_project_id_is_empty(const uint8_t project_id[16]);
static uint8_t smota_current_is_empty_boot(const struct smota_ctx *ctx);
static smota_err_t smota_verify_received_image(struct smota_ctx *ctx,
                                               struct smota_finish_resp *resp);

/*---------- variable ----------*/

/*---------- function ----------*/

static uint8_t smota_project_id_is_empty(const uint8_t project_id[16])
{
    uint8_t i;

    if (project_id == NULL) {
        return 1U;
    }

    for (i = 0; i < 16U; i++) {
        if (project_id[i] != 0U) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t smota_current_is_empty_boot(const struct smota_ctx *ctx)
{
    static const uint8_t boot_project_id[16] = "SMOTA_BOOT";

    if (ctx == NULL) {
        return 0U;
    }

    if (ctx->current_info.version[0] != 0U ||
        ctx->current_info.version[1] != 0U ||
        ctx->current_info.version[2] != 0U) {
        return 0U;
    }

    return (memcmp(ctx->current_info.project_id,
                   boot_project_id,
                   sizeof(ctx->current_info.project_id)) == 0) ? 1U : 0U;
}

static smota_err_t smota_verify_received_image(struct smota_ctx *ctx,
                                               struct smota_finish_resp *resp)
{
#if SMOTA_RELIABILITY_SOURCE || SMOTA_RELIABILITY_TRANSMISSION
    const struct smota_hal *hal;
    void *sha256_ctx;
    uint8_t hash[32];
    uint8_t buffer[SMOTA_VERIFY_READ_CHUNK_SIZE];
    uint32_t offset;
    int ret;

    if (ctx->verify_hash == 0U) {
        return SMOTA_ERR_OK;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || hal->crypto == NULL) {
        resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
        return SMOTA_ERR_INVALID_STATE;
    }

    sha256_ctx = hal->crypto->sha256_init();
    if (sha256_ctx == NULL) {
        resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
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
            resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
            return SMOTA_ERR_FLASH;
        }

        ret = hal->crypto->sha256_update(sha256_ctx, buffer, chunk_size);
        if (ret < 0) {
            resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
            return SMOTA_ERR_FLASH;
        }

        offset += chunk_size;
    }

    ret = hal->crypto->sha256_final(sha256_ctx, hash);
    if (ret < 0) {
        resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
        return SMOTA_ERR_FLASH;
    }

    if (memcmp(hash, ctx->expected_hash, sizeof(ctx->expected_hash)) != 0) {
        resp->error_code = SMOTA_PROTO_ERR_VERIFY_FAILED;
        return SMOTA_ERR_VERSION;
    }
#else
    (void)ctx;
    (void)resp;
#endif

    return SMOTA_ERR_OK;
}

smota_err_t smota_handle_query_req(const struct smota_query_req *req,
                                   struct smota_query_resp *resp)
{
    struct smota_ctx *ctx;
#if SMOTA_RELIABILITY_VERSION
    uint8_t requested_version[3];
#endif

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    resp->error_code = SMOTA_PROTO_ERR_OK;
    resp->allow_upgrade = 0U;
    resp->fw_version_major = ctx->current_info.version[0];
    resp->fw_version_minor = ctx->current_info.version[1];
    resp->fw_version_patch = ctx->current_info.version[2];
    memcpy(resp->project_id,
           ctx->current_info.project_id,
           sizeof(resp->project_id));

    ctx->query_allowed = 0U;
    memset(ctx->target_project_id, 0, sizeof(ctx->target_project_id));

    if ((req->flags & SMOTA_QUERY_FLAG_FORCE_UPGRADE) != 0U) {
        ctx->query_allowed = 1U;
        ctx->firmware_version[0] = req->fw_version_major;
        ctx->firmware_version[1] = req->fw_version_minor;
        ctx->firmware_version[2] = req->fw_version_patch;
        ctx->firmware_version[3] = 0U;
        memcpy(ctx->target_project_id, req->project_id, sizeof(ctx->target_project_id));
        resp->allow_upgrade = 1U;
        return SMOTA_ERR_OK;
    }

    if (smota_state_get() != SMOTA_STATE_IDLE) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_STATE;
        return SMOTA_ERR_OK;
    }

    if (smota_project_id_is_empty(req->project_id) != 0U) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_PARAM;
        return SMOTA_ERR_OK;
    }

    if (smota_current_is_empty_boot(ctx) == 0U &&
        memcmp(req->project_id,
               ctx->current_info.project_id,
               sizeof(ctx->current_info.project_id)) != 0) {
        resp->error_code = SMOTA_PROTO_ERR_PROJECT_ID_MISMATCH;
        return SMOTA_ERR_OK;
    }

#if SMOTA_RELIABILITY_VERSION
    requested_version[0] = req->fw_version_major;
    requested_version[1] = req->fw_version_minor;
    requested_version[2] = req->fw_version_patch;
    if (smota_current_is_empty_boot(ctx) == 0U &&
        !smota_verify_version(ctx->current_info.version, requested_version)) {
        resp->error_code = SMOTA_PROTO_ERR_VERSION_REJECTED;
        return SMOTA_ERR_OK;
    }
#endif

    ctx->query_allowed = 1U;
    ctx->firmware_version[0] = req->fw_version_major;
    ctx->firmware_version[1] = req->fw_version_minor;
    ctx->firmware_version[2] = req->fw_version_patch;
    ctx->firmware_version[3] = 0U;
    memcpy(ctx->target_project_id, req->project_id, sizeof(ctx->target_project_id));
    resp->allow_upgrade = 1U;

    return SMOTA_ERR_OK;
}

smota_err_t smota_handle_start_req(const struct smota_start_req *req,
                                   struct smota_start_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    int ret;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL) {
        resp->error_code = SMOTA_PROTO_ERR_FLASH_WRITE_FAILED;
        return SMOTA_ERR_INVALID_STATE;
    }

    ctx = smota_ctx_get();

    if (smota_state_get() != SMOTA_STATE_IDLE ||
        ctx->query_allowed == 0U) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    if (req->firmware_size > SMOTA_APP_SIZE) {
        resp->error_code = SMOTA_PROTO_ERR_FLASH_INSUFFICIENT;
        return SMOTA_ERR_SPACE;
    }

    ctx->firmware_size = req->firmware_size;
    ctx->received_size = 0;
    ctx->timeout_ms = req->block_timeout;
    ctx->reset_pending = 0;
    ctx->verify_hash = ((req->flags & SMOTA_START_FLAG_SHA256_VALID) != 0U) ? 1U : 0U;
    memcpy(ctx->expected_hash, req->sha256_hash, sizeof(ctx->expected_hash));

    ret = hal->flash->erase(0, ctx->firmware_size);
    if (ret < 0) {
        resp->error_code = SMOTA_PROTO_ERR_FLASH_WRITE_FAILED;
        return SMOTA_ERR_FLASH;
    }

    resp->error_code = SMOTA_PROTO_ERR_OK;
    resp->max_payload_size = SMOTA_MAX_DATA_PAYLOAD_SIZE;
    smota_state_set(SMOTA_STATE_STARTED);

    return SMOTA_ERR_OK;
}

smota_err_t smota_handle_data_req(const struct smota_data_req *req,
                                  const uint8_t *data,
                                  struct smota_data_resp *resp)
{
    struct smota_ctx *ctx;
    const struct smota_hal *hal;
    int ret;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_STARTED &&
        smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_STATE;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    hal = smota_hal_get();
    if (hal == NULL || hal->flash == NULL || data == NULL) {
        resp->error_code = SMOTA_PROTO_ERR_FLASH_WRITE_FAILED;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_STATE;
    }

    if (req->offset != ctx->received_size) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_PARAM;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_INVALID_PARAM;
    }

    if ((req->offset + req->length) > ctx->firmware_size) {
        resp->error_code = SMOTA_PROTO_ERR_LENGTH_ERROR;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_LENGTH;
    }

    ret = hal->flash->write(req->offset, data, req->length);
    if (ret != req->length) {
        resp->error_code = SMOTA_PROTO_ERR_FLASH_WRITE_FAILED;
        resp->received_offset = ctx->received_size;
        return SMOTA_ERR_FLASH;
    }

    ctx->received_size += req->length;
    resp->error_code = SMOTA_PROTO_ERR_OK;
    resp->received_offset = ctx->received_size;

    if (smota_state_get() == SMOTA_STATE_STARTED) {
        smota_state_set(SMOTA_STATE_TRANSFER);
    }

    return SMOTA_ERR_OK;
}

smota_err_t smota_handle_finish_req(const struct smota_finish_req *req,
                                    struct smota_finish_resp *resp)
{
    struct smota_ctx *ctx;
    smota_err_t ret;

    if (req == NULL || resp == NULL) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    ctx = smota_ctx_get();
    if (smota_state_get() != SMOTA_STATE_TRANSFER) {
        resp->error_code = SMOTA_PROTO_ERR_INVALID_STATE;
        return SMOTA_ERR_INVALID_STATE;
    }

    if (req->total_size != ctx->firmware_size ||
        ctx->received_size != ctx->firmware_size) {
        resp->error_code = SMOTA_PROTO_ERR_LENGTH_ERROR;
        return SMOTA_ERR_LENGTH;
    }

    ret = smota_verify_received_image(ctx, resp);
    if (ret != SMOTA_ERR_OK) {
        return ret;
    }

    resp->error_code = SMOTA_PROTO_ERR_OK;
    resp->reset_delay_ms = SMOTA_RESET_DELAY_MS;
    smota_state_set(SMOTA_STATE_FINISHED);
    ctx->reset_pending = 1;

    return SMOTA_ERR_OK;
}

/*---------- end of file ----------*/
