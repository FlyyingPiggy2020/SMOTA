from __future__ import annotations

import hashlib
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from .protocol import (
    CMD_DATA,
    CMD_DATA_RESP,
    CMD_FINISH,
    CMD_FINISH_RESP,
    CMD_QUERY,
    CMD_QUERY_RESP,
    CMD_START,
    CMD_START_RESP,
    DATA_REQ,
    DATA_RESP,
    FINISH_REQ,
    FINISH_RESP,
    QUERY_FLAG_FORCE_UPGRADE,
    QUERY_REQ,
    START_FLAG_SHA256_VALID,
    START_REQ,
    SmotaSerialClient,
    SmotaTcpClient,
    decode_query,
    decode_start,
    format_version,
    read_ota_package,
)


LogFn = Callable[[str, str], None]
ProgressFn = Callable[[int], None]
StatusFn = Callable[[str], None]
DeviceInfoFn = Callable[[tuple[int, int, int], str], None]
BOOT_CAPTURE_PROBE_TIMEOUT_S = 0.08
BOOT_CAPTURE_PROBE_DELAY_S = 0.02


@dataclass(frozen=True)
class UpgradeConfig:
    firmware_path: Path
    version: tuple[int, int, int]
    transport: str
    host: str
    port: int
    serial_port: str
    serial_baudrate: int
    timeout_s: float
    connect_timeout_s: float
    chunk_size: int
    block_timeout_ms: int
    install_timeout_ms: int
    force_install: bool
    activate_check: bool


class UpgradeSession:
    def __init__(
        self,
        config: UpgradeConfig,
        logger: LogFn,
        progress: ProgressFn,
        status: StatusFn,
        device_info: DeviceInfoFn | None = None,
        serial_connection: Any | None = None,
        serial_connection_owned: bool = False,
    ) -> None:
        self.config = config
        self.logger = logger
        self.progress = progress
        self.status = status
        self.device_info = device_info
        self.serial_connection = serial_connection
        self.serial_connection_owned = serial_connection_owned
        self._stop_event = threading.Event()

    def request_stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        package = read_ota_package(self.config.firmware_path)
        target_version = package.manifest.version
        firmware = package.firmware
        project_id = package.manifest.project_id.encode("utf-8")[:16].ljust(16, b"\x00")
        if self.config.transport == "serial":
            client = SmotaSerialClient(
                port=self.config.serial_port,
                baudrate=self.config.serial_baudrate,
                timeout=self.config.timeout_s,
                logger=self.logger,
                serial_connection=self.serial_connection,
                close_on_close=self.serial_connection is None or self.serial_connection_owned,
            )
            self.logger("INFO", f"传输方式=串口 串口={self.config.serial_port} 波特率={self.config.serial_baudrate}")
        else:
            client = SmotaTcpClient(
                host=self.config.host,
                port=self.config.port,
                timeout=self.config.timeout_s,
                logger=self.logger,
            )
            self.logger("INFO", f"传输方式=TCP 地址={self.config.host}:{self.config.port}")

        self.logger("INFO", f"固件版本={format_version(target_version)}")
        self.logger("INFO", f"固件ID={package.manifest.project_id}")
        self.logger("INFO", f"OTA文件={package.path}")
        self.logger("INFO", f"容器固件={package.manifest.firmware_name}")
        self.progress(0)

        try:
            self.status("捕获Boot")
            query_info = self._query_running_version_with_probe(
                client,
                self.config.connect_timeout_s,
                target_version,
                project_id,
            )
            self.logger("INFO", f"当前运行版本={format_version(query_info.version)}")
            self.logger("INFO", f"当前设备ID={query_info.project_id}")
            if self.device_info is not None:
                self.device_info(query_info.version, query_info.project_id)

            if query_info.error_code != 0 or not query_info.allow_upgrade:
                raise RuntimeError(f"设备拒绝升级：0x{query_info.error_code:08X}")
            self.logger("INFO", "设备允许升级，继续执行升级流程")

            firmware_hash = hashlib.sha256(firmware).digest()
            self.logger("INFO", f"固件文件={self.config.firmware_path}")
            self.logger("INFO", f"固件大小={len(firmware)} 字节")
            self.logger("INFO", f"sha256={firmware_hash.hex()}")

            self.status("开始升级")
            start = self._start_transfer(
                client=client,
                firmware_size=len(firmware),
                firmware_hash=firmware_hash,
            )

            self.logger(
                "INFO",
                "设备已准备接收 "
                f"最大payload={start.max_payload_size}",
            )

            self.status("传输中")
            payload_size = max(1, min(self.config.chunk_size, start.max_payload_size - DATA_REQ.size))
            offset = 0
            while offset < len(firmware):
                self._ensure_not_stopped()
                chunk = firmware[offset:offset + payload_size]
                frame = client.exchange(
                    CMD_DATA,
                    CMD_DATA_RESP,
                    DATA_REQ.pack(offset, len(chunk)) + chunk,
                )
                block_error, received_offset = DATA_RESP.unpack(frame.payload)
                if block_error != 0:
                    raise RuntimeError(f"数据块发送失败，偏移 {offset}：0x{block_error:08X}")
                if received_offset != offset + len(chunk):
                    raise RuntimeError(
                        f"设备确认偏移为 {received_offset}，期望值为 {offset + len(chunk)}"
                    )
                offset = received_offset
                self.progress(int(offset * 100 / len(firmware)))
                self.logger("INFO", f"进度 {offset}/{len(firmware)} 字节")

            self.status("校验中")
            complete_frame = client.exchange(
                CMD_FINISH,
                CMD_FINISH_RESP,
                FINISH_REQ.pack(len(firmware)),
            )
            complete_error, reset_delay_ms = FINISH_RESP.unpack(complete_frame.payload)
            if complete_error != 0:
                raise RuntimeError(f"传输校验失败：0x{complete_error:08X}")
            self.logger("INFO", f"传输校验成功，设备将在 {reset_delay_ms} ms 后复位")
            client.close()

            if not self.config.activate_check:
                self.status("已完成")
                self.progress(100)
                return

            self.status("等待重连")
            self.logger(
                "INFO",
                "设备即将断开；请重启 win_sim，并保持当前窗口开启以完成激活校验",
            )

            self.status("激活校验")
            activate = self._query_running_version_with_probe(
                client,
                self.config.install_timeout_ms / 1000.0 + 5.0,
                target_version,
                project_id,
            )
            if self.device_info is not None:
                self.device_info(activate.version, activate.project_id)
            if activate.version != target_version:
                raise RuntimeError(
                    "激活校验版本不一致："
                    f"当前运行 {format_version(activate.version)}，"
                    f"期望版本 {format_version(target_version)}"
                )

            self.logger("INFO", f"激活校验成功，当前运行版本 {format_version(activate.version)}")
            self.status("已完成")
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
                self.logger("INFO", f"连接重试中: {exc}")
                client.close()
                time.sleep(0.2)

        if last_error is None:
            raise TimeoutError("连接超时")
        raise TimeoutError(f"连接超时：{last_error}") from last_error

    def _query_running_version_with_probe(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        timeout_s: float,
        target_version: tuple[int, int, int],
        project_id: bytes,
    ):
        deadline = time.time() + timeout_s
        last_error: Exception | None = None
        original_timeout = client.timeout
        probe_timeout = min(original_timeout, BOOT_CAPTURE_PROBE_TIMEOUT_S)
        connected = False

        self.logger(
            "INFO",
            "等待连接并持续发送 QUERY 捕获 Boot，"
            f"最长 {timeout_s:.1f} 秒",
        )
        try:
            while time.time() < deadline:
                self._ensure_not_stopped()
                if not connected:
                    client.set_timeout(original_timeout)
                    try:
                        client.connect()
                        connected = True
                        client.set_timeout(probe_timeout)
                    except Exception as exc:
                        last_error = exc
                        self.logger("INFO", f"连接重试中: {exc}")
                        client.close()
                        time.sleep(0.2)
                        continue

                try:
                    return self._query_running_version(client, target_version, project_id, verbose=False)
                except Exception as exc:
                    last_error = exc
                    if not isinstance(exc, TimeoutError):
                        self.logger("INFO", f"捕获过程中连接异常，重新连接: {exc}")
                        client.close()
                        connected = False
                    time.sleep(BOOT_CAPTURE_PROBE_DELAY_S)
        finally:
            client.set_timeout(original_timeout)

        if last_error is None:
            raise TimeoutError("读取版本超时")
        raise TimeoutError(f"读取版本超时：{last_error}") from last_error

    def _query_running_version(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        target_version: tuple[int, int, int],
        project_id: bytes,
        verbose: bool = True,
    ):
        if verbose:
            self.logger("INFO", "准备查询设备当前运行版本和升级许可")
        frame = client.exchange(
            CMD_QUERY,
            CMD_QUERY_RESP,
            QUERY_REQ.pack(
                target_version[0],
                target_version[1],
                target_version[2],
                QUERY_FLAG_FORCE_UPGRADE if self.config.force_install else 0,
                project_id,
            ),
        )
        response = decode_query(frame.payload)
        return response

    def _start_transfer(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        firmware_size: int,
        firmware_hash: bytes,
    ):
        start = self._send_start(client, firmware_size, firmware_hash)
        if start.error_code == 0:
            return start

        raise RuntimeError(f"START 失败：0x{start.error_code:08X}")

    def _send_start(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        firmware_size: int,
        firmware_hash: bytes,
    ):
        start_frame = client.exchange(
            CMD_START,
            CMD_START_RESP,
            START_REQ.pack(
                START_FLAG_SHA256_VALID,
                firmware_size,
                firmware_hash,
                self.config.block_timeout_ms,
            ),
        )
        return decode_start(start_frame.payload)

    def _ensure_not_stopped(self) -> None:
        if self._stop_event.is_set():
            raise RuntimeError("升级已取消")
