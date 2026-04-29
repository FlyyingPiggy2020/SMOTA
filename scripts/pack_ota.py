#!/usr/bin/env python
from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import struct
import zipfile
from pathlib import Path

from elftools.elf.elffile import ELFFile


SMOTA_APP_VERSION_MAGIC = 0x41505056
SMOTA_APP_VERSION_SECTION = ".smota_app_version"
SMOTA_APP_VERSION_SYMBOL = "g_smota_app_version_info"
SMOTA_APP_VERSION_STRUCT = struct.Struct("<IBBB16s")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Pack firmware and version manifest into a .ota container.")
    parser.add_argument("--elf", required=True, help="Path to the input ELF/AXF file.")
    parser.add_argument("--firmware", required=True, help="Path to the input .bin or .hex firmware file.")
    parser.add_argument("--output", required=True, help="Path to the output .ota file.")
    return parser.parse_args()


def load_app_info_from_elf(path: Path) -> tuple[tuple[int, int, int], str]:
    with path.open("rb") as fp:
        elf = ELFFile(fp)
        section = elf.get_section_by_name(SMOTA_APP_VERSION_SECTION)
        if section is not None:
            data = section.data()
        else:
            data = load_version_data_from_symbol(elf)

    if len(data) < SMOTA_APP_VERSION_STRUCT.size:
        raise ValueError(
            f"version section too small: {len(data)} bytes, need {SMOTA_APP_VERSION_STRUCT.size}"
        )

    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != SMOTA_APP_VERSION_MAGIC:
        raise ValueError(f"invalid version magic: 0x{magic:08X}")

    _, major, minor, patch, project_id_raw = SMOTA_APP_VERSION_STRUCT.unpack(
        data[:SMOTA_APP_VERSION_STRUCT.size]
    )
    project_id = project_id_raw.split(b"\x00", 1)[0].decode("utf-8")
    if not project_id:
        raise ValueError("project_id is empty in app version section")

    return (major, minor, patch), project_id


def load_version_data_from_symbol(elf: ELFFile) -> bytes:
    symtab = elf.get_section_by_name(".symtab")
    if symtab is None:
        raise ValueError(f"missing ELF section: {SMOTA_APP_VERSION_SECTION}")

    for symbol in symtab.iter_symbols():
        if symbol.name != SMOTA_APP_VERSION_SYMBOL:
            continue

        address = int(symbol["st_value"])
        size = int(symbol["st_size"])
        if size < SMOTA_APP_VERSION_STRUCT.size:
            raise ValueError(f"version symbol too small: {size} bytes")

        for segment in elf.iter_segments():
            if segment["p_type"] != "PT_LOAD":
                continue

            start = int(segment["p_vaddr"])
            end = start + int(segment["p_filesz"])
            if address < start or (address + size) > end:
                continue

            offset = address - start
            data = segment.data()[offset:offset + size]
            if len(data) < size:
                raise ValueError("failed to read full version symbol data from ELF segment")
            return data

        raise ValueError("version symbol exists, but no load segment contains it")

    raise ValueError(
        f"missing ELF section {SMOTA_APP_VERSION_SECTION} and symbol {SMOTA_APP_VERSION_SYMBOL}"
    )


def load_firmware(path: Path) -> bytes:
    suffix = path.suffix.lower()
    if suffix == ".bin":
        return path.read_bytes()
    if suffix == ".hex":
        return load_hex_firmware(path)
    raise ValueError(f"unsupported firmware format: {suffix}, only .bin/.hex are allowed")


def load_hex_firmware(path: Path) -> bytes:
    image: dict[int, int] = {}
    upper_addr = 0
    start_addr: int | None = None
    end_addr = 0

    for line_no, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue

        if not line.startswith(":"):
            raise ValueError(f"invalid Intel HEX at line {line_no}: missing ':'")

        try:
            record = binascii.unhexlify(line[1:])
        except (binascii.Error, ValueError) as exc:
            raise ValueError(f"invalid Intel HEX at line {line_no}: {exc}") from exc

        if len(record) < 5:
            raise ValueError(f"invalid Intel HEX at line {line_no}: record too short")

        length = record[0]
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        data = record[4:-1]
        checksum = record[-1]

        if length != len(data):
            raise ValueError(f"invalid Intel HEX at line {line_no}: byte count mismatch")

        if ((sum(record[:-1]) + checksum) & 0xFF) != 0:
            raise ValueError(f"invalid Intel HEX at line {line_no}: checksum mismatch")

        if record_type == 0x00:
            absolute_addr = upper_addr + address
            if start_addr is None or absolute_addr < start_addr:
                start_addr = absolute_addr
            for offset, value in enumerate(data):
                image[absolute_addr + offset] = value
            end_addr = max(end_addr, absolute_addr + len(data))
        elif record_type == 0x01:
            break
        elif record_type == 0x04:
            if length != 2:
                raise ValueError(f"invalid Intel HEX at line {line_no}: bad extended linear address")
            upper_addr = (((data[0] << 8) | data[1]) << 16)
        elif record_type in (0x02, 0x03, 0x05):
            continue
        else:
            raise ValueError(f"invalid Intel HEX at line {line_no}: unsupported record type {record_type}")

    if start_addr is None:
        return b""

    firmware = bytearray([0xFF] * (end_addr - start_addr))
    for absolute_addr, value in image.items():
        firmware[absolute_addr - start_addr] = value

    return bytes(firmware)


def build_manifest(
    firmware_path: Path,
    firmware: bytes,
    version: tuple[int, int, int],
    project_id: str,
) -> dict[str, object]:
    if len(project_id.encode("utf-8")) > 16:
        raise ValueError(f"project_id is too long: {project_id}")

    return {
        "format_version": 1,
        "firmware_name": firmware_path.name,
        "firmware_format": firmware_path.suffix.lower().lstrip("."),
        "firmware_size": len(firmware),
        "firmware_sha256": hashlib.sha256(firmware).hexdigest(),
        "project_id": project_id,
        "fw_version": {
            "major": version[0],
            "minor": version[1],
            "patch": version[2],
        },
    }


def write_ota(output_path: Path, firmware_path: Path, manifest: dict[str, object]) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
        zf.write(firmware_path, arcname=firmware_path.name)


def main() -> int:
    args = parse_args()
    elf_path = Path(args.elf).expanduser().resolve()
    firmware_path = Path(args.firmware).expanduser().resolve()
    output_path = Path(args.output).expanduser().resolve()

    version, project_id = load_app_info_from_elf(elf_path)
    firmware = load_firmware(firmware_path)
    manifest = build_manifest(firmware_path, firmware, version, project_id)
    write_ota(output_path, firmware_path, manifest)

    print(f"packed {output_path}")
    print(f"version={version[0]}.{version[1]}.{version[2]}")
    print(f"project_id={project_id}")
    print(f"firmware={firmware_path.name} size={len(firmware)} sha256={manifest['firmware_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
