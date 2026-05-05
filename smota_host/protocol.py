from __future__ import annotations

import binascii
import hashlib
import json
import socket
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover - optional dependency
    serial = None


LogFn = Callable[[str, str], None]

SOF = b"smOTA"
FRAME_HEADER = struct.Struct("<5sBH")
CRC16 = struct.Struct("<H")
NO_RESPONSE_RETRY_COUNT = 3

CMD_QUERY = 0x01
CMD_START = 0x02
CMD_DATA = 0x03
CMD_FINISH = 0x04

CMD_QUERY_RESP = 0x81
CMD_START_RESP = 0x82
CMD_DATA_RESP = 0x83
CMD_FINISH_RESP = 0x84

QUERY_REQ = struct.Struct("<BBBB16s")
QUERY_RESP = struct.Struct("<IBBBB16s")
START_REQ = struct.Struct("<BI32sH")
START_RESP = struct.Struct("<IH")
DATA_REQ = struct.Struct("<IH")
DATA_RESP = struct.Struct("<II")
FINISH_REQ = struct.Struct("<I")
FINISH_RESP = struct.Struct("<IH")

QUERY_FLAG_FORCE_UPGRADE = 0x01
START_FLAG_SHA256_VALID = 0x01

CMD_NAMES = {
    CMD_QUERY: "QUERY",
    CMD_START: "START",
    CMD_DATA: "DATA",
    CMD_FINISH: "FINISH",
    CMD_QUERY_RESP: "QUERY_RESP",
    CMD_START_RESP: "START_RESP",
    CMD_DATA_RESP: "DATA_RESP",
    CMD_FINISH_RESP: "FINISH_RESP",
}


@dataclass(frozen=True)
class Frame:
    cmd: int
    payload: bytes


@dataclass(frozen=True)
class StartResponse:
    error_code: int
    max_payload_size: int


@dataclass(frozen=True)
class QueryVersionResponse:
    error_code: int
    allow_upgrade: bool
    version: tuple[int, int, int]
    project_id: str


@dataclass(frozen=True)
class OtaManifest:
    format_version: int
    firmware_name: str
    firmware_format: str
    firmware_size: int
    firmware_sha256: str
    project_id: str
    version: tuple[int, int, int]


@dataclass(frozen=True)
class OtaPackage:
    path: Path
    manifest: OtaManifest
    firmware: bytes


@dataclass(frozen=True)
class SerialPortInfo:
    device: str
    label: str


def parse_version(text: str) -> tuple[int, int, int]:
    parts = text.strip().split(".")
    if len(parts) != 3:
        raise ValueError("version must be major.minor.patch")

    version = tuple(int(part) for part in parts)
    if any(part < 0 or part > 255 for part in version):
        raise ValueError("each version field must be between 0 and 255")
    return version


def format_version(version: tuple[int, int, int]) -> str:
    return f"{version[0]}.{version[1]}.{version[2]}"


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def payload_preview(data: bytes, limit: int = 16) -> str:
    preview = data[:limit].hex(" ").upper()
    if len(data) > limit:
        return f"{preview} ..."
    return preview


def frame_hex(data: bytes) -> str:
    return data.hex(" ").upper()


def _load_bin_firmware(path: Path) -> bytes:
    return path.read_bytes()


def _load_hex_firmware(path: Path) -> bytes:
    return _load_hex_firmware_from_text(path.read_text(encoding="utf-8"))


def read_firmware(path: str | Path) -> bytes:
    resolved = Path(path).expanduser().resolve()
    suffix = resolved.suffix.lower()

    if suffix == ".bin":
        return _load_bin_firmware(resolved)
    if suffix == ".hex":
        return _load_hex_firmware(resolved)

    raise ValueError(f"unsupported firmware format: {resolved.suffix}, only .bin/.hex are allowed")


def _parse_ota_manifest(data: bytes) -> OtaManifest:
    try:
        raw = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"manifest.json 解析失败: {exc}") from exc

    required_fields = (
        "format_version",
        "firmware_name",
        "firmware_format",
        "firmware_size",
        "firmware_sha256",
        "project_id",
        "fw_version",
    )
    for field in required_fields:
        if field not in raw:
            raise ValueError(f"manifest.json 缺少字段: {field}")

    version_raw = raw["fw_version"]
    if not isinstance(version_raw, dict):
        raise ValueError("manifest.json 中 fw_version 必须是对象")

    version = (
        int(version_raw["major"]),
        int(version_raw["minor"]),
        int(version_raw["patch"]),
    )
    if any(part < 0 or part > 255 for part in version):
        raise ValueError("manifest.json 中的版本字段必须在 0-255 之间")

    firmware_name = str(raw["firmware_name"])
    firmware_format = str(raw["firmware_format"]).lower()
    project_id = str(raw["project_id"])
    if firmware_format not in ("bin", "hex"):
        raise ValueError(f"manifest.json 中的 firmware_format 不支持: {firmware_format}")
    if not project_id:
        raise ValueError("manifest.json 中的 project_id 不能为空")
    if len(project_id.encode("utf-8")) > 16:
        raise ValueError("manifest.json 中的 project_id 超过 16 字节")

    return OtaManifest(
        format_version=int(raw["format_version"]),
        firmware_name=firmware_name,
        firmware_format=firmware_format,
        firmware_size=int(raw["firmware_size"]),
        firmware_sha256=str(raw["firmware_sha256"]).lower(),
        project_id=project_id,
        version=version,
    )


def read_ota_package(path: str | Path) -> OtaPackage:
    resolved = Path(path).expanduser().resolve()
    if resolved.suffix.lower() != ".ota":
        raise ValueError(f"unsupported OTA package format: {resolved.suffix}, only .ota is allowed")

    try:
        with zipfile.ZipFile(resolved, "r") as zf:
            try:
                manifest_data = zf.read("manifest.json")
            except KeyError as exc:
                raise ValueError(".ota 中缺少 manifest.json") from exc

            manifest = _parse_ota_manifest(manifest_data)

            try:
                firmware_blob = zf.read(manifest.firmware_name)
            except KeyError as exc:
                raise ValueError(f".ota 中缺少固件文件: {manifest.firmware_name}") from exc
    except zipfile.BadZipFile as exc:
        raise ValueError(f".ota 文件损坏: {exc}") from exc

    firmware_path = Path(manifest.firmware_name)
    if firmware_path.suffix.lower().lstrip(".") != manifest.firmware_format:
        raise ValueError("manifest.json 中的固件格式与文件后缀不一致")

    if manifest.firmware_format == "bin":
        firmware = firmware_blob
    else:
        try:
            firmware = _load_hex_firmware_from_text(firmware_blob.decode("utf-8"))
        except UnicodeDecodeError as exc:
            raise ValueError(f"HEX 固件解码失败: {exc}") from exc

    if len(firmware) != manifest.firmware_size:
        raise ValueError(
            f"固件大小与 manifest 不一致: manifest={manifest.firmware_size}, actual={len(firmware)}"
        )

    firmware_sha256 = hashlib.sha256(firmware).hexdigest()
    if firmware_sha256.lower() != manifest.firmware_sha256.lower():
        raise ValueError("固件 SHA-256 与 manifest 不一致")

    if manifest.format_version != 1:
        raise ValueError(f"不支持的 OTA 容器版本: {manifest.format_version}")

    return OtaPackage(path=resolved, manifest=manifest, firmware=firmware)


def _load_hex_firmware_from_text(text: str) -> bytes:
    image: dict[int, int] = {}
    upper_addr = 0
    start_addr: int | None = None
    end_addr = 0

    for line_no, raw_line in enumerate(text.splitlines(), start=1):
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
            if absolute_addr + len(data) > end_addr:
                end_addr = absolute_addr + len(data)
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


def decode_start(payload: bytes) -> StartResponse:
    return StartResponse(*START_RESP.unpack(payload))


def decode_query(payload: bytes) -> QueryVersionResponse:
    error_code, allow_upgrade, major, minor, patch, project_id_raw = QUERY_RESP.unpack(payload)
    project_id = project_id_raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    return QueryVersionResponse(
        error_code=error_code,
        allow_upgrade=bool(allow_upgrade),
        version=(major, minor, patch),
        project_id=project_id,
    )


class SmotaTcpClient:
    def __init__(self, host: str, port: int, timeout: float, logger: LogFn | None = None) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.logger = logger
        self.sock: socket.socket | None = None

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.settimeout(self.timeout)
        self._log("INFO", f"connected to {self.host}:{self.port}")

    def set_timeout(self, timeout: float) -> None:
        self.timeout = timeout
        if self.sock is not None:
            self.sock.settimeout(timeout)

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None
            self._log("INFO", "connection closed")

    def exchange(self, cmd: int, expected_cmd: int, payload: bytes = b"") -> Frame:
        frame = self._build_frame(cmd, payload)

        for attempt in range(NO_RESPONSE_RETRY_COUNT + 1):
            self._send_raw_frame(frame, cmd, len(payload))
            try:
                response = self.recv_frame()
            except (TimeoutError, socket.timeout) as exc:
                if attempt >= NO_RESPONSE_RETRY_COUNT:
                    raise TimeoutError(
                        f"{cmd_name(cmd)} 无应答，已重传 {NO_RESPONSE_RETRY_COUNT} 次"
                    ) from exc
                self._log(
                    "INFO",
                    f"{cmd_name(cmd)} 第 {attempt + 1} 次无应答，准备重传",
                )
                continue

            if response.cmd != expected_cmd:
                raise RuntimeError(
                    f"unexpected response {cmd_name(response.cmd)} for {cmd_name(cmd)}, "
                    f"expected {cmd_name(expected_cmd)}"
                )

            return response

        raise RuntimeError(f"{cmd_name(cmd)} exchange failed")

    def send_frame(self, cmd: int, payload: bytes = b"") -> None:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        frame = self._build_frame(cmd, payload)
        self._send_raw_frame(frame, cmd, len(payload))

    def recv_frame(self) -> Frame:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        header = self._recv_exact(FRAME_HEADER.size)
        sof, cmd, payload_len = FRAME_HEADER.unpack(header)
        if sof != SOF:
            raise RuntimeError(f"invalid SOF: {sof!r}")

        payload = self._recv_exact(payload_len)
        frame_crc = CRC16.unpack(self._recv_exact(CRC16.size))[0]
        calc_crc = crc16_ccitt(header + payload)
        if frame_crc != calc_crc:
            raise RuntimeError("CRC mismatch")

        frame = header + payload + CRC16.pack(frame_crc)
        self._log(
            "RX",
            f"{cmd_name(cmd)} len={len(payload)} frame={frame_hex(frame)}",
        )
        return Frame(cmd=cmd, payload=payload)

    def _recv_exact(self, size: int) -> bytes:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        data = bytearray()
        while len(data) < size:
            chunk = self.sock.recv(size - len(data))
            if not chunk:
                raise ConnectionError("device closed the connection")
            data.extend(chunk)
        return bytes(data)

    def _build_frame(self, cmd: int, payload: bytes) -> bytes:
        header = FRAME_HEADER.pack(SOF, cmd, len(payload))
        return header + payload + CRC16.pack(crc16_ccitt(header + payload))

    def _send_raw_frame(self, frame: bytes, cmd: int, payload_len: int) -> None:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        self.sock.sendall(frame)
        self._log(
            "TX",
            f"{cmd_name(cmd)} len={payload_len} frame={frame_hex(frame)}",
        )

    def _log(self, level: str, message: str) -> None:
        if self.logger is not None:
            self.logger(level, message)


class SmotaSerialClient:
    def __init__(
        self,
        port: str,
        baudrate: int,
        timeout: float,
        logger: LogFn | None = None,
        serial_connection: Any | None = None,
        close_on_close: bool = True,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.logger = logger
        self.ser: serial.Serial | None = serial_connection
        self.external_serial = serial_connection
        self.close_on_close = close_on_close

    def connect(self) -> None:
        if serial is None:
            raise RuntimeError("pyserial is not installed, run: pip install pyserial")

        if self.ser is None and self.external_serial is not None:
            self.ser = self.external_serial

        if self.ser is not None:
            if not getattr(self.ser, "is_open", True):
                self.ser.open()
            self.ser.baudrate = self.baudrate
            self.ser.timeout = self.timeout
            self.ser.write_timeout = self.timeout
            self._log("INFO", f"using opened serial {self.port}@{self.baudrate}")
            return

        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=self.timeout,
            write_timeout=self.timeout,
        )
        self._log("INFO", f"connected to serial {self.port}@{self.baudrate}")

    def set_timeout(self, timeout: float) -> None:
        self.timeout = timeout
        if self.ser is not None:
            self.ser.timeout = timeout
            self.ser.write_timeout = timeout

    def close(self) -> None:
        if self.ser is not None:
            current = self.ser
            try:
                if self.close_on_close:
                    try:
                        current.close()
                        self._log("INFO", "connection closed")
                    except Exception as exc:
                        self._log("INFO", f"connection close failed: {exc}")
                else:
                    self._log("INFO", "serial connection released")
            finally:
                if self.close_on_close and current is self.external_serial:
                    self.external_serial = None
                self.ser = None

    def exchange(self, cmd: int, expected_cmd: int, payload: bytes = b"") -> Frame:
        frame = self._build_frame(cmd, payload)

        for attempt in range(NO_RESPONSE_RETRY_COUNT + 1):
            self._send_raw_frame(frame, cmd, len(payload))
            try:
                response = self.recv_frame()
            except TimeoutError as exc:
                if attempt >= NO_RESPONSE_RETRY_COUNT:
                    raise TimeoutError(
                        f"{cmd_name(cmd)} 无应答，已重传 {NO_RESPONSE_RETRY_COUNT} 次"
                    ) from exc
                self._log(
                    "INFO",
                    f"{cmd_name(cmd)} 第 {attempt + 1} 次无应答，准备重传",
                )
                continue

            if response.cmd != expected_cmd:
                raise RuntimeError(
                    f"unexpected response {cmd_name(response.cmd)} for {cmd_name(cmd)}, "
                    f"expected {cmd_name(expected_cmd)}"
                )

            return response

        raise RuntimeError(f"{cmd_name(cmd)} exchange failed")

    def send_frame(self, cmd: int, payload: bytes = b"") -> None:
        if self.ser is None:
            raise RuntimeError("serial is not connected")

        frame = self._build_frame(cmd, payload)
        self._send_raw_frame(frame, cmd, len(payload))

    def recv_frame(self) -> Frame:
        header = self._read_exact(FRAME_HEADER.size)
        sof, cmd, payload_len = FRAME_HEADER.unpack(header)
        if sof != SOF:
            raise RuntimeError(f"invalid SOF: {sof!r}")

        payload = self._read_exact(payload_len)
        frame_crc = CRC16.unpack(self._read_exact(CRC16.size))[0]
        calc_crc = crc16_ccitt(header + payload)
        if frame_crc != calc_crc:
            raise RuntimeError("CRC mismatch")

        frame = header + payload + CRC16.pack(frame_crc)
        self._log(
            "RX",
            f"{cmd_name(cmd)} len={len(payload)} frame={frame_hex(frame)}",
        )
        return Frame(cmd=cmd, payload=payload)

    def _read_exact(self, size: int) -> bytes:
        if self.ser is None:
            raise RuntimeError("serial is not connected")

        data = bytearray()
        while len(data) < size:
            chunk = self.ser.read(size - len(data))
            if not chunk:
                raise TimeoutError("serial read timeout")
            data.extend(chunk)
        return bytes(data)

    def _build_frame(self, cmd: int, payload: bytes) -> bytes:
        header = FRAME_HEADER.pack(SOF, cmd, len(payload))
        return header + payload + CRC16.pack(crc16_ccitt(header + payload))

    def _send_raw_frame(self, frame: bytes, cmd: int, payload_len: int) -> None:
        if self.ser is None:
            raise RuntimeError("serial is not connected")

        self.ser.write(frame)
        self.ser.flush()
        self._log(
            "TX",
            f"{cmd_name(cmd)} len={payload_len} frame={frame_hex(frame)}",
        )

    def _log(self, level: str, message: str) -> None:
        if self.logger is not None:
            self.logger(level, message)


def list_serial_ports() -> list[SerialPortInfo]:
    if serial is None:
        return []

    ports: list[SerialPortInfo] = []
    for port in serial.tools.list_ports.comports():
        description = (port.description or "").strip()
        if description and description != port.device:
            label = f"{port.device} - {description}"
        else:
            label = port.device
        ports.append(SerialPortInfo(device=port.device, label=label))
    return ports


def cmd_name(cmd: int) -> str:
    return CMD_NAMES.get(cmd, f"0x{cmd:02X}")
