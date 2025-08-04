# 從 Makefile 遷移到 CMake

本文檔記錄了 mongory-core 項目從 Makefile 遷移到 CMake 的過程。

## 🚀 遷移原因

原始的 Makefile 配置主要針對 macOS 環境，存在以下限制：

- **平台特定**: 硬編碼了 Homebrew 路徑 (`/opt/homebrew/include`)
- **依賴管理**: 依賴 `brew --prefix` 命令查找庫路徑
- **跨平台支援**: 無法在 Linux 或 Windows 上輕易構建

## ✅ 遷移成果

### 新的功能
- **跨平台支援**: 支援 macOS、Linux、Windows (MSYS2)
- **自動依賴檢測**: 通過 pkg-config 和手動查找自動定位 cJSON
- **便捷構建腳本**: 提供 `build.sh` 腳本簡化常用操作
- **靈活配置**: 支援 Release/Debug 模式、可選測試和基準測試

### 保留的功能
- **所有原有功能**: 構建靜態庫、運行測試、基準測試、代碼格式化
- **Unity 測試框架**: 繼續使用相同的測試設置
- **相同的輸出**: 產生相同的 `libmongory-core.a` 靜態庫

## 📋 遷移對比

| 功能 | 舊 Makefile | 新 CMake |
|------|-------------|----------|
| 基本構建 | `make` | `./build.sh` 或 `cmake --build build` |
| 運行測試 | `make test` | `./build.sh --test` 或 `ctest` |
| 基準測試 | `make benchmark` | `./build.sh --benchmark` |
| 代碼格式化 | `make format` | `cmake --build build --target format` |
| 清理 | `make clean` | `./build.sh --clean` |
| 設置 Unity | `make setup-unity` | `./build.sh --setup-unity` |

## 🔧 新增功能

### 構建腳本選項
```bash
./build.sh [選項]
  -d, --debug         Debug 模式構建
  -c, --clean         清理構建目錄
  -t, --test          運行測試
  -b, --benchmark     運行基準測試
  -u, --setup-unity   設置 Unity 測試框架
  -h, --help          顯示幫助信息
```

### CMake 配置選項
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..       # Debug 構建
cmake -DBUILD_TESTS=OFF ..              # 不構建測試
cmake -DBUILD_BENCHMARKS=OFF ..         # 不構建基準測試
```

## 🌍 跨平台支援

### 自動依賴檢測
- **優先使用 pkg-config**: 自動檢測系統安裝的 cJSON
- **回退到手動查找**: 在標準路徑和 Homebrew 路徑中查找
- **清晰的錯誤信息**: 如果找不到依賴會給出明確的安裝指引

### 平台特定處理
- **macOS**: 自動檢測 Homebrew 路徑
- **Linux**: 使用標準系統路徑
- **Windows**: 支援 MSYS2/MinGW 環境

## 📁 文件結構變化

### 新增文件
- `CMakeLists.txt` - CMake 配置文件
- `build.sh` - 便捷構建腳本
- `CMAKE_USAGE.md` - 詳細的 CMake 使用說明
- `MIGRATION.md` - 此遷移文檔

### 保留文件
- `Makefile` - 保留但標記為已棄用
- 所有源碼和頭文件保持不變
- 測試文件和結構保持不變

## 🚨 注意事項

### 對現有用戶的影響
1. **向後相容**: 舊的 Makefile 仍然可用
2. **推薦遷移**: 建議使用新的 CMake 系統
3. **文檔更新**: README.md 已更新為 CMake 指引

### 開發者工作流程
- **新開發者**: 使用 `./build.sh --setup-unity --test` 開始
- **CI/CD**: 可以使用 `cmake` 命令實現更精細的控制
- **IDE 支援**: 現在支援 CLion、VS Code 等現代 IDE

## 🎯 未來計劃

- ✅ 基本 CMake 遷移
- ✅ 跨平台構建支援
- ✅ 便捷構建腳本
- ✅ 文檔更新
- 🔄 CI/CD 配置更新 (未來)
- 🔄 IDE 項目文件 (可選)

## 📞 支援

如果遇到構建問題：

1. 查看 `CMAKE_USAGE.md` 詳細說明
2. 確認已安裝所需依賴
3. 使用 `./build.sh --help` 查看可用選項
4. 檢查 CMake 輸出的配置信息

---

**遷移完成日期**: 2024年5月8日  
**測試狀態**: ✅ 所有測試通過  
**支援平台**: macOS, Linux, Windows (MSYS2)