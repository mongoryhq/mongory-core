#!/bin/bash

# MongoDB Core Library 構建腳本

set -e  # 遇到錯誤就停止

# 顏色定義
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默認配置
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN=false
RUN_TESTS=false
RUN_BENCHMARKS=false
SETUP_UNITY=false

# 解析命令行參數
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -b|--benchmark)
            RUN_BENCHMARKS=true
            shift
            ;;
        -u|--setup-unity)
            SETUP_UNITY=true
            shift
            ;;
        -h|--help)
            echo "用法: $0 [選項]"
            echo "選項:"
            echo "  -d, --debug         Debug 模式構建"
            echo "  -c, --clean         清理構建目錄"
            echo "  -t, --test          運行測試"
            echo "  -b, --benchmark     運行基準測試"
            echo "  -u, --setup-unity   設置 Unity 測試框架"
            echo "  -h, --help          顯示此幫助信息"
            exit 0
            ;;
        *)
            echo "未知選項: $1"
            echo "使用 $0 --help 查看幫助"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}=== MongoDB Core Library 構建腳本 ===${NC}"

# 設置 Unity（如果需要）
if [ "$SETUP_UNITY" = true ]; then
    echo -e "${BLUE}設置 Unity 測試框架...${NC}"
    chmod +x scripts/setup_unity.sh
    ./scripts/setup_unity.sh
fi

# 清理構建目錄（如果需要）
if [ "$CLEAN" = true ]; then
    echo -e "${BLUE}清理構建目錄...${NC}"
    rm -rf $BUILD_DIR
fi

# 創建構建目錄
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 運行 CMake 配置
echo -e "${BLUE}配置項目 (構建類型: $BUILD_TYPE)...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# 構建項目
echo -e "${BLUE}構建項目...${NC}"
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo -e "${GREEN}✅ 構建完成！${NC}"

# 運行測試（如果需要）
if [ "$RUN_TESTS" = true ]; then
    echo -e "${BLUE}運行測試...${NC}"
    if ctest --output-on-failure; then
        echo -e "${GREEN}✅ 所有測試通過！${NC}"
    else
        echo -e "${RED}❌ 有測試失敗！${NC}"
        exit 1
    fi
fi

# 運行基準測試（如果需要）
if [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "${BLUE}運行基準測試...${NC}"
    for benchmark in bin/benchmark_*; do
        if [ -x "$benchmark" ]; then
            echo -e "${BLUE}運行 $benchmark${NC}"
            $benchmark
        fi
    done
fi

echo -e "${GREEN}🎉 完成！${NC}"
echo -e "構建產物位於: ${BLUE}$BUILD_DIR/${NC}"
echo -e "靜態庫: ${BLUE}$BUILD_DIR/lib/libmongory-core.a${NC}"