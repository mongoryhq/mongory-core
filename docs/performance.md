# Performance & Benchmarks

## Reproducible Benchmarks

Run the benchmark suite in `mongory-core`:

```bash
bash scripts/bench.sh
```

> Core library has zero external dependencies. Tests/benchmarks fetch `yyjson` and Unity automatically (`./scripts/setup_yyjson.sh` and `./build.sh --setup-unity`).

### Measured Results (Apple M3, macOS arm64, `Release` build, 10,000 records x 200 rounds)

| Case | Condition | Plain C | `mongory-core` | Overhead |
|---|---|---|---|---|
| `simple` | `{ "age": { "$gte": 18 } }` | `17.3 ns/rec` | `17.6 ns/rec` | `1.02x` |
| `or` | `{ "$or": [{ "age": { "$gte": 18 } }, { "status": "active" }] }` | `13.5 ns/rec` | `32.6 ns/rec` | `2.42x` |
| `nested_and` | `{ "profile": { "city": "taipei" }, "status": "active", "age": { "$lt": 40 } }` | `22.2 ns/rec` | `36.3 ns/rec` | `1.64x` |
| `in_array_field` | `{ "tags": { "$in": ["go", "rust"] } }` | `20.5 ns/rec` | `32.9 ns/rec` | `1.60x` |
| `large_in_500` | `{ "name": { "$in": [...500 items] } }` (linear scan) | `1240.4 ns/rec` | `1580.0 ns/rec` | `1.27x` |

> For cross-language benchmarks, see [Mongory Central](https://mongoryhq.github.io/mongory-central/#benchmarks).
