# Mongory Core

`mongory-core` is a lightweight, dependency-free C library for querying C data structures using a query syntax inspired by MongoDB. It is designed to be extensible, allowing you to integrate it with your own custom data types and matching logic.

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
    mongory_table *doc_table = mongory_table_new(pool);
    doc_table->set(doc_table, "name", mongory_value_wrap_s(pool, "Jules"));
    doc_table->set(doc_table, "language", mongory_value_wrap_s(pool, "C"));
    doc_table->set(doc_table, "year", mongory_value_wrap_i(pool, 2024));
    mongory_value *doc = mongory_value_wrap_t(pool, doc_table);

    // 4. Create the query condition
    // { "language": "C", "year": { "$gt": 2020 } }
    mongory_table *query_table = mongory_table_new(pool);
    mongory_table *year_cond_table = mongory_table_new(pool);

    // "$gt": 2020
    year_cond_table->set(year_cond_table, "$gt", mongory_value_wrap_i(pool, 2020));

    // "year": { ... }
    query_table->set(query_table, "year", mongory_value_wrap_t(pool, year_cond_table));
    // "language": "C"
    query_table->set(query_table, "language", mongory_value_wrap_s(pool, "C"));

    mongory_value *query = mongory_value_wrap_t(pool, query_table);

    // 5. Create a matcher with the query condition
    mongory_matcher *matcher = mongory_matcher_new(pool, query);

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
- `CMake` 3.12 or higher
- `gcc` or any C99-compatible compiler
- `cJSON` library

#### Installation by Platform

**macOS (Homebrew):**
```bash
brew install cmake cjson
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake libcjson-dev build-essential
```

**CentOS/RHEL/Fedora:**
```bash
# CentOS/RHEL
sudo yum install cmake cjson-devel gcc
# Fedora
sudo dnf install cmake cjson-devel gcc
```

### Quick Build (Recommended)

Use the provided build script for the easiest experience:

```bash
# Basic build
./build.sh

# Setup Unity test framework and run tests
./build.sh --setup-unity --test

# Debug build with tests
./build.sh --debug --test

# Clean rebuild with benchmarks
./build.sh --clean --benchmark

# See all options
./build.sh --help
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
