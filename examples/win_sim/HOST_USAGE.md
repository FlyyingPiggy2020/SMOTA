# win_sim 上位机说明

`examples/win_sim/test_host.py` 现在是一个可直接驱动 `win_sim` 的 Python CLI，上位机与模拟器通过 `TCP 127.0.0.1:8888` 通信。

## 典型流程

先编译模拟器：

```powershell
cmake -S examples/win_sim -B examples/win_sim/build
cmake --build examples/win_sim/build
```

然后执行完整升级仿真：

```powershell
python examples/win_sim/test_host.py --reset-sim --firmware examples/win_sim/README.md --version 1.2.3
```

脚本会自动完成这些步骤：

1. 重置模拟器 flash 和运行版本
2. 启动 `win_sim.exe -r`
3. 握手
4. 下发头部信息
5. 分块发送固件
6. 请求校验
7. 请求安装
8. 等待模拟器重启
9. 再次连接并执行 `activate_check`

## 常用参数

- `--firmware`: 仿真用固件文件路径
- `--version`: 目标版本号，格式 `major.minor.patch`
- `--chunk-size`: 主机侧期望的数据块大小
- `--sim-exe`: 指定 `win_sim.exe` 路径
- `--skip-activate-check`: 只验证到安装响应
- `--verbose-sim`: 打印模拟器日志

## 直接连接已运行设备

如果 `win_sim` 已经由你手动启动，可以关闭自动拉起逻辑：

```powershell
python examples/win_sim/test_host.py --sim-exe "" --host 127.0.0.1 --port 8888 --firmware path/to/fw.bin --version 2.0.0
```
