# Network Replication Benchmark Baseline

Purpose: a stable reference table so future refactors/optimizations can be compared.

Run:

```
cmake --build <build_dir> --target NetworkBenchmark --config Release
output/bin/Release/NetworkBenchmark.exe
```

Config: payload 12 bytes, iterations 2000, Win32 / MSVC / Release (single thread).

## Baseline (recorded 2026-09-25)

### Scale — opaque records, all entities change each tick

| entities | build ms/tick | apply ms/tick |
|---|---|---|
| 100 | 0.0179 | 0.0005 |
| 1000 | 0.1944 | 0.0048 |
| 5000 | 0.9780 | 0.0262 |

### Field delta — 2000 entities, 8 fields x 4 bytes

| changed fields | bytes/tick |
|---|---|
| 1 | 52,008 |
| 2 | 64,008 |
| 4 | 88,008 |
| 8 (full) | 136,008 |

### Dormancy — 2000 unchanged, revision-tracked, client acks each tick

| metric | value |
|---|---|
| build ms/tick | 0.0182 |
| bytes/tick | 8 (header only) |

Revision tracking skips encoding unchanged records: ~0.34 ms/tick (no revision, repair each tick) -> 0.018 ms/tick.

### Split — 2000 entities, all change

| maxMessageBytes | messages/tick | bytes/tick |
|---|---|---|
| 256 | 286 | 70,288 |
| 512 | 143 | 69,144 |
| 1200 | 58 | 68,464 |

### AoI — 2000 entities, cull entities divisible by N

| cullEveryN | culled | bytes/tick |
|---|---|---|
| 2 | 1000 | 34,008 |
| 4 | 500 | 51,008 |
| 8 | 250 | 59,508 |

### Multi-connection — 1000 entities, all change, per tick (shared encode cache)

| connections | no-cache ms/tick | cache ms/tick | speedup |
|---|---|---|---|
| 4 | 0.668 | 0.577 | 1.16x |
| 16 | 3.144 | 2.632 | 1.19x |
| 64 | 12.338 | 10.553 | 1.17x |

### Resume token — HMAC-SHA256

| operation | rate |
|---|---|
| issue | ~1.22 M/s |
| verify | ~1.23 M/s |

## Observations

- Delta + field masking cut bandwidth substantially (1/8 fields changed: 52 KB vs 136 KB full, -62%).
- Revision tracking turns dormancy from a bandwidth-only win into a CPU win (near-zero encode, 0.017 ms/tick).
- The shared encoding cache reduces per-connection encoding from O(C x N) to O(N), but the measured
  end-to-end speedup is only ~1.17x: the **dominant per-connection cost is the gather + sort + delta +
  payload build**, not `Encode`. The next optimization is a per-tick shared *gather* (encode and gather
  once, then per-connection only delta/split/filter), which the cache is a prerequisite for.

