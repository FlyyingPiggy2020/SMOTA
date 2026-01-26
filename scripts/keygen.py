#!/usr/bin/env python3
"""
smOTA 密钥生成工具

用于生成 ECDSA-P256 密钥对和 AES-128 主密钥

使用方法:
    python keygen.py                    # 生成所有密钥
    python keygen.py --ecdsa            # 仅生成 ECDSA 密钥对
    python keygen.py --aes              # 仅生成 AES 密钥
    python keygen.py --output keys/     # 指定输出目录
"""

import os
import sys
import argparse
from pathlib import Path

try:
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("警告: cryptography 库未安装，ECDSA 密钥生成功能不可用")
    print("请运行: pip install cryptography")


# 密钥 ID 定义
KEY_ID_AES_MASTER = 0
KEY_ID_ECDSA_PUB = 1


def generate_ecdsa_keypair():
    """生成 ECDSA-P256 密钥对"""
    if not CRYPTO_AVAILABLE:
        return None, None, None

    # 生成私钥
    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())

    # 获取公钥
    public_key = private_key.public_key()

    # 序列化私钥 (PEM 格式)
    private_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption()
    )

    # 获取公钥原始数据 (未压缩格式, 64 字节)
    public_key_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.X962,
        format=serialization.PublicFormat.UncompressedPoint
    )

    return private_pem, public_key_bytes, public_key


def generate_aes_key():
    """生成随机 AES-128 密钥"""
    import secrets
    return secrets.token_bytes(16)


def format_byte_array(bytes_data, var_name, size=8):
    """格式化字节数组为 C 代码，每行 8 个字节"""
    hex_values = [f"0x{b:02X}" for b in bytes_data]

    lines = []
    for i in range(0, len(hex_values), size):
        line_bytes = hex_values[i:i + size]
        line = "    " + ", ".join(line_bytes)
        if i + size < len(hex_values):
            line += ","
        lines.append(line)

    return "\n".join(lines)


def ecdsa_public_key_to_c_array(public_key_bytes, var_name="smota_ecdsa_pub_key"):
    """将 ECDSA 公钥转换为 C 数组格式"""
    # X962 编码会在公钥前加 0x04 前缀，需要跳过
    if public_key_bytes[0] == 0x04:
        public_key_bytes = public_key_bytes[1:]

    if len(public_key_bytes) != 64:
        raise ValueError(f"公钥长度应为 64 字节，实际为 {len(public_key_bytes)} 字节")

    x_coord = public_key_bytes[:32]
    y_coord = public_key_bytes[32:]

    x_lines = format_byte_array(x_coord, f"{var_name}_x", 8)
    y_lines = format_byte_array(y_coord, f"{var_name}_y", 8)
    full_lines = format_byte_array(public_key_bytes, var_name, 8)

    c_code = f"""// ECDSA-P256 公钥 (未压缩格式)
// X 坐标 (32 bytes)
static const uint8_t {var_name}_x[32] = {{
{x_lines}
}};

// Y 坐标 (32 bytes)
static const uint8_t {var_name}_y[32] = {{
{y_lines}
}};

// 完整公钥 (64 bytes)
static const uint8_t {var_name}[64] = {{
{full_lines}
}};
"""
    return c_code


def aes_key_to_c_array(key_bytes, var_name="smota_aes_master_key"):
    """将 AES 密钥转换为 C 数组格式"""
    if len(key_bytes) != 16:
        raise ValueError(f"AES-128 密钥长度应为 16 字节，实际为 {len(key_bytes)} 字节")

    lines = format_byte_array(key_bytes, var_name, 16)

    c_code = f"""// AES-128 主密钥 (16 字节)
static const uint8_t {var_name}[16] = {{
{lines}
}};
"""
    return c_code


def generate_smota_keys_c(aes_key=None, ecdsa_pub_key=None, output_path=None):
    """生成完整的 smota_keys.c 文件"""

    if aes_key is None and ecdsa_pub_key is None:
        raise ValueError("至少需要提供一种密钥")

    lines = []
    lines.append("/**")
    lines.append(" * @file    smota_keys.c")
    lines.append(" * @brief   smOTA 密钥定义")
    lines.append(" * @note   此文件由 scripts/keygen.py 自动生成")
    lines.append(" *          请妥善保管，不要提交到版本控制系统")
    lines.append(" */")
    lines.append("")
    lines.append('#include <stdint.h>')
    lines.append('#include <string.h>')
    lines.append("")

    # 密钥 ID 定义
    lines.append("// 密钥 ID 定义")
    lines.append(f"#define SMOTA_KEY_AES_MASTER   {KEY_ID_AES_MASTER}")
    lines.append(f"#define SMOTA_KEY_ECDSA_PUB    {KEY_ID_ECDSA_PUB}")
    lines.append("")

    # 密钥变量定义
    if ecdsa_pub_key is not None:
        x_lines = format_byte_array(ecdsa_pub_key[:32], "g_ecdsa_pub_key_x", 8)
        y_lines = format_byte_array(ecdsa_pub_key[32:], "g_ecdsa_pub_key_y", 8)

        lines.append("// ECDSA-P256 公钥 (未压缩格式)")
        lines.append("static const uint8_t g_ecdsa_pub_key_x[32] = {")
        lines.append(x_lines)
        lines.append("};")
        lines.append("")
        lines.append("static const uint8_t g_ecdsa_pub_key_y[32] = {")
        lines.append(y_lines)
        lines.append("};")
        lines.append("")

    if aes_key is not None:
        key_hex = ", ".join(f"0x{b:02X}" for b in aes_key)
        lines.append("// AES-128 主密钥")
        lines.append("static const uint8_t g_aes_master_key[16] = {")
        lines.append(f"    {key_hex}")
        lines.append("};")
        lines.append("")

    # smota_get_key 函数实现
    lines.append("/**")
    lines.append(" * @brief 获取密钥数据")
    lines.append(" * @param key_id  密钥 ID")
    lines.append(" * @param out_key 输出缓冲区")
    lines.append(" * @param len     缓冲区长度")
    lines.append(" */")
    lines.append("void smota_get_key(uint8_t key_id, uint8_t *out_key, uint32_t len) {")
    lines.append("    switch (key_id) {")

    if aes_key is not None:
        lines.append(f"        case SMOTA_KEY_AES_MASTER:")
        lines.append("            if (len >= 16) {")
        lines.append("                memcpy(out_key, g_aes_master_key, 16);")
        lines.append("            }")
        lines.append("            break;")

    if ecdsa_pub_key is not None:
        lines.append(f"        case SMOTA_KEY_ECDSA_PUB:")
        lines.append("            if (len >= 64) {")
        lines.append("                // 拼接 X 和 Y 坐标")
        lines.append("                memcpy(out_key, g_ecdsa_pub_key_x, 32);")
        lines.append("                memcpy(out_key + 32, g_ecdsa_pub_key_y, 32);")
        lines.append("            }")
        lines.append("            break;")

    lines.append("        default:")
    lines.append("            break;")
    lines.append("    }")
    lines.append("}")

    content = "\n".join(lines)

    if output_path:
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(content, encoding="utf-8")
        print(f"已生成: {output_path}")

    return content


def main():
    parser = argparse.ArgumentParser(description="smOTA 密钥生成工具")
    parser.add_argument("--ecdsa", action="store_true", help="生成 ECDSA-P256 密钥对")
    parser.add_argument("--aes", action="store_true", help="生成 AES-128 密钥")
    parser.add_argument("--output", "-o", default="keys", help="输出目录 (默认: keys/)")
    parser.add_argument("--c-file", action="store_true", help="生成 smota_keys.c 文件")

    args = parser.parse_args()

    # 默认生成所有密钥
    gen_ecdsa = args.ecdsa or not (args.ecdsa or args.aes)
    gen_aes = args.aes or not (args.ecdsa or args.aes)

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("smOTA 密钥生成工具")
    print("=" * 60)

    aes_key = None
    ecdsa_private_pem = None
    ecdsa_pub_key = None

    # 生成 ECDSA 密钥对
    if gen_ecdsa:
        print("\n[1/2] 生成 ECDSA-P256 密钥对...")
        if CRYPTO_AVAILABLE:
            ecdsa_private_pem, ecdsa_pub_key, public_key_obj = generate_ecdsa_keypair()

            # 保存私钥 (PEM 格式)
            private_key_path = output_dir / "ecdsa_private_key.pem"
            private_key_path.write_bytes(ecdsa_private_pem)
            print(f"  私钥已保存: {private_key_path}")
            print(f"  警告: 请妥善保管私钥，不要泄露！")

            # 保存公钥 (二进制)
            public_key_path = output_dir / "ecdsa_public_key.bin"
            public_key_path.write_bytes(ecdsa_pub_key)
            print(f"  公钥已保存: {public_key_path}")

            # 保存公钥 (C 数组)
            ecdsa_c_path = output_dir / "ecdsa_public_key.c"
            ecdsa_c_path.write_text(ecdsa_public_key_to_c_array(ecdsa_pub_key), encoding="utf-8")
            print(f"  公钥 C 数组: {ecdsa_c_path}")
        else:
            print("  跳过: cryptography 库未安装")

    # 生成 AES 密钥
    if gen_aes:
        print("\n[2/2] 生成 AES-128 主密钥...")
        aes_key = generate_aes_key()

        # 保存密钥 (二进制)
        aes_key_path = output_dir / "aes_master_key.bin"
        aes_key_path.write_bytes(aes_key)
        print(f"  密钥已保存: {aes_key_path}")

        # 保存密钥 (C 数组)
        aes_c_path = output_dir / "aes_master_key.c"
        aes_c_path.write_text(aes_key_to_c_array(aes_key), encoding="utf-8")
        print(f"  密钥 C 数组: {aes_c_path}")

    # 生成 smota_keys.c
    if args.c_file:
        print("\n生成 smota_keys.c...")
        generate_smota_keys_c(
            aes_key=aes_key if gen_aes else None,
            ecdsa_pub_key=ecdsa_pub_key if gen_ecdsa else None,
            output_path=output_dir / "smota_keys.c"
        )

    print("\n" + "=" * 60)
    print("密钥生成完成！")
    print("=" * 60)
    print("\n安全提醒:")
    print("  1. 请将私钥文件妥善保管，不要提交到 Git 仓库")
    print("  2. 建议在 .gitignore 中添加:")
    print("     *.pem")
    print("     *_key.bin")
    print("     smota_keys.c")
    print("  3. 在生产环境中，建议启用 MCU 的读保护 (RDP)")


if __name__ == "__main__":
    main()
