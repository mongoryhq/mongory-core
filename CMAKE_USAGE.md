# CMake 構建說明

本項目已從 Makefile 遷移到 CMake，支援跨平台構建。

## 系統要求

- CMake 3.12 或更高版本
- C99 相容的編譯器 (GCC, Clang, MSVC)
- cJSON 函式庫

## 依賴安裝

### macOS (Homebrew)
```bash
brew install cmake cjson
```

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install cmake libcjson-dev build-essential
```

### CentOS/RHEL/Fedora
```bash
# CentOS/RHEL
sudo yum install cmake cjson-devel gcc
# 或 Fedora
sudo dnf install cmake cjson-devel gcc
```

### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-cjson mingw-w64-x86_64-gcc
```

## 快速開始

### 使用構建腳本（推薦）

```bash
# 基本構建
./build.sh

# 設置 Unity 測試框架並構建
./build.sh --setup-unity

# Debug 模式構建並運行測試
./build.sh --debug --test

# 清理重新構建並運行基準測試
./build.sh --clean --benchmark

# 查看所有選項
./build.sh --help
```

### 手動使用 CMake

```bash
# 1. 設置 Unity 測試框架（首次需要）
chmod +x scripts/setup_unity.sh
./scripts/setup_unity.sh

# 2. 創建構建目錄
mkdir build && cd build

# 3. 配置項目
cmake ..

# 4. 構建
cmake --build .

# 5. 運行測試
ctest

# 6. 安裝（可選）
sudo cmake --install .
```

## 構建選項

### 構建類型
```bash
# Release 模式（默認，最佳化）
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug 模式（除錯符號）
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### 可選功能
```bash
# 不構建測試
cmake -DBUILD_TESTS=OFF ..

# 不構建基準測試
cmake -DBUILD_BENCHMARKS=OFF ..
```

### 自定義安裝路徑
```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

## 輸出文件

構建完成後，你會在 `build/` 目錄下找到：

- `lib/libmongory-core.a` - 靜態庫
- `bin/mongory_*_test` - 測試可執行文件
- `bin/benchmark_*` - 基準測試可執行文件

## 常用指令

```bash
# 格式化代碼
cmake --build build --target format

# 設置 Unity
cmake --build build --target setup-unity

# 只構建核心庫
cmake --build build --target mongory-core

# 運行特定測試
./build/bin/mongory_array_test

# 清理構建
rm -rf build
```

## 跨平台注意事項

- **macOS**: 自動檢測 Homebrew 路徑
- **Linux**: 使用標準系統路徑
- **Windows**: 支援 MSYS2/MinGW 環境
- cJSON 庫會自動通過 pkg-config 或手動路徑檢測

## 故障排除

### cJSON 找不到
```bash
# 檢查是否已安裝
pkg-config --exists libcjson && echo "已安裝" || echo "未安裝"

# 手動指定路徑（如果需要）
cmake -DCJSON_INCLUDE_DIR=/path/to/cjson/include -DCJSON_LIBRARY=/path/to/libcjson.so ..
```

### Unity 測試框架問題
```bash
# 重新設置 Unity
rm -rf tests/unity
./build.sh --setup-unity
```

### 權限問題
```bash
# 確保腳本可執行
chmod +x build.sh scripts/setup_unity.sh
```