/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_packet.c
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 10:00:00
 * @Brief        : smOTA 包解析实现
 */

/*---------- includes ----------*/
#include "../../smota.h"
#include "smota_packet.h"

/*---------- macro ----------*/
/* CRC16 多项式: 0x1021 (CRC-16-CCITT) */
#define CRC16_POLY                      0x1021
#define CRC16_INIT_VAL                  0xFFFF

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/

/*---------- function ----------*/

/**
 * @brief  计算数据的CRC16校验值 (直接计算法)
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @return 16位CRC校验值
 * @note   多项式: 0x1021, 初始值: 0xFFFF (CRC-16-CCITT)
 */
uint16_t smota_crc16_compute(const uint8_t *data, uint16_t len)
{
    uint16_t crc = CRC16_INIT_VAL;
    uint16_t i;
    uint16_t j;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief  验证帧的CRC16校验
 * @param  frame: 完整帧数据指针 (包含CRC字段)
 * @param  len: 帧数据长度 (不包含CRC字段的长度)
 * @return 0=校验成功, <0=校验失败
 */
int smota_crc16_verify(const uint8_t *frame, uint16_t len)
{
    uint16_t frame_crc;
    uint16_t calc_crc;

    if (frame == NULL) {
        return -1;
    }

    /* 提取帧中的CRC值 (最后2字节, 小端) */
    frame_crc = (uint16_t)frame[len] | ((uint16_t)frame[len + 1] << 8);

    /* 计算帧数据的CRC值 */
    calc_crc = smota_crc16_compute(frame, len);

    /* 比较CRC值 */
    if (calc_crc == frame_crc) {
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief  解析接收到的帧
 * @param  data: 原始数据指针
 * @param  len: 数据长度
 * @param  frame: 输出解析后的帧结构
 * @return 0=成功, <0=失败
 * @note   解析成功后，frame->payload 指向 data 中的 payload 位置
 */
int smota_frame_parse(const uint8_t *data, uint16_t len, struct smota_frame *frame)
{
    struct smota_frame_header *header;
    uint16_t expected_min_len;
    uint16_t calc_crc;
    uint16_t frame_crc;

    /* 参数检查 */
    if (data == NULL || frame == NULL) {
        return -1;
    }

    expected_min_len = sizeof(struct smota_frame_header) + sizeof(uint16_t);
    if (len < expected_min_len) {
        return -2;
    }

    header = (struct smota_frame_header *)data;

    /* 验证 SOF */
    if (data[0] != 's' || data[1] != 'm' || data[2] != 'O' ||
        data[3] != 'T' || data[4] != 'A') {
        return -3;
    }

    /* 验证协议版本 */
    if (header->ver != SMOTA_PROTOCOL_VER) {
        return -4;
    }

    /* 验证 payload 长度 */
    if (header->length > (len - sizeof(struct smota_frame_header) - sizeof(uint16_t))) {
        return -5;
    }

    /* 验证 CRC */
    calc_crc = smota_crc16_compute(data, sizeof(struct smota_frame_header) + header->length);
    frame_crc = (uint16_t)data[sizeof(struct smota_frame_header) + header->length] |
                ((uint16_t)data[sizeof(struct smota_frame_header) + header->length + 1] << 8);
    if (calc_crc != frame_crc) {
        return -6;
    }

    /* 填充帧结构 */
    frame->header = *header;
    frame->payload = (uint8_t *)data + sizeof(struct smota_frame_header);
    frame->crc16 = frame_crc;

    return 0;
}

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
                      uint8_t *buffer, uint16_t buflen)
{
    struct smota_frame_header *header;
    uint16_t frame_len;
    uint16_t crc;

    /* 参数检查 */
    if (buffer == NULL) {
        return -1;
    }

    frame_len = sizeof(struct smota_frame_header) + payload_len + sizeof(uint16_t);
    if (buflen < frame_len) {
        return -2;
    }

    /* 填充帧头 */
    header = (struct smota_frame_header *)buffer;
    header->sof[0] = 's';
    header->sof[1] = 'm';
    header->sof[2] = 'O';
    header->sof[3] = 'T';
    header->sof[4] = 'A';
    header->ver = SMOTA_PROTOCOL_VER;
    header->frag = 0;
    header->seq = 0;  /* TODO: 实现序号管理 */
    header->cmd = cmd;
    header->length = payload_len;

    /* 填充 payload */
    if (payload != NULL && payload_len > 0) {
        uint16_t i;
        uint8_t *dest = buffer + sizeof(struct smota_frame_header);
        for (i = 0; i < payload_len; i++) {
            dest[i] = payload[i];
        }
    }

    /* 计算并填充 CRC */
    crc = smota_crc16_compute(buffer, sizeof(struct smota_frame_header) + payload_len);
    buffer[sizeof(struct smota_frame_header) + payload_len] = (uint8_t)(crc & 0xFF);
    buffer[sizeof(struct smota_frame_header) + payload_len + 1] = (uint8_t)(crc >> 8);

    return frame_len;
}

/**
 * @brief  获取命令码对应的响应命令码
 * @param  req_cmd: 请求命令码
 * @return 响应命令码
 */
uint8_t smota_cmd_to_response(uint8_t req_cmd)
{
    return (req_cmd | SMOTA_CMD_RESPONSE_FLAG);
}

/*---------- end of file ----------*/
