#!/usr/bin/env python3
"""
smOTA Windows 仿真上位机。

默认行为：
1. 可选重置 win_sim 状态
2. 启动 win_sim TCP 仿真器
3. 执行完整 OTA 流程
4. 等待模拟器重启
5. 重新连接并校验升级后的版本
"""

from __future__ import annotations

import argparse
import hashlib
import socket
import struct
import subprocess
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass
from pathlib import Path


SOF = b"smOTA"
FRAME_HEADER = struct.Struct("<5sBBBBH")
CRC16 = struct.Struct("<H")

CMD_HANDSHAKE = 0x01
CMD_HEADER_INFO = 0x02
CMD_DATA_BLOCK = 0x03
CMD_DATA_COMPLETE = 0x04
CMD_INSTALL = 0x05
CMD_ACTIVATE_CHECK = 0x06
CMD_QUERY_VERSION = 0x07

CMD_HANDSHAKE_RESP = 0x81
CMD_HEADER_INFO_RESP = 0x82
CMD_DATA_BLOCK_RESP = 0x83
CMD_DATA_COMPLETE_RESP = 0x84
CMD_INSTALL_RESP = 0x85
CMD_ACTIVATE_CHECK_RESP = 0x86
CMD_QUERY_VERSION_RESP = 0x87

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
QUERY_VERSION_REQ = struct.Struct("<I")
QUERY_VERSION_RESP = struct.Struct("<IBBB")


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


def parse_version(text: str) -> tuple[int, int, int]:
    parts = text.split(".")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("version must be in major.minor.patch format")
    try:
        values = tuple(int(part) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("version must contain integers") from exc
    for value in values:
        if value < 0 or value > 255:
            raise argparse.ArgumentTypeError("each version field must be between 0 and 255")
    return values


def format_version(version: tuple[int, int, int]) -> str:
    return f"{version[0]}.{version[1]}.{version[2]}"


def compare_version(left: tuple[int, int, int], right: tuple[int, int, int]) -> int:
    if left > right:
        return 1
    if left < right:
        return -1
    return 0


@dataclass
class Frame:
    cmd: int
    seq: int
    payload: bytes


@dataclass
class HandshakeResponseData:
    error_code: int
    next_offset: int
    max_packet_size: int
    mtu_size: int
    flash_free_size: int
    block_timeout: int
    install_timeout: int
    capabilities: int


class WinSimProcess:
    def __init__(self, exe_path: Path, verbose: bool) -> None:
        self.exe_path = exe_path
        self.cwd = exe_path.parent
        self.verbose = verbose
        self.proc: subprocess.Popen[str] | None = None
        self._log_thread: threading.Thread | None = None
        self._log_buffer: deque[str] = deque(maxlen=80)

    def reset(self) -> None:
        subprocess.run(
            [str(self.exe_path), "-i"],
            cwd=self.cwd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

    def start(self) -> None:
        if self.proc is not None and self.proc.poll() is None:
            raise RuntimeError("simulator is already running")

        self.proc = subprocess.Popen(
            [str(self.exe_path), "-r"],
            cwd=self.cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        self._log_thread = threading.Thread(target=self._pump_logs, daemon=True)
        self._log_thread.start()

    def wait(self, timeout: float) -> None:
        if self.proc is None:
            return
        self.proc.wait(timeout=timeout)

    def stop(self) -> None:
        if self.proc is None:
            return
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
        self.proc = None

    def last_logs(self) -> str:
        return "".join(self._log_buffer)

    def _pump_logs(self) -> None:
        assert self.proc is not None
        assert self.proc.stdout is not None
        for line in self.proc.stdout:
            self._log_buffer.append(line)
            if self.verbose:
                print(f"[SIM] {line.rstrip()}")


class SmotaClient:
    def __init__(self, host: str, port: int, timeout: float) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.seq = 0
        self.sock: socket.socket | None = None

    def connect_with_retry(self, deadline_s: float) -> None:
        end_time = time.time() + deadline_s
        while time.time() < end_time:
            try:
                self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
                self.sock.settimeout(self.timeout)
                return
            except OSError:
                time.sleep(0.2)
        raise TimeoutError(f"unable to connect to {self.host}:{self.port}")

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def exchange(self, cmd: int, expected_cmd: int, payload: bytes = b"") -> Frame:
        self.send_frame(cmd, payload)
        frame = self.recv_frame()
        if frame.cmd != expected_cmd:
            raise RuntimeError(f"unexpected response cmd 0x{frame.cmd:02X}, expected 0x{expected_cmd:02X}")
        return frame

    def send_frame(self, cmd: int, payload: bytes = b"") -> None:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        header = FRAME_HEADER.pack(SOF, 0x00, 0x00, self.seq & 0xFF, cmd, len(payload))
        body = header + payload
        crc = CRC16.pack(crc16_ccitt(body))
        self.sock.sendall(body + crc)
        self.seq = (self.seq + 1) & 0xFF

    def recv_frame(self) -> Frame:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        header = self._recv_exact(FRAME_HEADER.size)
        sof, _ver, _frag, seq, cmd, payload_len = FRAME_HEADER.unpack(header)
        if sof != SOF:
            raise RuntimeError(f"invalid SOF: {sof!r}")

        payload = self._recv_exact(payload_len)
        crc_bytes = self._recv_exact(CRC16.size)
        expected_crc = CRC16.unpack(crc_bytes)[0]
        actual_crc = crc16_ccitt(header + payload)
        if expected_crc != actual_crc:
            raise RuntimeError("CRC mismatch")

        return Frame(cmd=cmd, seq=seq, payload=payload)

    def _recv_exact(self, size: int) -> bytes:
        if self.sock is None:
            raise RuntimeError("socket is not connected")

        chunks = bytearray()
        while len(chunks) < size:
            chunk = self.sock.recv(size - len(chunks))
            if not chunk:
                raise ConnectionError("socket closed while receiving data")
            chunks.extend(chunk)
        return bytes(chunks)


def decode_handshake(payload: bytes) -> HandshakeResponseData:
    unpacked = HANDSHAKE_RESP.unpack(payload)
    return HandshakeResponseData(*unpacked)


def decode_version_payload(payload: bytes) -> tuple[int, tuple[int, int, int]]:
    error_code, major, minor, patch = QUERY_VERSION_RESP.unpack(payload)
    return error_code, (major, minor, patch)


def decode_activate_payload(payload: bytes) -> tuple[int, tuple[int, int, int]]:
    error_code, major, minor, patch = ACTIVATE_CHECK_RESP.unpack(payload)
    return error_code, (major, minor, patch)


def run_upgrade(args: argparse.Namespace) -> int:
    firmware_path = Path(args.firmware).resolve()
    version = args.version
    project_id = args.project_id.encode("utf-8")[:16].ljust(16, b"\x00")

    sim: WinSimProcess | None = None
    if args.sim_exe is not None:
        sim = WinSimProcess(Path(args.sim_exe).resolve(), args.verbose_sim)
        if args.reset_sim:
            print("Resetting simulator state")
            sim.reset()
        print("Starting simulator")
        sim.start()

    client = SmotaClient(args.host, args.port, args.timeout)

    try:
        client.connect_with_retry(args.connect_timeout)
        print(f"Connected to {args.host}:{args.port}")

        try:
            version_frame = client.exchange(
                CMD_QUERY_VERSION,
                CMD_QUERY_VERSION_RESP,
                QUERY_VERSION_REQ.pack(0),
            )
            version_error, running_version = decode_version_payload(version_frame.payload)
            if version_error != 0:
                raise RuntimeError(f"query version failed with error code 0x{version_error:08X}")
        except Exception:
            version_frame = client.exchange(
                CMD_ACTIVATE_CHECK,
                CMD_ACTIVATE_CHECK_RESP,
                ACTIVATE_CHECK_REQ.pack(0),
            )
            version_error, running_version = decode_activate_payload(version_frame.payload)
            if version_error != 0:
                raise RuntimeError(f"activate check query failed with error code 0x{version_error:08X}")

        print(f"Running version: {format_version(running_version)}")
        version_cmp = compare_version(running_version, version)
        if not args.force_install:
            if version_cmp == 0:
                print("Running version matches target, skip download")
                return 0
            if version_cmp > 0:
                raise RuntimeError(
                    f"running version {format_version(running_version)} is newer than target "
                    f"{format_version(version)}, use --force-install to continue"
                )

        firmware = firmware_path.read_bytes()
        firmware_hash = hashlib.sha256(firmware).digest()

        handshake_payload = HANDSHAKE_REQ.pack(
            version[0],
            version[1],
            version[2],
            len(firmware),
            project_id,
            args.block_timeout_ms,
            args.check_timeout_ms,
            args.install_timeout_ms,
            args.total_timeout_ms,
        )
        handshake_frame = client.exchange(CMD_HANDSHAKE, CMD_HANDSHAKE_RESP, handshake_payload)
        handshake = decode_handshake(handshake_frame.payload)
        if handshake.error_code != 0:
            raise RuntimeError(f"handshake failed with error code 0x{handshake.error_code:08X}")

        print(
            "Handshake ok: "
            f"max_packet={handshake.max_packet_size}, "
            f"flash_free={handshake.flash_free_size}, "
            f"next_offset={handshake.next_offset}"
        )

        header_payload = HEADER_INFO_REQ.pack(firmware_hash, bytes(32), bytes(32))
        header_frame = client.exchange(CMD_HEADER_INFO, CMD_HEADER_INFO_RESP, header_payload)
        header_error = struct.unpack("<I", header_frame.payload)[0]
        if header_error != 0:
            raise RuntimeError(f"header info failed with error code 0x{header_error:08X}")

        data_payload_max = max(1, min(args.chunk_size, handshake.max_packet_size - DATA_BLOCK_REQ.size))
        print(f"Transferring firmware in {data_payload_max}-byte blocks")

        offset = handshake.next_offset
        while offset < len(firmware):
            chunk = firmware[offset:offset + data_payload_max]
            block_payload = DATA_BLOCK_REQ.pack(offset, len(chunk)) + chunk
            block_frame = client.exchange(CMD_DATA_BLOCK, CMD_DATA_BLOCK_RESP, block_payload)
            block_error, received_offset = DATA_BLOCK_RESP.unpack(block_frame.payload)
            if block_error != 0:
                raise RuntimeError(f"data block failed at offset {offset}, error code 0x{block_error:08X}")
            if received_offset != offset + len(chunk):
                raise RuntimeError(
                    f"device acknowledged offset {received_offset}, expected {offset + len(chunk)}"
                )
            offset = received_offset
            print(f"\rProgress: {offset}/{len(firmware)} bytes", end="", flush=True)
        print()

        complete_frame = client.exchange(
            CMD_DATA_COMPLETE,
            CMD_DATA_COMPLETE_RESP,
            TRANSFER_COMPLETE_REQ.pack(len(firmware)),
        )
        complete_error = TRANSFER_COMPLETE_RESP.unpack(complete_frame.payload)[0]
        if complete_error != 0:
            raise RuntimeError(f"transfer verification failed with error code 0x{complete_error:08X}")
        print("Transfer verification ok")

        install_frame = client.exchange(
            CMD_INSTALL,
            CMD_INSTALL_RESP,
            INSTALL_REQ.pack(1 if args.force_install else 0, bytes(15)),
        )
        install_error, estimated_time_s = INSTALL_RESP.unpack(install_frame.payload)
        if install_error != 0:
            raise RuntimeError(f"install request failed with error code 0x{install_error:08X}")
        print(f"Install accepted, estimated reboot time {estimated_time_s}s")
        client.close()

        if args.skip_activate_check:
            print("Upgrade finished without activate check")
            return 0

        if sim is not None:
            sim.wait(args.install_timeout_ms / 1000.0 + 2.0)
            print("Restarting simulator for activate check")
            sim.start()

        client = SmotaClient(args.host, args.port, args.timeout)
        client.connect_with_retry(args.install_timeout_ms / 1000.0 + 5.0)
        activate_frame = client.exchange(
            CMD_ACTIVATE_CHECK,
            CMD_ACTIVATE_CHECK_RESP,
            ACTIVATE_CHECK_REQ.pack(0),
        )
        activate_error, major, minor, patch = ACTIVATE_CHECK_RESP.unpack(activate_frame.payload)
        if activate_error != 0:
            raise RuntimeError(f"activate check failed with error code 0x{activate_error:08X}")

        running_version = (major, minor, patch)
        print(f"Activate check ok, running version {format_version(running_version)}")
        if running_version != version:
            raise RuntimeError(
                f"running version {format_version(running_version)} does not match target {format_version(version)}"
            )

        print("OTA simulation completed successfully")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        if sim is not None:
            if sim.proc is not None and sim.proc.poll() is not None:
                print(f"Simulator exited with code {sim.proc.returncode}", file=sys.stderr)
            logs = sim.last_logs().strip()
            if logs:
                print("Simulator logs:", file=sys.stderr)
                print(logs, file=sys.stderr)
        return 1
    finally:
        client.close()
        if sim is not None:
            sim.stop()


def build_parser() -> argparse.ArgumentParser:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="Run a full smOTA upgrade against win_sim")
    parser.add_argument(
        "--firmware",
        default=str(script_dir / "README.md"),
        help="path to the firmware image used for simulation",
    )
    parser.add_argument(
        "--version",
        type=parse_version,
        default=(1, 2, 3),
        help="target firmware version in major.minor.patch format",
    )
    parser.add_argument("--project-id", default="SMOTA_WIN_SIM", help="project identifier sent in handshake")
    parser.add_argument("--host", default="127.0.0.1", help="device host")
    parser.add_argument("--port", type=int, default=8888, help="device TCP port")
    parser.add_argument("--timeout", type=float, default=3.0, help="socket timeout in seconds")
    parser.add_argument("--connect-timeout", type=float, default=8.0, help="initial connect timeout in seconds")
    parser.add_argument("--chunk-size", type=int, default=128, help="requested data block size")
    parser.add_argument("--block-timeout-ms", type=int, default=5000, help="block timeout in milliseconds")
    parser.add_argument("--check-timeout-ms", type=int, default=30000, help="verify timeout in milliseconds")
    parser.add_argument("--install-timeout-ms", type=int, default=15000, help="install timeout in milliseconds")
    parser.add_argument("--total-timeout-ms", type=int, default=60000, help="overall timeout in milliseconds")
    parser.add_argument("--force-install", action="store_true", help="set force_install flag")
    parser.add_argument("--skip-activate-check", action="store_true", help="stop after install response")
    parser.add_argument("--verbose-sim", action="store_true", help="print simulator stdout")
    parser.add_argument(
        "--sim-exe",
        default=str(script_dir / "build" / "win_sim.exe"),
        help="path to win_sim executable; set empty string to connect to an already running device",
    )
    parser.add_argument("--reset-sim", action="store_true", help="reset simulator state before upgrade")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.sim_exe == "":
        args.sim_exe = None
    return run_upgrade(args)


if __name__ == "__main__":
    raise SystemExit(main())
