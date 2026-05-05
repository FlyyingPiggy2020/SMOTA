from __future__ import annotations

import queue
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from .protocol import SerialPortInfo, format_version, list_serial_ports, parse_version, read_ota_package
from .session import UpgradeConfig, UpgradeSession

try:
    import serial
except ImportError:  # pragma: no cover - optional dependency
    serial = None


class HostApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("smOTA 上位机")
        self.root.geometry("980x720")

        self.firmware_path = tk.StringVar(value="")
        self.version = tk.StringVar(value="")
        self.project_id = tk.StringVar(value="")
        self.device_version = tk.StringVar(value="")
        self.device_project_id = tk.StringVar(value="")
        self.transport = tk.StringVar(value="tcp")
        self.host = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="8888")
        self.serial_port = tk.StringVar(value="")
        self.serial_baudrate = tk.StringVar(value="115200")
        self.serial_status = tk.StringVar(value="串口未打开")
        self.chunk_size = tk.StringVar(value="128")
        self.timeout_s = tk.StringVar(value="3.0")
        self.connect_timeout_s = tk.StringVar(value="8.0")
        self.block_timeout_ms = tk.StringVar(value="5000")
        self.install_timeout_ms = tk.StringVar(value="15000")
        self.force_install = tk.BooleanVar(value=False)
        self.activate_check = tk.BooleanVar(value=True)
        self.status = tk.StringVar(value="空闲")

        self.progress = tk.IntVar(value=0)
        self.event_queue: queue.Queue[tuple[str, str, object]] = queue.Queue()
        self.worker: threading.Thread | None = None
        self.session: UpgradeSession | None = None
        self.serial_monitor = None
        self.serial_monitor_thread: threading.Thread | None = None
        self.serial_monitor_stop = threading.Event()
        self.serial_monitor_requested = False
        self.serial_monitor_port = ""
        self.serial_monitor_baudrate = 0
        self.serial_ports: list[SerialPortInfo] = []
        self.serial_port_labels: dict[str, str] = {}

        self._load_serial_ports()
        if self.serial_ports:
            self.serial_port.set(self.serial_ports[0].device)
            self.transport.set("serial")

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.after(100, self._process_events)

    def _build_ui(self) -> None:
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(3, weight=1)

        firmware_frame = ttk.LabelFrame(self.root, text="固件")
        firmware_frame.grid(row=0, column=0, sticky="ew", padx=12, pady=(12, 6))
        firmware_frame.columnconfigure(1, weight=1)

        ttk.Label(firmware_frame, text="文件").grid(row=0, column=0, padx=8, pady=8, sticky="w")
        ttk.Entry(firmware_frame, textvariable=self.firmware_path).grid(
            row=0, column=1, padx=8, pady=8, sticky="ew"
        )
        ttk.Button(firmware_frame, text="浏览", command=self._browse_firmware).grid(
            row=0, column=2, padx=8, pady=8
        )

        config_frame = ttk.LabelFrame(self.root, text="连接与升级")
        config_frame.grid(row=1, column=0, sticky="ew", padx=12, pady=6)
        config_frame.columnconfigure(0, weight=1)

        common_frame = ttk.Frame(config_frame)
        common_frame.grid(row=0, column=0, sticky="ew", padx=8, pady=8)
        for index in range(6):
            common_frame.columnconfigure(index, weight=1)

        self._add_entry(common_frame, "固件版本", self.version, 0, 0, state="readonly")
        self._add_entry(common_frame, "固件ID", self.project_id, 0, 2, state="readonly")
        self._add_combo(common_frame, "传输方式", self.transport, ["tcp", "serial"], 0, 4)
        self._add_entry(common_frame, "当前设备版本", self.device_version, 1, 0, state="readonly")
        self._add_entry(common_frame, "当前设备ID", self.device_project_id, 1, 2, state="readonly")
        self._add_entry(common_frame, "分块大小", self.chunk_size, 1, 4)
        self._add_entry(common_frame, "通信超时(s)", self.timeout_s, 2, 0)
        self._add_entry(common_frame, "连接/捕获(s)", self.connect_timeout_s, 2, 2)
        self._add_entry(common_frame, "分块超时(ms)", self.block_timeout_ms, 2, 4)
        self._add_entry(common_frame, "重连超时(ms)", self.install_timeout_ms, 3, 0)

        ttk.Checkbutton(common_frame, text="强制升级", variable=self.force_install).grid(
            row=4, column=0, padx=8, pady=8, sticky="w"
        )
        ttk.Checkbutton(common_frame, text="激活校验", variable=self.activate_check).grid(
            row=4, column=2, padx=8, pady=8, sticky="w"
        )

        self.transport_frame = ttk.Frame(config_frame)
        self.transport_frame.grid(row=1, column=0, sticky="ew", padx=8, pady=(0, 8))
        self.transport_frame.columnconfigure(0, weight=1)

        self.tcp_frame = ttk.LabelFrame(self.transport_frame, text="TCP 配置")
        for index in range(4):
            self.tcp_frame.columnconfigure(index, weight=1)
        self._add_entry(self.tcp_frame, "主机地址", self.host, 0, 0)
        self._add_entry(self.tcp_frame, "端口", self.port, 0, 2)

        self.serial_frame = ttk.LabelFrame(self.transport_frame, text="串口配置")
        for index in range(6):
            self.serial_frame.columnconfigure(index, weight=1)
        self._add_serial_combo(self.serial_frame, "串口", 0, 0)
        self._add_entry(self.serial_frame, "串口波特率", self.serial_baudrate, 0, 2)
        ttk.Button(self.serial_frame, text="刷新串口", command=self._refresh_serial_ports).grid(
            row=0, column=4, padx=8, pady=8, sticky="ew"
        )
        ttk.Label(self.serial_frame, text="串口状态").grid(row=1, column=0, padx=8, pady=8, sticky="w")
        ttk.Label(self.serial_frame, textvariable=self.serial_status).grid(
            row=1, column=1, columnspan=2, padx=8, pady=8, sticky="w"
        )
        self.serial_open_button = ttk.Button(self.serial_frame, text="打开串口", command=self._open_serial_monitor)
        self.serial_open_button.grid(row=1, column=3, padx=8, pady=8, sticky="ew")
        self.serial_close_button = ttk.Button(
            self.serial_frame,
            text="关闭串口",
            command=self._close_serial_monitor,
            state="disabled",
        )
        self.serial_close_button.grid(row=1, column=4, padx=8, pady=8, sticky="ew")

        self.transport.trace_add("write", self._on_transport_changed)
        self._update_transport_frame()

        action_frame = ttk.Frame(self.root)
        action_frame.grid(row=2, column=0, sticky="ew", padx=12, pady=6)
        action_frame.columnconfigure(3, weight=1)

        self.start_button = ttk.Button(
            action_frame,
            text="连接并升级",
            command=self._start_upgrade,
            state="disabled",
        )
        self.start_button.grid(row=0, column=0, padx=(0, 8), pady=4)
        self.stop_button = ttk.Button(action_frame, text="停止", command=self._stop_upgrade, state="disabled")
        self.stop_button.grid(row=0, column=1, padx=8, pady=4)
        ttk.Button(action_frame, text="清空日志", command=self._clear_log).grid(row=0, column=2, padx=8, pady=4)
        ttk.Label(action_frame, textvariable=self.status).grid(row=0, column=3, sticky="e")
        self._update_start_button()

        progress_frame = ttk.Frame(self.root)
        progress_frame.grid(row=3, column=0, sticky="nsew", padx=12, pady=(0, 12))
        progress_frame.columnconfigure(0, weight=1)
        progress_frame.rowconfigure(1, weight=1)

        ttk.Progressbar(progress_frame, maximum=100, variable=self.progress).grid(
            row=0, column=0, sticky="ew", pady=(0, 8)
        )

        notebook = ttk.Notebook(progress_frame)
        notebook.grid(row=1, column=0, sticky="nsew")

        log_tab = ttk.Frame(notebook)
        log_tab.columnconfigure(0, weight=1)
        log_tab.rowconfigure(0, weight=1)
        notebook.add(log_tab, text="日志")

        raw_tab = ttk.Frame(notebook)
        raw_tab.columnconfigure(0, weight=1)
        raw_tab.rowconfigure(0, weight=1)
        notebook.add(raw_tab, text="原始报文")

        serial_log_tab = ttk.Frame(notebook)
        serial_log_tab.columnconfigure(0, weight=1)
        serial_log_tab.rowconfigure(0, weight=1)
        notebook.add(serial_log_tab, text="串口报文")

        self.log_text = tk.Text(log_tab, wrap="word", font=("Consolas", 10))
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scrollbar = ttk.Scrollbar(log_tab, orient="vertical", command=self.log_text.yview)
        log_scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=log_scrollbar.set)

        self.raw_text = tk.Text(raw_tab, wrap="none", font=("Consolas", 10))
        self.raw_text.grid(row=0, column=0, sticky="nsew")
        raw_scrollbar = ttk.Scrollbar(raw_tab, orient="vertical", command=self.raw_text.yview)
        raw_scrollbar.grid(row=0, column=1, sticky="ns")
        self.raw_text.configure(yscrollcommand=raw_scrollbar.set)

        self.serial_text = tk.Text(serial_log_tab, wrap="none", font=("Consolas", 10))
        self.serial_text.grid(row=0, column=0, sticky="nsew")
        serial_scrollbar = ttk.Scrollbar(serial_log_tab, orient="vertical", command=self.serial_text.yview)
        serial_scrollbar.grid(row=0, column=1, sticky="ns")
        self.serial_text.configure(yscrollcommand=serial_scrollbar.set)

        self.log_text.tag_configure("INFO", foreground="#243b53")
        self.log_text.tag_configure("ERROR", foreground="#b42318")
        self.raw_text.tag_configure("TX", foreground="#0b6e4f")
        self.raw_text.tag_configure("RX", foreground="#8a4b08")
        self.raw_text.tag_configure("ERROR", foreground="#b42318")
        self.serial_text.tag_configure("INFO", foreground="#243b53")
        self.serial_text.tag_configure("RX", foreground="#0b6e4f")
        self.serial_text.tag_configure("ERROR", foreground="#b42318")

    def _add_entry(
        self,
        parent: ttk.LabelFrame,
        label: str,
        variable: tk.StringVar,
        row: int,
        column: int,
        state: str = "normal",
    ) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        ttk.Entry(parent, textvariable=variable, state=state).grid(
            row=row, column=column + 1, padx=8, pady=8, sticky="ew"
        )

    def _add_combo(
        self,
        parent: ttk.LabelFrame,
        label: str,
        variable: tk.StringVar,
        values: list[str],
        row: int,
        column: int,
    ) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        ttk.Combobox(parent, textvariable=variable, values=values, state="readonly").grid(
            row=row, column=column + 1, padx=8, pady=8, sticky="ew"
        )

    def _add_serial_combo(self, parent: ttk.LabelFrame, label: str, row: int, column: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        self.serial_combo = ttk.Combobox(parent, state="readonly")
        self.serial_combo.grid(row=row, column=column + 1, padx=8, pady=8, sticky="ew")
        self.serial_combo.bind("<<ComboboxSelected>>", self._on_serial_selected)
        self._update_serial_combo_values()

    def _load_serial_ports(self) -> None:
        self.serial_ports = list_serial_ports()
        self.serial_port_labels = {port.device: port.label for port in self.serial_ports}

    def _update_serial_combo_values(self) -> None:
        if not hasattr(self, "serial_combo"):
            return

        labels = [port.label for port in self.serial_ports]
        self.serial_combo.configure(values=labels)

        current_device = self.serial_port.get().strip()
        if current_device in self.serial_port_labels:
            self.serial_combo.set(self.serial_port_labels[current_device])
        elif labels:
            self.serial_combo.set(labels[0])
            self.serial_port.set(self.serial_ports[0].device)
        else:
            self.serial_combo.set("")

    def _on_serial_selected(self, _event: object) -> None:
        selected_label = self.serial_combo.get().strip()
        for port in self.serial_ports:
            if port.label == selected_label:
                if self.serial_monitor_requested and port.device != self.serial_port.get().strip():
                    self._append_log("INFO", "切换串口前关闭当前串口")
                    self._close_serial_monitor()
                self.serial_port.set(port.device)
                return

    def _browse_firmware(self) -> None:
        filename = filedialog.askopenfilename(
            title="选择 OTA 文件",
            filetypes=[("OTA 文件", "*.ota")],
        )
        if filename:
            try:
                package = read_ota_package(filename)
            except Exception as exc:
                messagebox.showerror("OTA 文件无效", str(exc))
                return

            self.firmware_path.set(filename)
            self.version.set(format_version(package.manifest.version))
            self.project_id.set(package.manifest.project_id)
            self._append_log(
                "INFO",
                "已加载 OTA 文件 "
                f"{package.path.name}，"
                f"固件版本 {format_version(package.manifest.version)}，"
                f"固件ID {package.manifest.project_id}",
            )

    def _on_transport_changed(self, *_args: object) -> None:
        self._update_transport_frame()
        self._update_start_button()

    def _update_transport_frame(self) -> None:
        transport = self.transport.get().strip().lower() or "tcp"

        self.tcp_frame.grid_forget()
        self.serial_frame.grid_forget()

        if transport == "serial":
            self.serial_frame.grid(row=0, column=0, sticky="ew")
        else:
            self.tcp_frame.grid(row=0, column=0, sticky="ew")

        self._update_serial_monitor_buttons()

    def _start_upgrade(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            return

        try:
            transport = self.transport.get().strip().lower() or "tcp"
            if transport not in ("tcp", "serial"):
                raise ValueError("传输方式只能是 tcp 或 serial")

            config = UpgradeConfig(
                firmware_path=Path(self.firmware_path.get()).expanduser().resolve(),
                version=parse_version(self.version.get()),
                transport=transport,
                host=self.host.get().strip(),
                port=int(self.port.get()),
                serial_port=self.serial_port.get().strip(),
                serial_baudrate=int(self.serial_baudrate.get()),
                timeout_s=float(self.timeout_s.get()),
                connect_timeout_s=float(self.connect_timeout_s.get()),
                chunk_size=int(self.chunk_size.get()),
                block_timeout_ms=int(self.block_timeout_ms.get()),
                install_timeout_ms=int(self.install_timeout_ms.get()),
                force_install=self.force_install.get(),
                activate_check=self.activate_check.get(),
            )
        except Exception as exc:
            messagebox.showerror("输入无效", str(exc))
            return

        if config.transport == "serial" and not config.serial_port:
            messagebox.showerror("串口未填写", "串口号不能为空")
            return

        if not config.firmware_path.exists():
            messagebox.showerror("固件不存在", f"未找到文件：{config.firmware_path}")
            return

        if config.firmware_path.suffix.lower() != ".ota":
            messagebox.showerror("OTA 类型错误", "只允许选择 .ota OTA 文件")
            return

        try:
            package = read_ota_package(config.firmware_path)
        except Exception as exc:
            messagebox.showerror("OTA 文件无效", str(exc))
            return

        serial_connection = None
        if config.transport == "serial":
            if self.serial_monitor_requested:
                self._pause_serial_monitor_for_upgrade()
                serial_connection = self._detach_serial_monitor()

        self.version.set(format_version(package.manifest.version))
        self.project_id.set(package.manifest.project_id)
        self.device_version.set("")
        self.device_project_id.set("")

        self.progress.set(0)
        self._append_log("INFO", "开始连接、版本检查和升级流程")
        self.start_button.configure(state="disabled")
        self.stop_button.configure(state="normal")

        self.session = UpgradeSession(
            config=config,
            logger=lambda level, msg: self.event_queue.put(("log", level, msg)),
            progress=lambda value: self.event_queue.put(("progress", "", value)),
            status=lambda msg: self.event_queue.put(("status", "", msg)),
            device_info=lambda version, project_id: self.event_queue.put(
                ("device_info", "", (version, project_id))
            ),
            serial_connection=serial_connection,
            serial_connection_owned=serial_connection is not None,
        )
        self.worker = threading.Thread(target=self._run_worker, daemon=True)
        self.worker.start()
        self._update_serial_monitor_buttons()

    def _refresh_serial_ports(self) -> None:
        current_device = self.serial_port.get().strip()
        self._load_serial_ports()
        self._update_serial_combo_values()

        if not self.serial_ports:
            messagebox.showwarning("串口列表", "未找到可用串口")
            return

        if current_device and current_device in self.serial_port_labels:
            self.serial_port.set(current_device)
            self.serial_combo.set(self.serial_port_labels[current_device])
        elif self.serial_ports:
            self.serial_port.set(self.serial_ports[0].device)
            self.serial_combo.set(self.serial_ports[0].label)

        messagebox.showinfo("串口列表", "\n".join(port.label for port in self.serial_ports))

    def _open_serial_monitor(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            messagebox.showwarning("串口忙", "升级过程中不能手动打开串口")
            return

        if self.serial_monitor_requested:
            return

        if serial is None:
            messagebox.showerror("缺少依赖", "pyserial 未安装，请先执行：pip install pyserial")
            return

        port = self.serial_port.get().strip()
        if not port:
            messagebox.showerror("串口未填写", "串口号不能为空")
            return

        try:
            baudrate = int(self.serial_baudrate.get())
        except ValueError:
            messagebox.showerror("输入无效", "串口波特率必须是整数")
            return

        self.serial_monitor_requested = True
        self.serial_monitor_port = port
        self.serial_monitor_baudrate = baudrate
        self.serial_monitor_stop.clear()
        self.serial_status.set(f"正在打开 {port}@{baudrate}")
        self._append_log("INFO", f"保持打开串口 {port}@{baudrate}")
        self._append_serial_log("INFO", f"保持打开串口 {port}@{baudrate}")

        self._start_serial_monitor_thread()
        self._update_serial_monitor_buttons()

    def _close_serial_monitor(self) -> None:
        if not self.serial_monitor_requested and self.serial_monitor is None:
            self.serial_status.set("串口未打开")
            self._update_serial_monitor_buttons()
            return

        self.serial_monitor_requested = False
        self.serial_monitor_stop.set()

        monitor = self._detach_serial_monitor()

        try:
            if monitor is not None:
                monitor.close()
        except Exception as exc:
            self._append_serial_log("ERROR", f"关闭串口失败: {exc}")

        if self.serial_monitor_thread is not None:
            self.serial_monitor_thread.join(timeout=0.2)
            self.serial_monitor_thread = None

        self.serial_monitor_port = ""
        self.serial_monitor_baudrate = 0
        self.serial_status.set("串口未打开")
        self._append_log("INFO", "手动关闭串口")
        self._append_serial_log("INFO", "关闭串口")
        self._update_serial_monitor_buttons()

    def _pause_serial_monitor_for_upgrade(self) -> None:
        if self.serial_monitor is None:
            self.serial_status.set(f"升级中，等待串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate}")
        else:
            self.serial_status.set("升级中，串口已打开")

        if self.serial_monitor_thread is not None:
            monitor_thread = self.serial_monitor_thread
            self.serial_monitor_stop.set()
            monitor_thread.join(timeout=1.0)
            if monitor_thread.is_alive():
                self._append_serial_log("ERROR", "串口监视线程未及时停止，等待其自行退出")
            else:
                self.serial_monitor_thread = None
                self.serial_monitor_stop.clear()

        self._append_serial_log("INFO", "暂停串口监视，交给 OTA 升级")

    def _resume_serial_monitor_after_upgrade(self) -> None:
        if not self.serial_monitor_requested:
            return

        if self.serial_monitor is None:
            self.serial_status.set(f"等待串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate} 重连")
        else:
            self.serial_status.set(f"已打开 {self.serial_monitor_port}@{self.serial_monitor_baudrate}")

        self._append_serial_log("INFO", "OTA 结束，恢复串口监视")
        self._start_serial_monitor_thread()

    def _start_serial_monitor_thread(self) -> None:
        if not self.serial_monitor_requested:
            return

        if self.serial_monitor_thread is not None and self.serial_monitor_thread.is_alive():
            return

        self.serial_monitor_stop.clear()
        self.serial_monitor_thread = threading.Thread(target=self._serial_monitor_loop, daemon=True)
        self.serial_monitor_thread.start()

    def _serial_monitor_loop(self) -> None:
        last_open_error = ""

        while self.serial_monitor_requested and not self.serial_monitor_stop.is_set():
            if self.serial_monitor is None:
                new_monitor = None
                try:
                    new_monitor = serial.Serial(
                        port=self.serial_monitor_port,
                        baudrate=self.serial_monitor_baudrate,
                        timeout=0.05,
                        write_timeout=0.5,
                    )
                except Exception as exc:
                    message = str(exc)
                    if message != last_open_error:
                        last_open_error = message
                        self.event_queue.put((
                            "serial_waiting",
                            "INFO",
                            f"等待串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate} 重连: {exc}",
                        ))
                    self._sleep_before_serial_reconnect()
                    continue

                if not self.serial_monitor_requested or self.serial_monitor_stop.is_set():
                    try:
                        new_monitor.close()
                    except Exception:
                        pass
                    break

                self.serial_monitor = new_monitor
                last_open_error = ""
                self.event_queue.put((
                    "serial_connected",
                    "INFO",
                    f"已打开串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate}",
                ))

            monitor = self.serial_monitor
            if monitor is None:
                break

            try:
                waiting = int(getattr(monitor, "in_waiting", 0))
                data = monitor.read(waiting if waiting > 0 else 1)
            except Exception as exc:
                if not self.serial_monitor_stop.is_set():
                    self._close_detached_serial_monitor()
                    self.event_queue.put(("serial_disconnected", "ERROR", f"串口断开，等待重连: {exc}"))
                    self._sleep_before_serial_reconnect()
                    continue
                break

            if data:
                self.event_queue.put(("serial_log", "RX", self._format_serial_bytes(data)))

    def _sleep_before_serial_reconnect(self) -> None:
        for _ in range(10):
            if not self.serial_monitor_requested or self.serial_monitor_stop.is_set():
                return
            time.sleep(0.05)

    def _detach_serial_monitor(self):
        monitor = self.serial_monitor
        self.serial_monitor = None
        return monitor

    def _close_detached_serial_monitor(self) -> None:
        monitor = self._detach_serial_monitor()
        if monitor is None:
            return

        try:
            monitor.close()
        except Exception:
            pass

    def _update_serial_monitor_buttons(self) -> None:
        if not hasattr(self, "serial_open_button"):
            return

        worker_running = self.worker is not None and self.worker.is_alive()
        serial_visible = self.transport.get().strip().lower() == "serial"

        self.serial_open_button.configure(
            state="normal" if serial_visible and not worker_running and not self.serial_monitor_requested else "disabled"
        )
        self.serial_close_button.configure(
            state="normal" if serial_visible and self.serial_monitor_requested and not worker_running else "disabled"
        )
        self._update_start_button()

    def _update_start_button(self) -> None:
        if not hasattr(self, "start_button"):
            return

        worker_running = self.worker is not None and self.worker.is_alive()
        transport = self.transport.get().strip().lower() or "tcp"
        self.start_button.configure(state="disabled" if worker_running else "normal")

    def _run_worker(self) -> None:
        assert self.session is not None
        try:
            self.session.run()
            self.event_queue.put(("done", "INFO", "升级完成"))
        except Exception as exc:
            self.event_queue.put(("done", "ERROR", str(exc)))

    def _stop_upgrade(self) -> None:
        if self.session is not None:
            self.session.request_stop()
            self._append_log("INFO", "已请求停止升级")

    def _clear_log(self) -> None:
        self.log_text.delete("1.0", tk.END)
        self.raw_text.delete("1.0", tk.END)
        self.serial_text.delete("1.0", tk.END)

    def _process_events(self) -> None:
        while True:
            try:
                event, level, payload = self.event_queue.get_nowait()
            except queue.Empty:
                break

            if event == "log":
                self._append_log(level, str(payload))
            elif event == "progress":
                self.progress.set(int(payload))
            elif event == "status":
                self.status.set(str(payload))
            elif event == "serial_log":
                self._append_serial_log(level, str(payload))
            elif event == "serial_waiting":
                self.serial_status.set(f"等待串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate} 重连")
                self._append_serial_log(level, str(payload))
                self._update_serial_monitor_buttons()
            elif event == "serial_connected":
                self.serial_status.set(f"已打开 {self.serial_monitor_port}@{self.serial_monitor_baudrate}")
                self._append_log(level, str(payload))
                self._append_serial_log(level, str(payload))
                self._update_serial_monitor_buttons()
            elif event == "serial_disconnected":
                self.serial_status.set(f"等待串口 {self.serial_monitor_port}@{self.serial_monitor_baudrate} 重连")
                self._append_log(level, str(payload))
                self._append_serial_log(level, str(payload))
                self._update_serial_monitor_buttons()
            elif event == "device_info":
                version, project_id = payload
                self.device_version.set(format_version(version))
                self.device_project_id.set(str(project_id))
            elif event == "done":
                self._append_log(level, str(payload))
                self.status.set("空闲" if level == "ERROR" else "已完成")
                self.stop_button.configure(state="disabled")
                self._resume_serial_monitor_after_upgrade()
                self._update_serial_monitor_buttons()

        self.root.after(100, self._process_events)

    def _append_log(self, level: str, message: str) -> None:
        if level in ("TX", "RX"):
            self.raw_text.insert(tk.END, f"[{level}] {message}\n", level)
            self.raw_text.see(tk.END)
            if self.transport.get().strip().lower() == "serial":
                self._append_serial_log(level, f"协议帧 {message}")
            return

        self.log_text.insert(tk.END, f"[{level}] {message}\n", level)
        self.log_text.see(tk.END)

    def _append_serial_log(self, level: str, message: str) -> None:
        self.serial_text.insert(tk.END, f"[{level}] {message}\n", level)
        self.serial_text.see(tk.END)

    @staticmethod
    def _format_serial_bytes(data: bytes) -> str:
        hex_text = " ".join(f"{byte:02X}" for byte in data)
        text = data.decode("utf-8", errors="replace")
        text = text.replace("\\", "\\\\").replace("\r", "\\r").replace("\n", "\\n")
        return f"len={len(data)} hex={hex_text} text=\"{text}\""

    def _on_close(self) -> None:
        if self.session is not None:
            self.session.request_stop()
        self._close_serial_monitor()
        self.root.destroy()


def main() -> int:
    root = tk.Tk()
    style = ttk.Style(root)
    if "vista" in style.theme_names():
        style.theme_use("vista")
    HostApp(root)
    root.mainloop()
    return 0
