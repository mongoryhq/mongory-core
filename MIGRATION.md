# Migration from Makefile to CMake

This document records the migration process of the mongory-core project from Makefile to CMake.

## 🚀 Migration Reasons

The original Makefile configuration was primarily targeted for macOS environments, with the following limitations:

- **Platform-specific**: Hard-coded Homebrew paths (`/opt/homebrew/include`)
- **Dependency management**: Relies on `brew --prefix` command to find library paths
- **Cross-platform support**: Cannot easily build on Linux or Windows

## ✅ Migration Results

### New Features
- **Cross-platform support**: Supports macOS, Linux, Windows (MSYS2)
- **Automatic dependency detection**: Automatically locates cJSON through pkg-config and manual discovery
- **Convenient build script**: Provides `build.sh` script to simplify common operations
- **Flexible configuration**: Supports Release/Debug modes, optional tests and benchmarks

### Preserved Features
- **All original functionality**: Build static library, run tests, benchmarks, code formatting
- **Unity test framework**: Continues to use the same test setup
- **Same output**: Produces the same `libmongory-core.a` static library

## 📋 Migration Comparison

| Feature | Old Makefile | New CMake |
|---------|-------------|-----------|
| Basic build | `make` | `./build.sh` or `cmake --build build` |
| Run tests | `make test` | `./build.sh --test` or `ctest` |
| Benchmarks | `make benchmark` | `./build.sh --benchmark` |
| Code formatting | `make format` | `cmake --build build --target format` |
| Clean | `make clean` | `./build.sh --clean` |
| Setup Unity | `make setup-unity` | `./build.sh --setup-unity` |

## 🔧 New Features

### Build Script Options
```bash
./build.sh [options]
  -d, --debug         Debug mode build
  -c, --clean         Clean build directory
  -t, --test          Run tests
  -b, --benchmark     Run benchmarks
  -u, --setup-unity   Setup Unity test framework
  -h, --help          Show help information
```

### CMake Configuration Options
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..       # Debug build
cmake -DBUILD_TESTS=OFF ..              # Don't build tests
cmake -DBUILD_BENCHMARKS=OFF ..         # Don't build benchmarks
```

## 🌍 Cross-Platform Support

### Automatic Dependency Detection
- **Prioritize pkg-config**: Automatically detects system-installed cJSON
- **Fallback to manual search**: Searches in standard paths and Homebrew paths
- **Clear error messages**: Provides clear installation guidance if dependencies are not found

### Platform-Specific Handling
- **macOS**: Automatically detects Homebrew paths
- **Linux**: Uses standard system paths
- **Windows**: Supports MSYS2/MinGW environment

## 📁 File Structure Changes

### New Files
- `CMakeLists.txt` - CMake configuration file
- `build.sh` - Convenient build script
- `CMAKE_USAGE.md` - Detailed CMake usage instructions
- `MIGRATION.md` - This migration document

### Preserved Files
- `Makefile` - Preserved but marked as deprecated
- All source and header files remain unchanged
- Test files and structure remain unchanged

## 🚨 Important Notes

### Impact on Existing Users
1. **Backward compatibility**: Old Makefile is still usable
2. **Recommended migration**: Recommend using the new CMake system
3. **Documentation update**: README.md has been updated with CMake guidance

### Developer Workflow
- **New developers**: Use `./build.sh --setup-unity --test` to get started
- **CI/CD**: Can use `cmake` commands for more fine-grained control
- **IDE support**: Now supports modern IDEs like CLion, VS Code

## 🎯 Future Plans

- ✅ Basic CMake migration
- ✅ Cross-platform build support
- ✅ Convenient build script
- ✅ Documentation update
- 🔄 CI/CD configuration update (future)
- 🔄 IDE project files (optional)

## 📞 Support

If you encounter build issues:

1. Check `CMAKE_USAGE.md` for detailed instructions
2. Ensure required dependencies are installed
3. Use `./build.sh --help` to view available options
4. Check CMake configuration output information

---

**Migration completion date**: May 8, 2024  
**Test status**: ✅ All tests passing  
**Supported platforms**: macOS, Linux, Windows (MSYS2)