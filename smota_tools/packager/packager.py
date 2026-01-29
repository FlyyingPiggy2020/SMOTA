#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
smOTA 固件打包工具

功能:
    - 将 .bin/.hex 固件打包为 .ota 格式
    - 计算 SHA-256 哈希
    - 生成固件包头部
    - 可选：调用签名工具进行签名
"""

# __file_name: packager.py
# __author: lxf
# __date: 2026-01-29 09:39:06
# __last_edit_by: lxf_zjnb@qq.com
# __brief: smOTA 固件打包工具


import os
import sys
import struct
import hashlib
import argparse
from pathlib import Path


# 固件包头部结构 (256 Bytes)
class PackageHeader:
    """固件包头部"""

    MAGIC = 0xAA55AA55
    HEADER_SIZE = 256

    def __init__(self):
        self.magic = self.MAGIC
        self.version_major = 1
        self.version_minor = 0
        self.version_patch = 0
        self.firmware_size = 0
        self.firmware_crc = 0
        self.sha256_hash = bytes(32)
        self.signature_r = bytes(32)
        self.signature_s = bytes(32)
        self.flags = 0
        self.reserved = bytes(144)

    def pack(self):
        """打包为字节流"""
        return struct.pack(
            '<I3BI32s32s32sH144s',
            self.magic,
            self.version_major,
            self.version_minor,
            self.version_patch,
            self.firmware_size,
            self.firmware_crc,
            self.sha256_hash,
            self.signature_r,
            self.signature_s,
            self.flags,
            self.reserved
        )[:self.HEADER_SIZE]


def calculate_sha256(data: bytes) -> bytes:
    """计算 SHA-256 哈希"""
    return hashlib.sha256(data).digest()


def calculate_crc32(data: bytes) -> int:
    """计算 CRC-32 校验"""
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def create_package(input_file: str, output_file: str,
                   version: str = None, sign: bool = False):
    """
    创建固件包

    Args:
        input_file: 输入固件文件 (.bin/.hex)
        output_file: 输出固件包文件 (.ota)
        version: 固件版本 (格式: major.minor.patch)
        sign: 是否进行签名
    """
    # 读取固件数据
    with open(input_file, 'rb') as f:
        firmware_data = f.read()

    # 创建头部
    header = PackageHeader()
    header.firmware_size = len(firmware_data)

    # 解析版本号
    if version:
        parts = version.split('.')
        if len(parts) >= 1:
            header.version_major = int(parts[0])
        if len(parts) >= 2:
            header.version_minor = int(parts[1])
        if len(parts) >= 3:
            header.version_patch = int(parts[2])

    # 计算哈希和 CRC
    header.sha256_hash = calculate_sha256(firmware_data)
    header.firmware_crc = calculate_crc32(firmware_data)

    # 可选：签名
    if sign:
        # TODO: 调用签名工具
        pass

    # 写入输出文件
    with open(output_file, 'wb') as f:
        f.write(header.pack())
        f.write(firmware_data)

    print(f"固件包创建成功: {output_file}")
    print(f"  固件大小: {header.firmware_size} bytes")
    print(f"  固件版本: {header.version_major}.{header.version_minor}.{header.version_patch}")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='smOTA 固件打包工具')
    parser.add_argument('input', help='输入固件文件 (.bin/.hex)')
    parser.add_argument('-o', '--output', help='输出固件包文件 (.ota)')
    parser.add_argument('-v', '--version', help='固件版本 (major.minor.patch)')
    parser.add_argument('-s', '--sign', action='store_true', help='进行签名')

    args = parser.parse_args()

    # 默认输出文件名
    if not args.output:
        input_path = Path(args.input)
        args.output = input_path.stem + '.ota'

    create_package(args.input, args.output, args.version, args.sign)


if __name__ == '__main__':
    main()
