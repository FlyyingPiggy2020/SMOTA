/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_packet.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 12:32:56
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
/* smFrame 帏起始符 */
#define SMOTA_SOF                      "smOTA"
#define SMOTA_SOF_SIZE                 5

/* 协议版本 */
#define SMOTA_PROTOCOL_VER             0x00

/* 命令码定义 */
#define SMOTA_CMD_HANDSHAKE            0x01 /* 握手请求 */
#define SMOTA_CMD_HEADER_INFO          0x02 /* 发送固件头部信息 */
#define SMOTA_CMD_DATA_BLOCK           0x03 /* 发送数据包 */
#define SMOTA_CMD_DATA_COMPLETE        0x04 /* 数据包传输完毕 */
#define SMOTA_CMD_INSTALL              0x05 /* 触发安装请求 */
#define SMOTA_CMD_ACTIVATE_CHECK       0x06 /* 状态确认请求 */

/* 应答标志位 (D7置位) */
#define SMOTA_CMD_RESPONSE_FLAG        0x80

/* 应答命令码 */
#define SMOTA_CMD_HANDSHAKE_RESP       (SMOTA_CMD_HANDSHAKE | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_HEADER_INFO_RESP     (SMOTA_CMD_HEADER_INFO | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_DATA_BLOCK_RESP      (SMOTA_CMD_DATA_BLOCK | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_DATA_COMPLETE_RESP   (SMOTA_CMD_DATA_COMPLETE | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_INSTALL_RESP         (SMOTA_CMD_INSTALL | SMOTA_CMD_RESPONSE_FLAG)
#define SMOTA_CMD_ACTIVATE_CHECK_RESP  (SMOTA_CMD_ACTIVATE_CHECK | SMOTA_CMD_RESPONSE_FLAG)

/* 通用错误码定义 (uint32_t bit位) */
#define SMOTA_ERR_PROTOCOL_MISMATCH    (1U << 0)  /* bit0: 协议版本不匹配 */
#define SMOTA_ERR_PROJECT_ID_MISMATCH  (1U << 1)  /* bit1: 项目ID不匹配 */
#define SMOTA_ERR_VERSION_MISMATCH     (1U << 2)  /* bit2: 版本不匹配 (防回滚) */
#define SMOTA_ERR_FLASH_INSUFFICIENT   (1U << 3)  /* bit3: Flash空间不足 */
#define SMOTA_ERR_DATA_AES             (1U << 8)  /* bit8: AES解密错误 */
#define SMOTA_ERR_FLASH_WRITE          (1U << 9)  /* bit9: FLASH写入错误 */
#define SMOTA_ERR_VERIFY_SHA256_FAILED (1U << 17) /* bit17: SHA256校验不匹配 */
#define SMOTA_ERR_VERIFY_SIGN_FAILED   (1U << 18) /* bit18: ECDSA签名验证未通过 */
#define SMOTA_ERR_INSTALL_FLASH_READ   (1U << 19) /* bit19: 从下载区读取数据失败 */
#define SMOTA_ERR_INSTALL_LOW_BATTERY  (1U << 20) /* bit20: 电池电量过低 */
#define SMOTA_ERR_INSTALL_BUSY         (1U << 21) /* bit21: 设备处于关键业务状态 */
#define SMOTA_ERR_INSTALL_VERSION_OLD  (1U << 22) /* bit22: 安装后版本号未更新 */

/* 设备能力标志位 */
#define SMOTA_CAP_SIGNATURE            (1U << 0) /* bit0: 支持ECDSA签名验证 */
#define SMOTA_CAP_ENCRYPT              (1U << 1) /* bit1: 支持AES解密 */
#define SMOTA_CAP_ANTI_ROLLBACK        (1U << 2) /* bit2: 支持防回滚 */

/* 分片控制字段定义 */
#define SMOTA_FRAG_EN_MASK             0x80 /* bit7: 分片使能标志 */
#define SMOTA_FRAG_MORE_MASK           0x40 /* bit6: 后续分片标志 */
#define SMOTA_FRAG_TOTAL_MASK          0x3F /* bit5-0: 分片总数 */

/*---------- type define ----------*/

#pragma pack(push, 1)

/**
 * @brief  smFrame 通用帧头
 * @note   所有阶段使用统一的帧格式
 */
struct smota_frame_header {
    uint8_t sof[5];  /* 帧起始符，固定为 "smOTA" */
    uint8_t ver;     /* 协议版本，当前为 0x00 */
    uint8_t frag;    /* 分片控制字段 (暂不支持) */
    uint16_t seq;    /* 帧序号 0-65535，循环使用 */
    uint8_t cmd;     /* 命令码 */
    uint16_t length; /* Payload 长度 (小端) */
};

/**
 * @brief  smFrame 通用帧结构
 */
struct smota_frame {
    struct smota_frame_header header;
    uint8_t *payload; /* 实际数据 */
    uint16_t crc16;   /* CRC-16 校验 */
};

/**
 * @brief  握手请求 (Server -> Device, 0x01)
 */
struct smota_handshake_req {
    uint8_t fw_version_major; /* 固件主版本号 */
    uint8_t fw_version_minor; /* 固件次版本号 */
    uint8_t fw_version_patch; /* 固件补丁版本号 */
    uint32_t firmware_size;   /* 固件大小 */
    uint8_t project_id[16];   /* 项目ID */
    uint16_t block_timeout;   /* 数据超时建议值(ms) */
    uint16_t check_timeout;   /* 校验超时建议值(ms) */
    uint16_t install_timeout; /* 安装超时建议值(ms) */
    uint32_t total_timeout;   /* 总超时建议值(ms) */
};

/**
 * @brief  握手响应 (Device -> Server, 0x81)
 */
struct smota_handshake_resp {
    uint32_t error_code;      /* 通用应答错误码 */
    uint32_t next_offset;     /* 用于断点续传 */
    uint16_t max_packet_size; /* 设备实际支持的最大包长度 */
    uint16_t mtu_size;        /* 设备物理支持最大MTU长度 */
    uint32_t flash_free_size; /* 可用Flash空间 */
    uint16_t block_timeout;   /* 数据超时确认值(ms) */
    uint16_t install_timeout; /* 安装超时确认值(ms) */
    uint8_t capabilities;     /* 设备能力标志位 */
};

/**
 * @brief  固件头部信息请求 (Server -> Device, 0x02)
 */
struct smota_header_info_req {
    uint8_t sha256_hash[32]; /* 固件SHA-256摘要 */
    uint8_t signature_r[32]; /* ECDSA签名r分量 */
    uint8_t signature_s[32]; /* ECDSA签名s分量 */
};

/**
 * @brief  固件头部信息应答 (Device -> Server, 0x82)
 */
struct smota_header_info_resp {
    uint32_t error_code; /* 通用应答错误码 */
};

/**
 * @brief  数据块传输请求 (Server -> Device, 0x03)
 */
struct smota_data_block_req {
    uint32_t offset; /* 在固件中的字节偏移 */
    uint16_t length; /* 数据长度 */
    uint8_t data[0]; /* 可变长度数据 */
};

/**
 * @brief  数据块传输响应 (Device -> Server, 0x83)
 */
struct smota_data_block_resp {
    uint32_t error_code;      /* 写入结果 */
    uint32_t received_offset; /* 设备确认已成功写入的偏移量 */
};

/**
 * @brief  传输完成请求 (Server -> Device, 0x04)
 */
struct smota_transfer_complete_req {
    uint32_t total_size; /* 再次确认总大小，防止漏包 */
};

/**
 * @brief  传输完成应答 (Device -> Server, 0x84)
 */
struct smota_transfer_complete_resp {
    uint32_t error_code; /* 0=成功, 非0=校验错误 */
};

/**
 * @brief  触发安装请求 (Server -> Device, 0x05)
 */
struct smota_install_req {
    uint8_t force_install; /* 强制安装标志: 0-正常安装, 1-强制执行 */
    uint16_t reserved;     /* 预留 */
};

/**
 * @brief  触发安装应答 (Device -> Server, 0x85)
 */
struct smota_install_resp {
    uint32_t error_code;       /* 0=成功, bit20=电量过低, bit21=关键业务中 */
    uint16_t estimated_time_s; /* 设备预估安装并重启所需秒数 */
};

/**
 * @brief  状态确认请求 (Server -> Device, 0x06)
 */
struct smota_activate_check_req {
    uint32_t task_id; /* 升级任务ID (可选) */
};

/**
 * @brief  状态确认应答 (Device -> Server, 0x86)
 */
struct smota_activate_check_resp {
    uint32_t error_code;      /* 0=升级成功并已运行新版本 */
    uint8_t fw_version_major; /* 当前运行的主版本号 */
    uint8_t fw_version_minor; /* 当前运行的次版本号 */
    uint8_t fw_version_patch; /* 当前运行的补丁版本号 */
};

#pragma pack(pop)

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
/**
 * @brief  计算数据的CRC16校验值
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @return 16位CRC校验值
 */
uint16_t smota_crc16_compute(const uint8_t *data, uint16_t len);

/**
 * @brief  验证smFrame的CRC16校验
 * @param  frame: 完整帧数据指针
 * @param  len: 帧数据长度
 * @return 0=校验成功, <0=校验失败
 */
int smota_crc16_verify(const uint8_t *frame, uint16_t len);

/**
 * @brief  解析接收到的帧
 * @param  data: 原始数据指针
 * @param  len: 数据长度
 * @param  frame: 输出解析后的帧结构
 * @return 0=成功, <0=失败
 * @note   解析成功后，frame->payload 指向 data 中的 payload 位置
 */
int smota_frame_parse(const uint8_t *data, uint16_t len, struct smota_frame *frame);

/**
 * @brief  构建待发送的帧
 * @param  cmd: 命令码
 * @param  payload: payload 数据指针
 * @param  payload_len: payload 长度
 * @param  buffer: 输出缓冲区
 * @param  buflen: 缓冲区大小
 * @return 构建的帧长度, <0=失败
 */
int smota_frame_build(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                      uint8_t *buffer, uint16_t buflen);

/**
 * @brief  获取命令码对应的响应命令码
 * @param  req_cmd: 请求命令码
 * @return 响应命令码
 */
uint8_t smota_cmd_to_response(uint8_t req_cmd);

/**
 * @brief  处理握手请求 (0x01)
 * @param[in]   req: 握手请求结构体
 * @param[out]  resp: 握手响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_handshake_req(const struct smota_handshake_req *req,
                                        struct smota_handshake_resp *resp);

/**
 * @brief  处理头部信息请求 (0x02)
 * @param[in]   req: 头部信息请求结构体
 * @param[out]  resp: 头部信息响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_header_info_req(const struct smota_header_info_req *req,
                                          struct smota_header_info_resp *resp);

/**
 * @brief  处理数据块请求 (0x03)
 * @param[in]   req: 数据块请求结构体
 * @param[in]   data: 数据指针
 * @param[out]  resp: 数据块响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_data_block_req(const struct smota_data_block_req *req,
                                         const uint8_t *data,
                                         struct smota_data_block_resp *resp);

/**
 * @brief  处理传输完成请求 (0x04)
 * @param[in]   req: 传输完成请求结构体
 * @param[out]  resp: 传输完成响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_transfer_complete_req(const struct smota_transfer_complete_req *req,
                                                struct smota_transfer_complete_resp *resp);

/**
 * @brief  处理安装请求 (0x05)
 * @param[in]   req: 安装请求结构体
 * @param[out]  resp: 安装响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_install_req(const struct smota_install_req *req,
                                      struct smota_install_resp *resp);

/**
 * @brief  处理激活检查请求 (0x06)
 * @param[in]   req: 激活检查请求结构体
 * @param[out]  resp: 激活检查响应结构体
 * @return      smota_err_t 错误码
 */
smota_err_t smota_handle_activate_check_req(const struct smota_activate_check_req *req,
                                             struct smota_activate_check_resp *resp);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_PACKET_H
