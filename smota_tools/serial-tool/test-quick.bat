@echo off
chcp 65001 >nul
echo ============================================
echo       SMOTA Tool 快速验证脚本
echo ============================================
echo.

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

echo [1/4] 检查 Rust 编译...
echo -------------------------------------------
cd src-tauri
cargo check
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ❌ Rust 编译失败，请检查错误信息
    pause
    exit /b 1
)
echo ✅ Rust 编译通过
echo.

echo [2/4] 安装前端依赖...
echo -------------------------------------------
cd ..
pnpm install
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ❌ 依赖安装失败，请检查错误信息
    pause
    exit /b 1
)
echo ✅ 依赖安装完成
echo.

echo [3/4] 构建前端...
echo -------------------------------------------
pnpm build
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ❌ 前端构建失败，请检查错误信息
    pause
    exit /b 1
)
echo ✅ 前端构建成功
echo.

echo [4/4] 启动开发服务器...
echo -------------------------------------------
echo ✅ 开发服务器将在 http://localhost:1420/ 启动
echo.
echo ============================================
echo       验证完成！请检查以下项目：
echo ============================================
echo.
echo [UI 检查清单]
echo ☐ 窗口标题显示 "串口调试工具"
echo ☐ 显示串口连接面板（刷新按钮、下拉框、连接按钮）
echo ☐ 显示串口参数设置（波特率、数据位、停止位、校验位）
echo ☐ 显示数据接收区域（黑色背景）
echo ☐ 显示数据发送区域（输入框和发送按钮）
echo ☐ Naive UI 组件样式正常
echo ☐ 中文文字正常显示
echo.
echo [功能检查清单]
echo ☐ 点击刷新可列出串口
echo ☐ 选择串口后点击连接
echo ☐ 发送数据测试
echo ☐ Hex 模式切换
echo ☐ 时间戳显示
echo ☐ 发送历史功能
echo.
echo 如有异常，请按 Ctrl+C 停止服务器并修复问题
echo ============================================
echo.

timeout /t 2 >nul

REM 启动开发服务器但不退出
pnpm dev

echo.
echo 服务器已停止
pause
