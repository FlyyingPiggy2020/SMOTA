# smOTA 上位机

这个目录是 Python 图形上位机实现，基于 `tkinter`，用于通过 `TCP` 或 `Serial(USB CDC)` 执行 smOTA 升级流程。

## 当前能力

- 选择并校验 `.ota` 文件
- 支持 `.bin` 和 `.hex` 固件
- 支持 `TCP` / `串口` 两种传输方式
- 支持手动打开/关闭串口观察原始字节流
- 自动从 `.ota` 的 `manifest.json` 读取目标版本和项目 ID
- 通过 `QUERY` 让设备端裁决是否允许升级
- 支持强制升级，通过 `QUERY` 请求 flag 交给设备端处理
- 支持升级后重连并做激活校验
- 显示升级进度、运行日志和 TX/RX 协议帧摘要

## 启动

在仓库根目录执行：

```powershell
pip install pyserial
python -m smota_host
```

## OTA 文件规则

当前只允许选择 `.ota` 文件。`.ota` 本质是 ZIP 容器，至少包含：

- `manifest.json`
- 一个固件文件（`.bin` 或 `.hex`）

`manifest.json` 至少包含：

- `format_version`
- `firmware_name`
- `firmware_format`
- `firmware_size`
- `firmware_sha256`
- `project_id`
- `fw_version`

主机会解压 `.ota`、校验 manifest、读取固件、把 `.hex` 转为连续二进制镜像，并校验固件 SHA-256 后再进入链路升级。

## 当前 OTA 流程

1. 建立连接。
2. 发送 `QUERY(0x01)`，携带目标固件版本、目标项目 ID 和强制升级 flag。
3. 设备返回当前版本、当前项目 ID 和是否允许升级。
4. 如果设备拒绝升级，主机停止，不发送 `START`。
5. 发送 `START(0x02)`，携带固件大小、SHA-256 和分块超时。
6. 循环发送 `DATA(0x03)`。
7. 发送 `FINISH(0x04)`，设备校验、标记 App 有效并复位。
8. 如果启用“激活校验”，等待设备重连后再次 `QUERY` 确认版本。

## 参数说明

### 分块大小

影响每次 `DATA` 请求携带的固件字节数。实际发送大小为：

```text
min(分块大小, 设备 START 响应 max_payload_size - DATA 请求头长度)
```

### 通信超时(s)

作为 TCP socket 或串口读写超时。值太小容易误判链路抖动，值太大则出错后等待更久。

### 连接/捕获(s)

控制主机等待设备上线并捕获 boot 的总时长。连接成功后，主机会用短读超时持续发送携带目标固件信息的 `QUERY`，让设备在 boot 捕获窗口内停留并裁决是否允许升级。

### 分块超时(ms)

写入 `START` 请求中的 `block_timeout`。设备侧会把它作为 OTA 包间超时。

### 重连超时(ms)

用于 `FINISH` 后等待设备重启并重新上线。激活校验等待时间为 `重连超时 / 1000 + 5` 秒。

### 强制升级

勾选后，主机会在 `QUERY` 请求中置位 `flags.bit0`。设备收到该 bit 后直接返回允许升级，并缓存本次目标固件信息供后续 `START` 使用。

### 激活校验

勾选后，`FINISH` 成功后主机会等待设备重连，再次 `QUERY` 并确认当前运行版本等于目标版本。

## 设备端准入

是否允许升级完全由设备端决定：

- 空 boot（`0.0.0 + SMOTA_BOOT`）允许升级任意非空目标项目 ID。
- 非空 boot 要求目标项目 ID 与当前项目 ID 一致。
- 如果设备开启防回滚，目标版本还必须通过设备端版本校验。
- 如果 `QUERY.flags.bit0` 置位，设备跳过上述检查并允许升级。
