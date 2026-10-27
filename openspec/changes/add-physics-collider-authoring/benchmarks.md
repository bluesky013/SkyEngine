# Physics asset and streaming benchmarks

The cases live in `plugins/jolt/test/PhysicsAssetStreamingTest.cpp` and reuse its asset builder, temporary product bundle, metadata owner and World fixture. No additional executable, production target or implementation-source compilation was introduced.

## Run

From the repository root in PowerShell, after closing SandboxEditor:

```powershell
cmake --build cmake-build-debug --config Debug --parallel --target JoltPhysicsTest
New-Item -ItemType Directory -Force build_3rd | Out-Null
$benchmarkOutput = Join-Path (Get-Location) 'build_3rd/physics-streaming-benchmark.json'
& ./output/bin/Debug/JoltPhysicsTest.exe --gtest_also_run_disabled_tests '--gtest_filter=PhysicsStreamingBenchmark.*' "--gtest_output=json:$benchmarkOutput"
```

The `DISABLED_` names are intentional opt-in benchmarks, not failing or unfinished tests. The ordinary executable/CTest run continues to execute the 58 existing correctness tests without this additional workload. Select one benchmark with a narrower gtest filter; each invocation should start a fresh process. For optimized comparisons build the same target with `--config Release` and run the Release executable. Do not compare those values directly with the Debug baseline below.

## Workloads

| Case (under PhysicsStreamingBenchmark) | Workload | Additional checks |
| --- | --- | --- |
| DISABLED_SharedCollisionAndMaterialPaging | 4 cells × 64 static bodies; one Box Collision and one PhysicsMaterial; 20 full residency cycles | One prepared resource for 256 bodies, complete release each cycle |
| DISABLED_UniqueCollisionPaging | 4 cells × 32 static bodies; 128 separate Sphere products; 5 cycles | 128 simultaneous prepared resources, complete release |
| DISABLED_ResidentShapeReuseAcrossCells | Same Box/material and 256-body workload as shared paging; 20 cycles; one additional Ready anchor | Exactly one resource survives workload retirement; final anchor retirement releases it |
| DISABLED_TriangleMeshPaging | One 128 × 128 grid: 16,641 vertices / 32,768 triangles; 4 cells × 16 static instances; 5 cycles | Independent cooked mesh product, shared native preparation and query hit |
| DISABLED_CancelAdmittedPreparationAndReload | One 64 × 64 grid: 4,225 vertices / 8,192 triangles; 16 cells × 16 bodies; 10 preparation cancellations then 2 full reload cycles | Wait until at least one native preparation resource is admitted, drain cancelled work, reject leaked/stale bodies on reload |

Cancellation records how many cycles still had a native task pending when cancelled. Worker completion is timing-dependent; the benchmark does not pretend every cancelled preparation was still running.

All workloads pause simulation. These measure asset/World streaming, not solver/contact throughput, moving-body residency policy or render performance.

## Metrics and interpretation

- `prepare_*`: request Ready through actor cloning, asynchronous product consumption and native Shape readiness. It is an end-to-end phase, not an isolated native cook timer.
- `activate_*`: Ready to Active, using the existing bounded safe-point commit path.
- `retire_and_drain_*` / `cancel_and_drain_*`: retirement/cancellation and completion of outstanding asset tasks.
- `*_wall_*`: nearest-rank p50/p95, maximum, total and sample count across phase executions. Small sample counts (mesh/unique/reload cases) make p95 an exploratory statistic, often equal to the maximum.
- `*_host_tick_*`: timing only around World::Tick(0); assertions, statistics collection, explicit polling sleep and final worker drain are outside this timer.
- `*_poll_wait_ms` and `*_worker_drain_ms`: measured cumulative waiting time. Requested polling sleep is 1 ms, but the OS may sleep substantially longer. Wall time also includes harness statistics/state inspection; it must not be interpreted as pure CPU work or latency at a 60 Hz host cadence.
- `fixture_publish_*`: source-file write plus builder/product publication before runtime timing. Source geometry generation and source encoding are excluded.
- `first_prepare_wall_ms`: first residency measurement in this process. Fixtures have just been cooked; OS file caches are uncontrolled, so this is not a cold-disk benchmark.
- `sampled_peak_*`: bodies, prepared resources, pending preparation tasks and accounted Shape source bytes, sampled after host ticks. These counters exclude native allocator overhead, asset-manager metadata and total process memory; brief worker-only peaks may fall between samples.

Every cycle also checks body/actor counts, no query visibility in Ready, query visibility in Active, and no residual bodies/tasks after retirement. Fully retired workloads require zero prepared resources and zero accounted source bytes. No absolute timing threshold is used as a correctness gate.

## Baseline

Measured on 2026-10-27: Windows / Visual Studio 2022 / Debug, Intel Core i7-12700KF, existing Jolt 5.6.0 dependency bundle. Ordinary regression: **58 passed / 5 benchmarks skipped**, 5.330 seconds. Explicit benchmark selection: **5 executed and passed**, 72.688 seconds. The JSON reports disabled=5 because of the opt-in names; every selected case has status=RUN and result=COMPLETED.

| Case | Prepare wall p50 (ms) | Prepare host Tick p95 (ms) | Activate host Tick p95 (ms) | Retire host Tick p95 (ms) | Peak shapes / accounted source bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Shared Collision + material | 1214.408 | 26.869 | 3.887 | 2.220 | 1 / 216 |
| Unique Collisions | 1034.275 | 26.572 | 1.912 | 1.626 | 128 / 27,648 |
| Resident reuse | 1202.088 | 27.079 | 3.769 | 2.367 | 1 / 216 |
| Triangle mesh | 728.466 | 0.421 | 1.017 | 0.853 | 1 / 593,124 |
| Reload after cancellation | 1288.082 | 26.692 | 1.023 | 0.823 | 1 / 149,220 |

The cancellation case observed pending native tasks in 10/10 cancellation cycles. Its cancel-and-drain wall p50 was 90.975 ms; the host cancellation Tick p95 was 0.185 ms. That wall value includes waiting for admitted work to finish, not synchronous shape cooking on the host.

For shared paging, the 20 preparation cycles included 19,249.595 ms of measured polling sleep. Keep wall latency separate from host Tick cost. The approximately 27 ms preparation-Tick p95 persists in the resident-reuse case; the measurement identifies a profiling target, not a proven implementation root cause. The triangle-mesh case has smaller cell batches and should not be compared as an equal workload.

Local evidence:

- `build_3rd/physics-streaming-benchmark-build.log`
- `build_3rd/physics-streaming-benchmark-regression.json` and `.log`
- `build_3rd/physics-streaming-benchmark-debug-final.json` and `.log`

Optimized builds, allocator/process memory and larger sustained scene workloads remain separate measurements. No production performance fix, commit or archive was made by this benchmark addition.
