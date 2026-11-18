#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -d tests/unity ]]; then
  ./build.sh -u >/dev/null
fi
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON -S . >/dev/null
cmake --build build --target benchmark_bench_suite >/dev/null
./build/bin/benchmark_bench_suite
