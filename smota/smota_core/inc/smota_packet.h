/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_packet.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-30 10:00:00
 * @Brief        : smOTA 包解析定义
 */

#ifndef SMOTA_PACKET_H
#define SMOTA_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/
#include <stdint.h>
#include <stddef.h>
#include "smota_types.h"

/*---------- macro ----------*/
#define SMOTA_SOF                      "smOTA"
#define SMOTA_SOF_SIZE                 5
#define SMOTA_FRAME_HEADER_SIZE        8
#define SMOTA_MAX_PAYLOAD_LEN          4096

#define SMOTA_CMD_QUERY                0x01
#define SMOTA_CMD_START                0x02
#define SMOTA_CMD_DATA                 0x03
#define SMOTA_CMD_FINISH               0x04

#define SMOTA_CMD_RESPONSE_FLAG        0x80
#define SMOTA_CMD_QUERY_RESP           (SMOTA_CMD_QUERY | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_START_RESP           (SMOTA_CMD_START | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_DATA_RESP            (SMOTA_CMD_DATA | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_FINISH_RESP          (SMOTA_CMD_FINISH | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_RESP(cmd)            ((cmd) | SMOTA_CMD_RESPONSE_FLAG)

#define SMOTA_PROTO_ERR_OK                     0U
#define SMOTA_PROTO_ERR_INVALID_STATE          1U
#define SMOTA_PROTO_ERR_INVALID_PARAM          2U
#define SMOTA_PROTO_ERR_PROJECT_ID_MISMATCH    3U
#define SMOTA_PROTO_ERR_VERSION_REJECTED       4U
#define SMOTA_PROTO_ERR_FLASH_INSUFFICIENT     5U
#define SMOTA_PROTO_ERR_FLASH_WRITE_FAILED     6U
#define SMOTA_PROTO_ERR_VERIFY_FAILED          7U
#define SMOTA_PROTO_ERR_LENGTH_ERROR           8U

#define SMOTA_QUERY_FLAG_FORCE_UPGRADE         (1U << 0)
#define SMOTA_START_FLAG_SHA256_VALID          (1U << 0)

/*---------- type define ----------*/
#pragma pack(push, 1)

struct smota_version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t reserved;
};

#define SMOTA_VERSION_COMPARE(v1, v2) \
    (((v1).major > (v2).major) ? 1 : \
     ((v1).major < (v2).major) ? -1 : \
     ((v1).minor > (v2).minor) ? 1 : \
     ((v1).minor < (v2).minor) ? -1 : \
     ((v1).patch > (v2).patch) ? 1 : \
     ((v1).patch < (v2).patch) ? -1 : 0)

struct smota_frame_header {
    uint8_t sof[5];
    uint8_t cmd;
    uint16_t length;
};

struct smota_frame {
    struct smota_frame_header header;
    uint8_t *payload;
    uint16_t crc16;
};

struct smota_query_req {
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    uint8_t fw_version_patch;
    uint8_t flags;
    uint8_t project_id[16];
};

struct smota_query_resp {
    uint32_t error_code;
    uint8_t allow_upgrade;
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    uint8_t fw_version_patch;
    uint8_t project_id[16];
};

struct smota_start_req {
    uint8_t flags;
    uint32_t firmware_size;
    uint8_t sha256_hash[32];
    uint16_t block_timeout;
};

struct smota_start_resp {
    uint32_t error_code;
    uint16_t max_payload_size;
};

struct smota_data_req {
    uint32_t offset;
    uint16_t length;
    uint8_t data[];
};

struct smota_data_resp {
    uint32_t error_code;
    uint32_t received_offset;
};

struct smota_finish_req {
    uint32_t total_size;
};

struct smota_finish_resp {
    uint32_t error_code;
    uint16_t reset_delay_ms;
};

#pragma pack(pop)

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
uint16_t smota_crc16_compute(const uint8_t *data, uint16_t len);
int smota_crc16_verify(const uint8_t *frame, uint16_t len);
smota_err_t smota_frame_parse(const uint8_t *data, uint16_t len, struct smota_frame *frame);
int smota_frame_build(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                      uint8_t *buffer, uint16_t buflen);
uint8_t smota_cmd_to_response(uint8_t req_cmd);
int smota_find_sof(const uint8_t *data, uint16_t len);

smota_err_t smota_handle_query_req(const struct smota_query_req *req,
                                    struct smota_query_resp *resp);
smota_err_t smota_handle_start_req(const struct smota_start_req *req,
                                    struct smota_start_resp *resp);
smota_err_t smota_handle_data_req(const struct smota_data_req *req,
                                   const uint8_t *data,
                                   struct smota_data_resp *resp);
smota_err_t smota_handle_finish_req(const struct smota_finish_req *req,
                                     struct smota_finish_resp *resp);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* SMOTA_PACKET_H */
