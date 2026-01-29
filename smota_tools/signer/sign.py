#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
smOTA 固件签名工具

功能:
    - 使用 ECDSA-P256 对固件进行签名
    - 从私钥文件读取私钥
    - 输出签名数据 (r, s 各 32 字节)
"""

# __file_name: sign.py
# __author: lxf
# __date: 2026-01-29 09:39:06
# __last_edit_by: lxf_zjnb@qq.com
# __brief: smOTA 固件签名工具


import os
import sys
import hashlib
import argparse
from pathlib import Path

try:
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("错误: 未安装 cryptography 库")
    print("请运行: pip install cryptography")
    sys.exit(1)


def load_private_key(key_file: str):
    """
    加载 ECDSA 私钥 (PEM 格式)

    Args:
        key_file: 私钥文件路径

    Returns:
        私钥对象
    """
    with open(key_file, 'rb') as f:
        private_key = f.read()

    # 从 PEM 格式加载私钥
    from cryptography.hazmat.primitives import serialization
    return serialization.load_pem_private_key(
        private_key,
        password=None,
        backend=default_backend()
    )


def sign_firmware(firmware_file: str, private_key):
    """
    对固件进行签名

    Args:
        firmware_file: 固件文件路径
        private_key: ECDSA 私钥对象

    Returns:
        (signature_r, signature_s): 各 32 字节
    """
    # 读取固件数据
    with open(firmware_file, 'rb') as f:
        firmware_data = f.read()

    # 计算 SHA-256 哈希
    firmware_hash = hashlib.sha256(firmware_data).digest()

    # 使用 ECDSA-P256 进行签名
    signature = private_key.sign(
        firmware_hash,
        ec.ECDSA(hashes.SHA256())
    )

    # 解析签名 (DER 编码)
    from cryptography.hazmat.primitives.asymmetric import utils
    r, s = utils.decode_dss_signature(signature)

    # 转换为 32 字节
    signature_r = r.to_bytes(32, byteorder='big')
    signature_s = s.to_bytes(32, byteorder='big')

    return signature_r, signature_s


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='smOTA 固件签名工具')
    parser.add_argument('firmware', help='固件文件路径')
    parser.add_argument('-k', '--key', default='ecdsa_private_key.pem',
                       help='私钥文件路径 (默认: ecdsa_private_key.pem)')
    parser.add_argument('-o', '--output', help='输出签名文件路径')

    args = parser.parse_args()

    # 检查私钥文件
    if not os.path.exists(args.key):
        print(f"错误: 私钥文件不存在: {args.key}")
        sys.exit(1)

    # 加载私钥
    print(f"加载私钥: {args.key}")
    private_key = load_private_key(args.key)

    # 对固件签名
    print(f"签名固件: {args.firmware}")
    signature_r, signature_s = sign_firmware(args.firmware, private_key)

    # 输出签名
    if args.output:
        with open(args.output, 'wb') as f:
            f.write(signature_r + signature_s)
        print(f"签名已保存到: {args.output}")
    else:
        print("签名 (r):", signature_r.hex())
        print("签名 (s):", signature_s.hex())


if __name__ == '__main__':
    main()
