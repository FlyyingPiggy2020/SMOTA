# smOTA 上位机

这个目录是新的独立上位机实现，和 `examples/win_sim/test_host.py` 不再绑在一起。

当前版本特性：

- 桌面 GUI，技术栈为 Python `tkinter`
- 选择固件文件
- 配置版本号、项目 ID、TCP 地址和超时参数
- 显示升级进度
- 显示主机侧详细日志
- 显示 `TX` / `RX` 协议帧摘要
- 可直接连接手动启动的 `win_sim`
- 支持 `TCP` / `Serial(USB CDC)` 两种传输
- 连接后先读取设备当前固件版本，再决定是否继续下载

## 启动

在仓库根目录执行：

```powershell
pip install pyserial
python -m smota_host
```

## 和 win_sim 联调

先在一个终端里手动启动模拟器，这样你能直接看到设备侧日志：

```powershell
cd C:\Users\w1545\Desktop\Project\SMOTA
.\examples\win_sim\build\win_sim.exe -i
.\examples\win_sim\build\win_sim.exe -r
```

然后启动 GUI：

```powershell
python -m smota_host
```

推荐参数：

- Host: `127.0.0.1`
- Port: `8888`
- Version: `1.2.3`
- Project ID: `SMOTA_WIN_SIM`
- Chunk Size: `128`

连接流程现在默认是：

1. 建立连接
2. 优先发送 `QUERY_VERSION(0x07)` 查询设备当前运行版本（旧固件会自动回退到 `ACTIVATE_CHECK(0x06)`）
3. 版本一致时默认跳过下载（除非启用 `Force Install`）
4. 版本不一致时继续握手与升级流程

固件文件第一版不强制要求是真实 MCU 镜像，`win_sim` 当前验证的是 OTA 传输流程，所以任意文件都可用于联调，例如：

- `examples/win_sim/README.md`
- 你的 `app.bin`
- 你的 `firmware.hex`

## 当前范围

现在可选 `TCP` 或 `Serial`：

- 调 `win_sim`：选择 `tcp`
- 连真实设备 USB CDC：选择 `serial`，填写 `COMx` 和波特率（默认 `115200`）

后续可继续补充：

- 固件包元数据解析
- 升级历史和结果导出
- 更完整的十六进制报文查看器
