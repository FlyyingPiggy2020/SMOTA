from __future__ import annotations

import socket
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


LogFn = Callable[[str, str], None]

SOF = b"smOTA"
FRAME_HEADER = struct.Struct("<5sBBBBH")
CRC16 = struct.Struct("<H")

CMD_HANDSHAKE = 0x01
CMD_HEADER_INFO = 0x02
CMD_DATA_BLOCK = 0x03
CMD_DATA_COMPLETE = 0x04
CMD_INSTALL = 0x05
CMD_ACTIVATE_CHECK = 0x06

CMD_HANDSHAKE_RESP = 0x81
CMD_HEADER_INFO_RESP = 0x82
CMD_DATA_BLOCK_RESP = 0x83
CMD_DATA_COMPLETE_RESP = 0x84
CMD_INSTALL_RESP = 0x85
CMD_ACTIVATE_CHECK_RESP = 0x86

HANDSHAKE_REQ = struct.Struct("<BBBI16sHHHI")
HANDSHAKE_RESP = struct.Struct("<IIHHIHHB")
HEADER_INFO_REQ = struct.Struct("<32s32s32s")
DATA_BLOCK_REQ = struct.Struct("<IH")
DATA_BLOCK_RESP = struct.Struct("<II")
TRANSFER_COMPLETE_REQ = struct.Struct("<I")
TRANSFER_COMPLETE_RESP = struct.Struct("<I")
INSTALL_REQ = struct.Struct("<B15s")
INSTALL_RESP = struct.Struct("<IH")
ACTIVATE_CHECK_REQ = struct.Struct("<I")
ACTIVATE_CHECK_RESP = struct.Struct("<IBBB")

CMD_NAMES = {
    CMD_HANDSHAKE: "HANDSHAKE",
    CMD_HEADER_INFO: "HEADER_INFO",
    CMD_DATA_BLOCK: "DATA_BLOCK",
    CMD_DATA_COMPLETE: "DATA_COMPLETE",
    CMD_INSTALL: "INSTALL",
    CMD_ACTIVATE_CHECK: "ACTIVATE_CHECK",
    CMD_HANDSHAKE_RESP: "HANDSHAKE_RESP",
    CMD_HEADER_INFO_RESP: "HEADER_INFO_RESP",
    CMD_DATA_BLOCK_RESP: "DATA_BLOCK_RESP",
    CMD_DATA_COMPLETE_RESP: "DATA_COMPLETE_RESP",
    CMD_INSTALL_RESP: "INSTALL_RESP",
    CMD_ACTIVATE_CHECK_RESP: "ACTIVATE_CHECK_RESP",
}


@dataclass(frozen=True)
class Frame:
    cmd: int
    seq: int
    payload: bytes


@dataclass(frozen=True)
class HandshakeResponse:
    error_code: int
    next_offset: int
    max_packet_size: int
    mtu_size: int
    flash_free_size: int
    block_timeout: int
    install_timeout: int
    capabilities: int


@dataclass(frozen=True)
class ActivateCheckResponse:
    error_code: int
    version: tuple[int, int, int]


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


def read_firmware(path: str | Path) -> bytes:
    return Path(path).expanduser().resolve().read_bytes()


def decode_handshake(payload: bytes) -> HandshakeResponse:
    return HandshakeResponse(*HANDSHAKE_RESP.unpack(payload))


def decode_activate_check(payload: bytes) -> ActivateCheckResponse:
    error_code, major, minor, patch = ACTIVATE_CHECK_RESP.unpack(payload)
    return ActivateCheckResponse(error_code=error_code, version=(major, minor, patch))


class SmotaTcpClient:
    def __init__(self, host: str, port: int, timeout: float, logger: LogFn | None = None) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.logger = logger
        self.seq = 0
        self.sock: socket.socket | None = None

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.settimeout(self.timeout)
        self._log("INFO", f"connected to {self.host}:{self.port}")

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None
            self._log("INFO", "connection closed")

    def exchange(self, cmd: int, expected_cmd: int, payload: bytes = b"") -> Frame:
        self.send_frame(cmd, payload)
        frame = self.recv_frame()
        if frame.cmd != expected_cmd:
            raise RuntimeError(
                f"unexpected response {cmd_name(frame.cmd)} for {cmd_name(cmd)}, "
                f"expected {cmd_name(expected_cmd)}"
            )
        return frame

    def send_frame(self, cmd: int, payload: bytes = b"") -> None:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        header = FRAME_HEADER.pack(SOF, 0x00, 0x00, self.seq & 0xFF, cmd, len(payload))
        frame = header + payload + CRC16.pack(crc16_ccitt(header + payload))
        self.sock.sendall(frame)
        self._log(
            "TX",
            f"{cmd_name(cmd)} seq={self.seq & 0xFF} len={len(payload)} payload={payload_preview(payload)}",
        )
        self.seq = (self.seq + 1) & 0xFF

    def recv_frame(self) -> Frame:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        header = self._recv_exact(FRAME_HEADER.size)
        sof, _ver, _frag, seq, cmd, payload_len = FRAME_HEADER.unpack(header)
        if sof != SOF:
            raise RuntimeError(f"invalid SOF: {sof!r}")

        payload = self._recv_exact(payload_len)
        frame_crc = CRC16.unpack(self._recv_exact(CRC16.size))[0]
        calc_crc = crc16_ccitt(header + payload)
        if frame_crc != calc_crc:
            raise RuntimeError("CRC mismatch")

        self._log(
            "RX",
            f"{cmd_name(cmd)} seq={seq} len={len(payload)} payload={payload_preview(payload)}",
        )
        return Frame(cmd=cmd, seq=seq, payload=payload)

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

    def _log(self, level: str, message: str) -> None:
        if self.logger is not None:
            self.logger(level, message)


def cmd_name(cmd: int) -> str:
    return CMD_NAMES.get(cmd, f"0x{cmd:02X}")
