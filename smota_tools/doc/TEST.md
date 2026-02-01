# SMOTA Tool 测试清单

> 本文档用于验证每个开发阶段的正确性，避免配置问题遗漏到生产环境

## 测试策略

每次代码变更后，按照以下顺序执行测试：
1. **快速验证** - 编译检查
2. **UI 验证** - 界面渲染
3. **功能验证** - 核心功能

---

## 快速验证（每次提交后执行）

### 1. Rust 后端编译

```bash
cd smota_tools/serial-tool/src-tauri
cargo check
```

**预期输出：** `Finished ...` 无错误无警告

**常见问题：**
- ❌ 缺少依赖 → `cargo add <package-name>`
- ❌ 弃用 API → 按警告修复

---

### 2. 前端依赖安装

```bash
cd smota_tools/serial-tool
pnpm install
```

**预期输出：** `Done in ...s`

**验证：**
- 检查 `node_modules` 目录存在
- 检查 `pnpm-lock.yaml` 已更新

---

### 3. 前端编译

```bash
pnpm build
```

**预期输出：** `✓ built in ...s`

**TypeScript 检查：**
- 无 `error TS...`
- 无未使用的导入警告

---

### 4. 启动开发服务器

```bash
pnpm dev
```

**预期：**
- 输出 `Local: http://localhost:1420/`
- 浏览器能访问

---

## UI 渲染验证（每个阶段执行）

### 检查清单

| 检查项 | 预期结果 | 通过 |
|--------|----------|------|
| 窗口正常打开 | 无白屏/黑屏 | ☐ |
| 标题正确显示 | 显示 "串口调试工具" | ☐ |
| 串口连接面板 | 显示刷新按钮、下拉框 | ☐ |
| 串口参数设置 | 显示波特率、数据位等选项 | ☐ |
| 数据接收区域 | 显示黑色背景的接收区 | ☐ |
| 数据发送区域 | 显示输入框和发送按钮 | ☐ |
| Naive UI 组件 | 按钮、开关等样式正常 | ☐ |
| 字体显示 | 中文正常显示 | ☐ |

### 截图对比

**正常情况：**
```
┌─────────────────────────────────────────┐
│            串口调试工具                  │
├─────────────────────────────────────────┤
│  [下拉框选择串口] [刷新] [连接]          │
│  波特率: [115200 ▼] 数据位: [8 ▼] ...   │
├─────────────────────────────────────────┤
│  接收区                                  │
│  ┌─────────────────────────────────┐    │
│  │ [14:32:05.123] Hello World      │    │
│  └─────────────────────────────────┘    │
│  [ ] 时间戳 [ ] Hex显示 [ ] 自动滚动     │
├─────────────────────────────────────────┤
│  发送区                                  │
│  ┌─────────────────────────┐ [发送]     │
│  │                         │            │
│  └─────────────────────────┘            │
└─────────────────────────────────────────┘
```

---

## 功能测试（每个阶段执行）

### 阶段一：纯串口工具

| 功能 | 测试步骤 | 预期结果 | 通过 |
|------|----------|----------|------|
| 加载串口 | 点击刷新 | 列出可用串口 | ☐ |
| 连接串口 | 选择串口，点击连接 | 按钮变"断开" | ☐ |
| 断开串口 | 点击断开 | 按钮恢复"连接" | ☐ |
| 发送数据 | 输入文本，点击发送 | 发送计数增加 | ☐ |
| 接收数据 | 对端发送数据 | 接收区显示数据 | ☐ |
| Hex 显示 | 勾选 Hex 模式 | 数据以 Hex 显示 | ☐ |
| Hex 发送 | 勾选 Hex，发送 | 正确发送 Hex | ☐ |
| 时间戳 | 勾选时间戳 | 显示时间 | ☐ |
| 发送历史 | 发送后查看历史 | 历史有记录 | ☐ |
| 热插拔 | 连接后拔出设备 | 弹出警告并断开 | ☐ |

---

## 自动化测试命令

创建脚本 `test-quick.ps1`（PowerShell）：

```powershell
# 快速验证脚本
Write-Host "=== 1. Rust Check ===" -ForegroundColor Cyan
cd "src-tauri"
cargo check
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Rust 编译失败" -ForegroundColor Red
    exit 1
}
Write-Host "✅ Rust 编译通过" -ForegroundColor Green

Write-Host "`n=== 2. Frontend Build ===" -ForegroundColor Cyan
cd ".."
pnpm build
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ 前端编译失败" -ForegroundColor Red
    exit 1
}
Write-Host "✅ 前端编译通过" -ForegroundColor Green

Write-Host "`n=== 3. UI Check ===" -ForegroundColor Cyan
Write-Host "请手动检查窗口是否正常渲染"
Write-Host "- 窗口标题: '串口调试工具'"
Write-Host "- 显示串口连接面板"
Write-Host "- 显示数据收发区域"
Write-Host "- Naive UI 组件样式正常"
Write-Host "`n如果一切正常，输入 y 继续，否则输入 n:"
$answer = Read-Host
if ($answer -ne "y") {
    Write-Host "❌ UI 验证未通过" -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ 所有快速验证通过" -ForegroundColor Green
```

---

## CI/CD 验证（可选）

如果使用 GitHub Actions，添加 `.github/workflows/test.yml`：

```yaml
name: Test

on: [push, pull_request]

jobs:
  rust-check:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Rust Check
        run: |
          cd smota_tools/serial-tool/src-tauri
          cargo check

  frontend-build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: pnpm/action-setup@v4
        with:
          version: 10
      - name: Install deps
        run: pnpm install
        working-directory: smota_tools/serial-tool
      - name: Build
        run: pnpm build
        working-directory: smota_tools/serial-tool
```

---

## 问题排查清单

### 白色窗口/空白页面

| 可能原因 | 排查方法 | 解决方法 |
|----------|----------|----------|
| 前端未构建 | 检查 `dist/` 目录是否存在 | `pnpm build` |
| Tailwind 未配置 | 检查 `vite.config.ts` | 添加 `@tailwindcss/vite` |
| 依赖未安装 | 检查 `node_modules` | `pnpm install` |
| 端口被占用 | 检查 1420 端口 | 关闭占用程序 |

### 编译错误

| 错误信息 | 解决方法 |
|----------|----------|
| `cannot find module` | `pnpm install` |
| `error TS...` | 修复 TypeScript 错误 |
| `deprecated` | 按警告修复 API |
| `linker.exe not found` | 安装 VS Build Tools |

---

## 阶段验证签字

| 阶段 | 测试日期 | 测试人 | 结果 |
|------|----------|--------|------|
| 阶段一 | 2025-01-31 | - | ✅ 通过 |
| 阶段二 | - | - | ⏳ 待测试 |
| 阶段三 | - | - | ⏳ 待测试 |

---

*文档版本: v1.0*
*最后更新: 2025-01-31*
