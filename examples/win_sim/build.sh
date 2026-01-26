#!/bin/bash
# smOTA Windows 模拟示例编译脚本 (基于 CMake)

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 路径定义
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# 检测构建生成器
CMAKE_GENERATOR=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    # Windows 环境：优先使用 Ninja，其次 MinGW Makefiles
    if command -v ninja &> /dev/null; then
        CMAKE_GENERATOR="-G Ninja"
    elif command -v mingw32-make &> /dev/null; then
        CMAKE_GENERATOR="-G 'MinGW Makefiles'"
    elif command -v mingw64-make &> /dev/null; then
        CMAKE_GENERATOR="-G 'MinGW Makefiles'"
    fi
fi

# 解析命令行参数
DEBUG_BUILD="OFF"
CLEAN_BUILD=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            DEBUG_BUILD="ON"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD="--clean-first"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --debug    Enable debug output (WIN_SIM_ENABLE_DEBUG)"
            echo "  -c, --clean    Clean build before compiling"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown option: $1"
            echo "Use -h or --help for usage"
            exit 1
            ;;
    esac
done

echo "=============================================="
echo "  smOTA Windows Simulation Example"
echo "  CMake Build Script"
echo "=============================================="
echo ""

# 1. 创建构建目录
echo "[1/3] Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$SCRIPT_DIR"

# 2. CMake 配置
echo "[2/3] Configuring with CMake..."

# 检查是否需要清理旧的构建缓存（生成器变更时）
if [ -d "$BUILD_DIR" ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    # 检测当前生成器
    CURRENT_GENERATOR=$(grep "CMAKE_GENERATOR:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2)
    # 如果定义了新的生成器且与当前不同，则清理
    if [ -n "$CMAKE_GENERATOR" ]; then
        NEW_GEN=$(echo "$CMAKE_GENERATOR" | sed "s/-G //; s/'//g")
        if [ "$CURRENT_GENERATOR" != "$NEW_GEN" ]; then
            echo -e "${YELLOW}[INFO]${NC} Generator changed from '$CURRENT_GENERATOR' to '$NEW_GEN', cleaning build directory..."
            rm -rf "$BUILD_DIR"
            mkdir -p "$BUILD_DIR"
        fi
    fi
fi

eval cmake -B "$BUILD_DIR" $CMAKE_GENERATOR \
      -DWIN_SIM_ENABLE_DEBUG=$DEBUG_BUILD \
      -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} CMake configuration failed!"
    echo ""
    echo "Tip: Try cleaning the build directory:"
    echo "  rm -rf $BUILD_DIR"
    exit 1
fi

# 3. 构建
echo "[3/3] Building..."
cmake --build "$BUILD_DIR" $CLEAN_BUILD

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} Build failed!"
    exit 1
fi

# 完成
echo ""
echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}  Build successful!${NC}"
echo -e "${GREEN}==============================================${NC}"
echo ""

# 检测可执行文件路径
if [ -f "$BUILD_DIR/win_sim.exe" ]; then
    echo "Executable: $BUILD_DIR/win_sim.exe"
elif [ -f "$BUILD_DIR/Release/win_sim.exe" ]; then
    echo "Executable: $BUILD_DIR/Release/win_sim.exe"
elif [ -f "$BUILD_DIR/Debug/win_sim.exe" ]; then
    echo "Executable: $BUILD_DIR/Debug/win_sim.exe"
else
    echo "Executable: $BUILD_DIR/win_sim (or win_sim.exe)"
fi
echo ""
echo "Run with:"
echo "  cd $SCRIPT_DIR"
echo "  ./build/win_sim.exe"
echo ""
