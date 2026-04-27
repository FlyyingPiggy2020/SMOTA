from __future__ import annotations

import hashlib
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .protocol import (
    ACTIVATE_CHECK_REQ,
    QUERY_VERSION_REQ,
    CMD_ACTIVATE_CHECK,
    CMD_ACTIVATE_CHECK_RESP,
    CMD_DATA_BLOCK,
    CMD_DATA_BLOCK_RESP,
    CMD_DATA_COMPLETE,
    CMD_DATA_COMPLETE_RESP,
    CMD_HANDSHAKE,
    CMD_HANDSHAKE_RESP,
    CMD_HEADER_INFO,
    CMD_HEADER_INFO_RESP,
    CMD_INSTALL,
    CMD_INSTALL_RESP,
    CMD_QUERY_VERSION,
    CMD_QUERY_VERSION_RESP,
    DATA_BLOCK_REQ,
    DATA_BLOCK_RESP,
    HANDSHAKE_REQ,
    HEADER_INFO_REQ,
    INSTALL_REQ,
    INSTALL_RESP,
    SmotaSerialClient,
    SmotaTcpClient,
    TRANSFER_COMPLETE_REQ,
    TRANSFER_COMPLETE_RESP,
    compare_version,
    decode_activate_check,
    decode_handshake,
    decode_query_version,
    format_version,
    read_firmware,
)


LogFn = Callable[[str, str], None]
ProgressFn = Callable[[int], None]
StatusFn = Callable[[str], None]


@dataclass(frozen=True)
class UpgradeConfig:
    firmware_path: Path
    version: tuple[int, int, int]
    project_id: str
    transport: str
    host: str
    port: int
    serial_port: str
    serial_baudrate: int
    timeout_s: float
    connect_timeout_s: float
    chunk_size: int
    block_timeout_ms: int
    check_timeout_ms: int
    install_timeout_ms: int
    total_timeout_ms: int
    force_install: bool
    activate_check: bool


class UpgradeSession:
    def __init__(
        self,
        config: UpgradeConfig,
        logger: LogFn,
        progress: ProgressFn,
        status: StatusFn,
    ) -> None:
        self.config = config
        self.logger = logger
        self.progress = progress
        self.status = status
        self._stop_event = threading.Event()

    def request_stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        project_id = self.config.project_id.encode("utf-8")[:16].ljust(16, b"\x00")
        if self.config.transport == "serial":
            client = SmotaSerialClient(
                port=self.config.serial_port,
                baudrate=self.config.serial_baudrate,
                timeout=self.config.timeout_s,
                logger=self.logger,
            )
            self.logger("INFO", f"transport=serial port={self.config.serial_port} baud={self.config.serial_baudrate}")
        else:
            client = SmotaTcpClient(
                host=self.config.host,
                port=self.config.port,
                timeout=self.config.timeout_s,
                logger=self.logger,
            )
            self.logger("INFO", f"transport=tcp endpoint={self.config.host}:{self.config.port}")

        self.logger("INFO", f"target version={format_version(self.config.version)}")
        self.progress(0)

        try:
            self.status("Connecting")
            self._connect_with_retry(client, self.config.connect_timeout_s)
            self._ensure_not_stopped()

            self.status("Query Version")
            running_version = self._query_running_version(client)
            self.logger("INFO", f"running version={format_version(running_version)}")

            compare_result = compare_version(running_version, self.config.version)
            if not self.config.force_install:
                if compare_result == 0:
                    self.logger("INFO", "running version already matches target, skip download")
                    self.status("Skipped")
                    self.progress(100)
                    return
                if compare_result > 0:
                    raise RuntimeError(
                        "running version is newer than target: "
                        f"{format_version(running_version)} > {format_version(self.config.version)}; "
                        "enable Force Install to continue"
                    )
            else:
                self.logger("INFO", "force install enabled, continue upgrade flow")

            firmware = read_firmware(self.config.firmware_path)
            firmware_hash = hashlib.sha256(firmware).digest()
            self.logger("INFO", f"firmware={self.config.firmware_path}")
            self.logger("INFO", f"firmware size={len(firmware)} bytes")
            self.logger("INFO", f"sha256={firmware_hash.hex()}")

            self.status("Handshake")
            handshake_frame = client.exchange(
                CMD_HANDSHAKE,
                CMD_HANDSHAKE_RESP,
                HANDSHAKE_REQ.pack(
                    self.config.version[0],
                    self.config.version[1],
                    self.config.version[2],
                    len(firmware),
                    project_id,
                    self.config.block_timeout_ms,
                    self.config.check_timeout_ms,
                    self.config.install_timeout_ms,
                    self.config.total_timeout_ms,
                ),
            )
            handshake = decode_handshake(handshake_frame.payload)
            if handshake.error_code != 0:
                raise RuntimeError(f"handshake failed: 0x{handshake.error_code:08X}")

            self.logger(
                "INFO",
                "handshake ok "
                f"max_packet={handshake.max_packet_size} "
                f"flash_free={handshake.flash_free_size} "
                f"next_offset={handshake.next_offset}",
            )

            self.status("Header Info")
            header_frame = client.exchange(
                CMD_HEADER_INFO,
                CMD_HEADER_INFO_RESP,
                HEADER_INFO_REQ.pack(firmware_hash, bytes(32), bytes(32)),
            )
            header_error = int.from_bytes(header_frame.payload[:4], "little")
            if header_error != 0:
                raise RuntimeError(f"header info failed: 0x{header_error:08X}")

            self.status("Transferring")
            payload_size = max(1, min(self.config.chunk_size, handshake.max_packet_size - DATA_BLOCK_REQ.size))
            offset = handshake.next_offset
            while offset < len(firmware):
                self._ensure_not_stopped()
                chunk = firmware[offset:offset + payload_size]
                frame = client.exchange(
                    CMD_DATA_BLOCK,
                    CMD_DATA_BLOCK_RESP,
                    DATA_BLOCK_REQ.pack(offset, len(chunk)) + chunk,
                )
                block_error, received_offset = DATA_BLOCK_RESP.unpack(frame.payload)
                if block_error != 0:
                    raise RuntimeError(f"data block failed at offset {offset}: 0x{block_error:08X}")
                if received_offset != offset + len(chunk):
                    raise RuntimeError(
                        f"device acknowledged offset {received_offset}, expected {offset + len(chunk)}"
                    )
                offset = received_offset
                self.progress(int(offset * 100 / len(firmware)))
                self.logger("INFO", f"progress {offset}/{len(firmware)} bytes")

            self.status("Verifying")
            complete_frame = client.exchange(
                CMD_DATA_COMPLETE,
                CMD_DATA_COMPLETE_RESP,
                TRANSFER_COMPLETE_REQ.pack(len(firmware)),
            )
            complete_error = TRANSFER_COMPLETE_RESP.unpack(complete_frame.payload)[0]
            if complete_error != 0:
                raise RuntimeError(f"transfer verify failed: 0x{complete_error:08X}")
            self.logger("INFO", "transfer verification ok")

            self.status("Installing")
            install_frame = client.exchange(
                CMD_INSTALL,
                CMD_INSTALL_RESP,
                INSTALL_REQ.pack(1 if self.config.force_install else 0, bytes(15)),
            )
            install_error, estimated_time_s = INSTALL_RESP.unpack(install_frame.payload)
            if install_error != 0:
                raise RuntimeError(f"install request failed: 0x{install_error:08X}")
            self.logger("INFO", f"install accepted, estimated reboot time {estimated_time_s}s")
            client.close()

            if not self.config.activate_check:
                self.status("Completed")
                self.progress(100)
                return

            self.status("Waiting Reconnect")
            self.logger(
                "INFO",
                "device will disconnect now; restart win_sim and keep this window running for activate check",
            )
            self._connect_with_retry(client, self.config.install_timeout_ms / 1000.0 + 5.0)
            self._ensure_not_stopped()

            self.status("Activate Check")
            activate_frame = client.exchange(
                CMD_ACTIVATE_CHECK,
                CMD_ACTIVATE_CHECK_RESP,
                ACTIVATE_CHECK_REQ.pack(0),
            )
            activate = decode_activate_check(activate_frame.payload)
            if activate.error_code != 0:
                raise RuntimeError(f"activate check failed: 0x{activate.error_code:08X}")
            if activate.version != self.config.version:
                raise RuntimeError(
                    "activate check version mismatch: "
                    f"running {format_version(activate.version)}, "
                    f"expected {format_version(self.config.version)}"
                )

            self.logger("INFO", f"activate check ok, running version {format_version(activate.version)}")
            self.status("Completed")
            self.progress(100)
        finally:
            client.close()

    def _connect_with_retry(self, client: SmotaTcpClient | SmotaSerialClient, timeout_s: float) -> None:
        deadline = time.time() + timeout_s
        last_error: Exception | None = None

        while time.time() < deadline:
            self._ensure_not_stopped()
            try:
                client.connect()
                return
            except Exception as exc:
                last_error = exc
                time.sleep(0.2)

        if last_error is None:
            raise TimeoutError("connect timeout")
        raise TimeoutError(f"connect timeout: {last_error}") from last_error

    def _query_running_version(self, client: SmotaTcpClient | SmotaSerialClient) -> tuple[int, int, int]:
        try:
            frame = client.exchange(
                CMD_QUERY_VERSION,
                CMD_QUERY_VERSION_RESP,
                QUERY_VERSION_REQ.pack(0),
            )
            response = decode_query_version(frame.payload)
            if response.error_code != 0:
                raise RuntimeError(f"query version failed: 0x{response.error_code:08X}")
            return response.version
        except Exception as exc:
            self.logger("INFO", f"query version 0x07 unavailable, fallback to activate check: {exc}")

        frame = client.exchange(
            CMD_ACTIVATE_CHECK,
            CMD_ACTIVATE_CHECK_RESP,
            ACTIVATE_CHECK_REQ.pack(0),
        )
        response = decode_activate_check(frame.payload)
        if response.error_code != 0:
            raise RuntimeError(f"activate check query failed: 0x{response.error_code:08X}")
        return response.version

    def _ensure_not_stopped(self) -> None:
        if self._stop_event.is_set():
            raise RuntimeError("upgrade canceled")
