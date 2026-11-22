# CMake Build Instructions

This project has been migrated from Makefile to CMake, supporting cross-platform builds.

## System Requirements

- CMake 3.12 or higher
- C99 compatible compiler (GCC, Clang, MSVC)
- Zero external dependencies for `libmongory-core` itself (tests/benchmarks and opt-in `-DMONGORY_WITH_JSON=ON` use `yyjson`, fetched on demand via `./scripts/setup_yyjson.sh` or CMake `FetchContent`)

## Dependency Installation

### macOS (Homebrew)
```bash
brew install cmake
```

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install cmake build-essential
```

### CentOS/RHEL/Fedora
```bash
# CentOS/RHEL
sudo yum install cmake gcc
# or Fedora
sudo dnf install cmake gcc
```

### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
```

## Quick Start

### Using Build Script (Recommended)

```bash
# Basic build
./build.sh

# Setup Unity test framework and build
./build.sh --setup-unity

# Debug mode build and run tests
./build.sh --debug --test

# Clean rebuild and run benchmarks
./build.sh --clean --benchmark

# View all options
./build.sh --help
```

### Manual CMake Usage

```bash
# 1. Setup Unity test framework (first time only)
chmod +x scripts/setup_unity.sh
./scripts/setup_unity.sh

# 2. Create build directory
mkdir build && cd build

# 3. Configure project
cmake ..

# 4. Build
cmake --build .

# 5. Run tests
ctest

# 6. Install (optional)
sudo cmake --install .
```

## Build Options

### Build Types
```bash
# Release mode (default, optimized)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug mode (debug symbols)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### Optional Features
```bash
# Don't build tests
cmake -DBUILD_TESTS=OFF ..

# Don't build benchmarks
cmake -DBUILD_BENCHMARKS=OFF ..
```

### Custom Install Path
```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

## Output Files

After building, you will find the following in the `build/` directory:

- `lib/libmongory-core.a` - Static library
- `bin/mongory_*_test` - Test executables
- `bin/benchmark_*` - Benchmark executables

## Common Commands

```bash
# Format code
cmake --build build --target format

# Setup Unity
cmake --build build --target setup-unity

# Build only core library
cmake --build build --target mongory-core

# Run specific test
./build/bin/mongory_array_test

# Clean build
rm -rf build
```

## Cross-Platform Notes

- **macOS**: Supports native arm64/x86_64 and Universal Binaries (`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`)
- **Linux**: Uses standard system paths
- **Windows**: Supports MSYS2/MinGW and MSVC environments
- Optional `yyjson` (for tests/benchmarks or `-DMONGORY_WITH_JSON=ON`) is resolved via system `find_package(yyjson)`, local `third_party/yyjson` (`./scripts/setup_yyjson.sh`), or CMake `FetchContent`

## Troubleshooting

### yyjson not found (when building tests or `-DMONGORY_WITH_JSON=ON`)
```bash
# Fetch yyjson into untracked third_party/yyjson
./scripts/setup_yyjson.sh
```

### Unity test framework issues
```bash
# Re-setup Unity
rm -rf tests/unity
./build.sh --setup-unity
```

### Permission issues
```bash
# Ensure scripts are executable
chmod +x build.sh scripts/setup_unity.sh
```