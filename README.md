# Mongory Core

[![Release](https://img.shields.io/github/v/release/mongoryhq/mongory-core)](https://github.com/mongoryhq/mongory-core/releases)
[![API Docs](https://img.shields.io/badge/API%20Docs-Doxygen-blue)](https://mongoryhq.github.io/mongory-core/)
[![CI](https://github.com/mongoryhq/mongory-core/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/mongoryhq/mongory-core/actions/workflows/cmake-multi-platform.yml)
[![mongory-spec](https://github.com/mongoryhq/mongory-core/actions/workflows/spec-conformance.yml/badge.svg)](https://github.com/mongoryhq/mongory-core/actions/workflows/spec-conformance.yml)
![C99](https://img.shields.io/badge/C-C99-555555)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**Zero-dependency C99 core engine for in-memory MongoDB-style query matching and rule evaluation.** Built on arena memory pools with OOM NULL-safety, it powers [`mongory-rb`](https://github.com/mongoryhq/mongory-rb) and [`mongory-py`](https://github.com/mongoryhq/mongory-py) and is verified against [`mongory-spec`](https://github.com/mongoryhq/mongory-spec).

> **Ecosystem:** [Interactive Playground ↗](https://mongoryhq.github.io/mongory-central/) · [`mongory-rb` (Ruby)](https://github.com/mongoryhq/mongory-rb) · [`mongory-py` (Python)](https://github.com/mongoryhq/mongory-py) · [`mongory-go` (Go)](https://github.com/mongoryhq/mongory-go) · [`mongory-js` (TypeScript/JS)](https://github.com/mongoryhq/mongory-js) · [`mongory-spec`](https://github.com/mongoryhq/mongory-spec)

## Quick Start

### Option A: Pure C99 API (zero dependencies)

```c
#include <stdio.h>
#include <mongory-core.h>

int main(void) {
    mongory_init();
    mongory_memory_pool *pool = mongory_memory_pool_new();

    mongory_value *doc = MG_TABLE_WRAP(pool, 3,
        "name",     mongory_value_wrap_s(pool, "Jules"),
        "language", mongory_value_wrap_s(pool, "C"),
        "year",     mongory_value_wrap_i(pool, 2024)
    );

    mongory_value *query = MG_TABLE_WRAP(pool, 2,
        "language", mongory_value_wrap_s(pool, "C"),
        "year", MG_TABLE_WRAP(pool, 1, "$gt", mongory_value_wrap_i(pool, 2020))
    );

    mongory_matcher *matcher = mongory_matcher_new(pool, query, NULL);
    bool is_match = mongory_matcher_match(matcher, doc);
    printf("%s\n", is_match ? "match" : "no match");

    pool->free(pool);
    mongory_cleanup();
    return 0;
}
```

### Option B: Direct Raw JSON Filtering (`-DMONGORY_WITH_JSON=ON`)

Build with `-DMONGORY_WITH_JSON=ON`, include `<mongory-core/adapters/json.h>`, and link `mongory-core::json` (backed by vendored `yyjson`) to compile JSON conditions and match raw JSON strings via shallow adapters without full `mongory_table` materialization (`mongory_matcher_new_from_json`, `mongory_matcher_match_json`, `mongory_matcher_match_json_with_ctx`).

## Why Mongory Core? (and Limitations)

- **Zero runtime dependencies**: portable C99 (`gcc`/`clang`/`MSVC`), builds cleanly as a static or shared library with CMake and `pkg-config`.
- **Arena memory pools**: all allocations live inside a `mongory_memory_pool` with configurable chunk/total byte caps (`mongory_memory_pool_set_limits`) and full OOM NULL-safety (verified under ASan/LSan/UBSan/TSan + libFuzzer).
- **Performance**: priority-ordered short-circuiting, immutable type-bucketed O(1) hash lookup for `$in`/`$nin` (`>= 16` items), and shallow adapter interfaces so host runtimes (`PyObject*`, Ruby `VALUE`, `yyjson`) avoid deep copying.
- **Limitations**: `mongory-core` is a pure predicate matcher (no aggregation pipeline, no storage engine) and is regex-engine agnostic: callers supply a regex callback (e.g., PCRE2, Oniguruma, RE2, or host-language regex) when `$regex` is needed.

## Performance Snapshot

- **Raw JSON shallow adapter (`yyjson`)**: filters wide 20+ field JSON records in **`207.0 ns/rec`**, **`6.30x` faster** than eager full conversion (`deep_convert`).
- **O(1) `$in` / `$nin` hash lookup (`v0.3.1`)**: accelerates 500-item `$in` in downstream C bindings by **~15x** (`115.8 ns/rec` in Ruby, `111.6 ns/rec` in Python).

See [docs/performance.md](docs/performance.md) and [`scripts/bench.sh`](scripts/bench.sh) for full C99 benchmark results.

## Explain & Trace

`mongory-core` provides both structured tree APIs (returning `mongory_value *` trees allocated in your arena pool) and stdout helpers:

```c
// Structured explain tree: { "name", "condition", "priority", "children", ["field"] }
mongory_value *explain_node = mongory_matcher_explain_tree(matcher, pool);
mongory_matcher_explain(matcher, pool);

// Structured trace tree: { ..., "result": "pass"|"fail"|"skip", "children", ["record"] }
mongory_value *trace_node = mongory_matcher_trace_tree(matcher, doc);
bool matched = mongory_matcher_trace(matcher, doc);
```

## Building, Installing & Testing

### Quick Build

```bash
# Core library only (zero external dependencies: C99 compiler + CMake >= 3.12)
./build.sh

# First-time setup for Unity + yyjson and run unit tests
./build.sh --setup-unity --test

# Full unit tests + mongory-spec conformance (requires libpcre2)
./build.sh --setup-unity
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=OFF -DMONGORY_SPEC_DIR=../mongory-spec
cmake --build build
ctest --test-dir build --output-on-failure
```

### Build Options & Consuming via CMake / `pkg-config`

```bash
# Shared library + CMake config + pkg-config install
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF -DCMAKE_INSTALL_PREFIX=/usr/local -S .
cmake --build build && sudo cmake --install build

# Optional raw JSON shallow-wrapper adapter (mongory-core::json)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMONGORY_WITH_JSON=ON -S .
cmake --build build

# Sanitizers (GCC/Clang)
cmake -B build-san -DMONGORY_SANITIZERS=address,undefined -S .
```

```cmake
find_package(mongory-core 0.3 REQUIRED)
target_link_libraries(my_app PRIVATE mongory-core::mongory-core)
```

```bash
cc main.c $(pkg-config --cflags --libs mongory-core)
```

<details>
<summary>Platform prerequisites &amp; formatting</summary>

- **macOS (Homebrew)**: `brew install cmake pcre2`
- **Ubuntu/Debian**: `sudo apt install -y cmake build-essential libpcre2-dev pkg-config`
- **Fedora/RHEL**: `sudo dnf install cmake pcre2-devel gcc`
- **Code formatting**: `cmake --build build --target format` (`clang-format`). Public headers in `include/` carry Doxygen API comments. See `CMAKE_USAGE.md` for additional CMake options.

</details>

## Safety, Limits & ReDoS Protection

- **Memory Pool Limits**: Geometric chunk growth is capped at `MONGORY_DEFAULT_MAX_CHUNK_SIZE` (4 MiB). Use `mongory_memory_pool_set_limits(pool, max_chunk_size, max_total_bytes)` to enforce a per-pool memory budget, or provide a custom `mongory_memory_pool` vtable (`alloc`/`trace`/`reset`/`free`). Allocation failures report `MONGORY_ERROR_MEMORY` via `&MONGORY_ALLOC_ERROR` without dereferencing `NULL`.
- **Compile Limits (`MaxDepth` / `MaxNodes`)**: Defaults `64` / `4096` (configurable via `mongory_max_depth_set` / `mongory_max_nodes_set`; `0` = unlimited). Exceeding either limit fails cleanly with `MONGORY_ERROR_LIMIT_EXCEEDED`.
- **External Regex Adapter ReDoS Guidance**: When evaluating untrusted patterns with a backtracking engine, configure match/backtrack limits (e.g., `pcre2_set_match_limit` + `pcre2_set_depth_limit` as in [`tests/conformance/mongory_spec_conformance.c`](tests/conformance/mongory_spec_conformance.c), or `onig_set_retry_limit_in_match`), or prefer linear-time RE2.

## License

MIT
