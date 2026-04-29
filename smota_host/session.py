from __future__ import annotations

import hashlib
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from .protocol import (
    QUERY_VERSION_REQ,
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
    decode_handshake,
    decode_query_version,
    format_version,
    read_ota_package,
)


LogFn = Callable[[str, str], None]
ProgressFn = Callable[[int], None]
StatusFn = Callable[[str], None]
DeviceInfoFn = Callable[[tuple[int, int, int], str], None]
BOOT_PROJECT_ID = "SMOTA_BOOT"
EMPTY_BOOT_VERSION = (0, 0, 0)
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
            running_info = self._query_running_version_with_probe(
                client,
                self.config.connect_timeout_s,
            )
            running_version = running_info.version
            self.logger("INFO", f"当前运行版本={format_version(running_version)}")
            self.logger("INFO", f"当前设备ID={running_info.project_id}")
            if self.device_info is not None:
                self.device_info(running_info.version, running_info.project_id)

            compare_result = compare_version(running_version, target_version)
            if not self.config.force_install:
                if compare_result == 0:
                    self.logger("INFO", "当前版本与目标版本一致，跳过下载")
                    self.status("已跳过")
                    self.progress(100)
                    return
                if compare_result > 0:
                    raise RuntimeError(
                        "当前运行版本高于目标版本："
                        f"{format_version(running_version)} > {format_version(target_version)}; "
                        "如需继续请启用强制安装"
                    )
            else:
                self.logger("INFO", "已启用强制安装，继续执行升级流程")

            firmware_hash = hashlib.sha256(firmware).digest()
            self.logger("INFO", f"固件文件={self.config.firmware_path}")
            self.logger("INFO", f"固件大小={len(firmware)} 字节")
            self.logger("INFO", f"sha256={firmware_hash.hex()}")

            self.status("握手中")
            handshake = self._handshake(
                client=client,
                target_version=target_version,
                firmware_size=len(firmware),
                project_id=project_id,
                running_version=running_version,
            )

            self.logger(
                "INFO",
                "握手成功 "
                f"最大包长={handshake.max_packet_size} "
                f"可用 Flash={handshake.flash_free_size} "
                f"起始偏移={handshake.next_offset}",
            )

            self.status("发送头信息")
            header_frame = client.exchange(
                CMD_HEADER_INFO,
                CMD_HEADER_INFO_RESP,
                HEADER_INFO_REQ.pack(firmware_hash, bytes(32), bytes(32)),
            )
            header_error = int.from_bytes(header_frame.payload[:4], "little")
            if header_error != 0:
                raise RuntimeError(f"头信息发送失败：0x{header_error:08X}")

            self.status("传输中")
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
                CMD_DATA_COMPLETE,
                CMD_DATA_COMPLETE_RESP,
                TRANSFER_COMPLETE_REQ.pack(len(firmware)),
            )
            complete_error = TRANSFER_COMPLETE_RESP.unpack(complete_frame.payload)[0]
            if complete_error != 0:
                raise RuntimeError(f"传输校验失败：0x{complete_error:08X}")
            self.logger("INFO", "传输校验成功")

            self.status("安装中")
            install_frame = client.exchange(
                CMD_INSTALL,
                CMD_INSTALL_RESP,
                INSTALL_REQ.pack(1 if self.config.force_install else 0, bytes(15)),
            )
            install_error, estimated_time_s = INSTALL_RESP.unpack(install_frame.payload)
            if install_error != 0:
                raise RuntimeError(f"安装请求失败：0x{install_error:08X}")
            self.logger("INFO", f"设备已接受安装，预计重启时间 {estimated_time_s} 秒")
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

    def _query_running_version_with_probe(self, client: SmotaTcpClient | SmotaSerialClient, timeout_s: float):
        deadline = time.time() + timeout_s
        last_error: Exception | None = None
        original_timeout = client.timeout
        probe_timeout = min(original_timeout, BOOT_CAPTURE_PROBE_TIMEOUT_S)
        connected = False

        self.logger(
            "INFO",
            "等待连接并持续发送 QUERY_VERSION 捕获 Boot，"
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
                    return self._query_running_version(client, verbose=False)
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

    def _query_running_version(self, client: SmotaTcpClient | SmotaSerialClient, verbose: bool = True):
        if verbose:
            self.logger("INFO", "准备查询设备当前运行版本")
        frame = client.exchange(
            CMD_QUERY_VERSION,
            CMD_QUERY_VERSION_RESP,
            QUERY_VERSION_REQ.pack(0),
        )
        response = decode_query_version(frame.payload)
        if response.error_code != 0:
            raise RuntimeError(f"读取版本失败：0x{response.error_code:08X}")
        return response

    def _handshake(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        target_version: tuple[int, int, int],
        firmware_size: int,
        project_id: bytes,
        running_version: tuple[int, int, int],
    ):
        if running_version == EMPTY_BOOT_VERSION:
            self.logger("INFO", "检测到空 Boot，使用 SMOTA_BOOT 兼容握手")
            boot_project_id = BOOT_PROJECT_ID.encode("utf-8")[:16].ljust(16, b"\x00")
            handshake = self._send_handshake(client, target_version, firmware_size, boot_project_id)
            if handshake.error_code != 0:
                raise RuntimeError(f"握手失败：0x{handshake.error_code:08X}")
            return handshake

        handshake = self._send_handshake(client, target_version, firmware_size, project_id)
        if handshake.error_code == 0:
            return handshake

        raise RuntimeError(f"握手失败：0x{handshake.error_code:08X}")

    def _send_handshake(
        self,
        client: SmotaTcpClient | SmotaSerialClient,
        target_version: tuple[int, int, int],
        firmware_size: int,
        project_id: bytes,
    ):
        handshake_frame = client.exchange(
            CMD_HANDSHAKE,
            CMD_HANDSHAKE_RESP,
            HANDSHAKE_REQ.pack(
                target_version[0],
                target_version[1],
                target_version[2],
                firmware_size,
                project_id,
                self.config.block_timeout_ms,
                self.config.check_timeout_ms,
                self.config.install_timeout_ms,
                self.config.total_timeout_ms,
            ),
        )
        return decode_handshake(handshake_frame.payload)

    def _ensure_not_stopped(self) -> None:
        if self._stop_event.is_set():
            raise RuntimeError("升级已取消")
