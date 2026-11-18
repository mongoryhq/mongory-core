# Mongory Core

**Zero-dependency C99 core engine for Mongo-style in-memory query matching and rule evaluation** using arena memory pools — powering native language bindings and cross-checked against [`mongory-spec`](https://github.com/mongoryhq/mongory-spec).

> **[Interactive Playground & Visual Debugger ↗](https://mongoryhq.github.io/mongory-central/)** · [`mongory-rb` (Ruby)](https://github.com/mongoryhq/mongory-rb) · [`mongory-go` (Go)](https://github.com/mongoryhq/mongory-go) · [`mongory-js` (TypeScript/JS)](https://github.com/mongoryhq/mongory-js) · [`mongory-spec`](https://github.com/mongoryhq/mongory-spec)

## ✨ Features

- **MongoDB-like Queries**: Use familiar operators like `$eq`, `$gt`, `$lt`, `$in`, `$and`, `$or`, etc., to build complex queries.
- **Memory Pool**: All memory is managed through a simple memory pool, making resource management straightforward and helping to prevent memory leaks.
- **Generic Data Types**: Data is represented using a generic `mongory_value` type, which can encapsulate integers, doubles, strings, arrays, and tables (hash maps).
- **Extensible**: Provides interfaces to define your own custom value converters and regex engines, allowing `mongory-core` to work seamlessly with your existing data structures.
- **Well-Documented**: The public API is thoroughly documented with Doxygen-style comments in the header files.

## 🚀 Quick Start

Here is a simple example of how to use `mongory-core` to match a document against a query.

First, let's define a C data structure we want to query. Imagine we have a "document" representing a user:

```c
// Document to be queried
// {
//   "name": "Jules",
//   "language": "C",
//   "year": 2024
// }
```

And we want to see if it matches the following query:

```json
// Query
// {
//   "language": "C",
//   "year": { "$gt": 2020 }
// }
```

Here's the C code to perform this match using `mongory-core`:

```c
#include <stdio.h>
#include <mongory-core.h>

int main() {
    // 1. Initialize the Mongory library
    mongory_init();

    // 2. Create a memory pool for all allocations
    mongory_memory_pool *pool = mongory_memory_pool_new();

    // 3. Create the document to be queried as a mongory_table
    mongory_value *doc = MG_TABLE_WRAP(pool, 3,
        "name", mongory_value_wrap_s(pool, "Jules"),
        "language", mongory_value_wrap_s(pool, "C"),
        "year", mongory_value_wrap_i(pool, 2024)
    );

    // 4. Create the query condition
    // { "language": "C", "year": { "$gt": 2020 } }
    mongory_value *query = MG_TABLE_WRAP(pool, 2,
        "language", mongory_value_wrap_s(pool, "C"),
        "year", MG_TABLE_WRAP(pool, 1,
            "$gt", mongory_value_wrap_i(pool, 2020)
        )
    );

    // 5. Create a matcher with the query condition
    mongory_matcher *matcher = mongory_matcher_new(pool, query, NULL);

    // 6. Perform the match
    bool is_match = mongory_matcher_match(matcher, doc);

    // 7. Print the result
    if (is_match) {
        printf("The document matches the query!\\n");
    } else {
        printf("The document does not match the query.\\n");
    }

    // 8. Clean up
    pool->free(pool);
    mongory_cleanup();

    return 0;
}
```
*(Note: For a runnable version of this example, see `benchmarks/basic_test.c`)*

## 🛠️ Building and Testing

This project uses `CMake` for cross-platform building and testing, with a convenient build script for easy usage.

### Dependencies
- **Core library (`libmongory-core.a`)**: Zero external runtime dependencies — only a C99 compiler (`gcc`/`clang`) and `CMake` (>= 3.12) (`-DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF`).
- **Unit tests & benchmarks**: Require `cJSON` (`libcjson-dev` on Ubuntu/Debian, `cjson` on macOS) and the Unity test framework (`./build.sh --setup-unity`).
- **Cross-language `mongory-spec` conformance tests**: Additionally require `libpcre2-8` (`libpcre2-dev` on Ubuntu/Debian, `pcre2` on macOS) and `-DMONGORY_SPEC_DIR=/path/to/mongory-spec`.

#### Installation by Platform

**macOS (Homebrew):**
```bash
# Library only
brew install cmake

# Full tests + mongory-spec conformance
brew install cmake cjson pcre2
```

**Ubuntu/Debian:**
```bash
# Library only
sudo apt update && sudo apt install -y cmake build-essential

# Full tests + mongory-spec conformance
sudo apt update && sudo apt install -y cmake build-essential libcjson-dev libpcre2-dev pkg-config
```

**CentOS/RHEL/Fedora:**
```bash
# CentOS/RHEL
sudo yum install cmake cjson-devel pcre2-devel gcc
# Fedora
sudo dnf install cmake cjson-devel pcre2-devel gcc
```

### Quick Build (Recommended)

Use the provided build script for the easiest experience:

```bash
# Basic build
./build.sh

# First-time setup for Unity test framework and run unit tests
./build.sh --setup-unity --test

# Run unit tests + mongory-spec conformance
./build.sh --setup-unity
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=OFF -DMONGORY_SPEC_DIR=../mongory-spec
cmake --build build
ctest --test-dir build --output-on-failure
```

### Manual CMake Build

```bash
# 1. Setup Unity test framework (first time only)
./build.sh --setup-unity

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

### Build Options

```bash
# Release build (optimized)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (with debug symbols)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Disable tests or benchmarks
cmake -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF ..
```

### Code Formatting
The project uses `clang-format` for code formatting:
```bash
cmake --build build --target format
# or
./build.sh && cmake --build build --target format
```

### Legacy Makefile
The original Makefile is still available but deprecated. For cross-platform compatibility, please use the CMake build system.

> 📝 **Migration Note**: This project was migrated from Makefile to CMake for better cross-platform support. See `CMAKE_USAGE.md` for detailed CMake usage instructions.

## 📚 API Documentation

The public API is documented in the header files in the `include/` directory. The comments are compatible with Doxygen. You can generate HTML documentation if you have Doxygen installed.
