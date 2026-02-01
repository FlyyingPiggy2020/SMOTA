# 串口工具 UI 测试说明

## 快速开始

### 1. 启动应用

```bash
cd smota_tools/serial-tool
pnpm tauri dev
```

### 2. 运行 UI 测试

```bash
pnpm test:ui
```

## 测试文件

- `tests/dropdown.spec.ts` - 下拉框功能测试

## 测试内容

### dropdown.spec.ts 测试项

1. **波特率下拉框测试**
   - 验证下拉框可点击
   - 验证选项显示正确

2. **串口选择测试**
   - 验证检测到的串口显示
   - 验证可以展开下拉菜单

3. **Vue 警告检测**
   - 检测 Teleport 相关警告
   - 记录问题便于排查

4. **页面组件测试**
   - 验证所有 UI 组件正确加载

## 预期问题

由于 Tauri + NaiveUI 的 Teleport 兼容性问题，下拉框点击可能无响应。这是已知问题，测试会记录相关 Vue 警告。

## 手动测试步骤

如果自动测试失败，请手动验证：

1. 打开 `http://localhost:1420`
2. 点击"波特率"下拉框
3. 观察是否有选项菜单出现
4. 检查浏览器控制台 (F12) 是否有 Vue 警告
