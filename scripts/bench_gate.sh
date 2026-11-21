#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/bin/benchmark_bench_suite}"
RUNS="${MONGORY_BENCH_RUNS:-3}"

if [[ ! -x "$BIN" ]]; then
  echo "Benchmark binary not found or not executable: $BIN" >&2
  exit 1
fi

python3 - "$BIN" "$RUNS" <<'PY'
import re
import statistics
import subprocess
import sys

binary = sys.argv[1]
runs = int(sys.argv[2])

# Conservative ratio ceilings (mongory_ns / plain_ns) resilient to CI runner noise,
# while still catching accidental O(n) / allocation regressions.
THRESHOLDS = {
    "simple": 3.0,
    "or": 4.5,
    "nested_and": 3.5,
    "in_array_field": 3.5,
    "large_in_500": 2.5,
}

line_re = re.compile(
    r"^(\S+)\s+plain=\s*([0-9.]+)\s+ns/rec\s+mongory=\s*([0-9.]+)\s+ns/rec\s+"
    r"\(ratio=([0-9.]+)x,\s+matched=(\d+)/(\d+)\)$"
)

ratios = {k: [] for k in THRESHOLDS}

for run in range(1, runs + 1):
    out = subprocess.check_output([binary], text=True)
    print(f"=== Run {run}/{runs} ===")
    print(out.strip())
    seen = set()
    for line in out.splitlines():
        m = line_re.match(line.strip())
        if not m:
            continue
        name, _, _, ratio_s, matched_s, plain_matched_s = m.groups()
        if int(matched_s) != int(plain_matched_s):
            print(f"FAIL: {name} match count mismatch: {matched_s} != {plain_matched_s}", file=sys.stderr)
            sys.exit(1)
        if name in ratios:
            ratios[name].append(float(ratio_s))
            seen.add(name)
    missing = set(THRESHOLDS) - seen
    if missing:
        print(f"FAIL: missing benchmark cases in output: {sorted(missing)}", file=sys.stderr)
        sys.exit(1)

print("\n=== Benchmark Gate Summary (median over runs) ===")
failed = False
for name, limit in THRESHOLDS.items():
    med = statistics.median(ratios[name])
    status = "PASS" if med <= limit else "FAIL"
    print(f"{status}: {name:16s} median_ratio={med:.2f}x (limit <= {limit:.2f}x)")
    if med > limit:
        failed = True

if failed:
    sys.exit(1)
PY
