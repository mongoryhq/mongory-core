# Performance & Benchmarks

## Reproducible Benchmarks

Run the benchmark suite in `mongory-core`:

```bash
bash scripts/bench.sh
```

> Core library has zero external dependencies. Tests/benchmarks fetch `yyjson` and Unity automatically (`./scripts/setup_yyjson.sh` and `./build.sh --setup-unity`).

### Measured Results (Apple M3, macOS arm64, `Release` build, 10,000 records x 200 rounds, median of 7 samples)

| Case | Condition | Plain C | `mongory-core` | Ratio |
|---|---|---|---|---|
| `simple` | `{ "age": { "$gte": 18 } }` | `8.8 ns/rec` | `12.9 ns/rec` | `1.48x` |
| `or` | `{ "$or": [{ "age": { "$gte": 18 } }, { "status": "active" }] }` | `12.0 ns/rec` | `28.4 ns/rec` | `2.37x` |
| `nested_and` | `{ "profile": { "city": "taipei" }, "status": "active", "age": { "$lt": 40 } }` | `19.6 ns/rec` | `32.5 ns/rec` | `1.66x` |
| `in_array_field` | `{ "tags": { "$in": ["go", "rust"] } }` | `20.2 ns/rec` | `31.1 ns/rec` | `1.54x` |
| `large_in_500` | `{ "name": { "$in": [...500 items] } }` (20% hit rate; O(1) hash vs plain C `strcmp` scan) | `1074.4 ns/rec` | `33.2 ns/rec` | `0.03x` (32.4x faster) |

> Note on `large_in_500`: The plain C baseline scans all 500 strings with `strcmp`, which exits on the first differing byte when prefixes do not match (`other_*` vs `user_*`). On datasets with higher shared-prefix rates, plain C linear scans slow down further while Mongory's O(1) hash lookup stays flat.

> For cross-language benchmarks, see [Mongory Central](https://mongoryhq.github.io/mongory-central/#benchmarks).
