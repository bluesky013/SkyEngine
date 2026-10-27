# Physics assets and streaming implementation verification

Date: 2026-10-27. Platform: Windows / VS 2022 / Debug, existing cmake-build-debug and Jolt 5.6.0. No commit or OpenSpec archive performed.

## Delivered scope

- `plugins/jolt/builder`: independent `.collision` / `.physmat` publication through the existing asset builder and atomic product-group store. The shared private codec has one compilation owner, `JoltPhysics.AssetCodec.Static`; no core target or builder dependency on runtime/adaptor/render.
- `plugins/jolt/adaptor`: unique runtime handler owner, reflected component asset fields, local shape rotation, damping on Body, immutable native Shape preparation through AssetExecutor, world-owned material handles.
- Framework: fully asynchronous product lookup/read entry, shared product encoding/commit path, component preparation hooks, incremental World cell manifest and safe-point group lifecycle.
- Scene data preserves unloaded cell definitions and all direct asset roots. Ready cells own resources but no native bodies; activation and retirement are separate operations. Dynamic/kinematic transform trees become World residents on retirement. Cross-cell constraints and current non-trigger contacts pin support cells.
- Existing SceneVerification project now contains `assets/jolt-streaming-demo.world`, `streaming-box.collision`, `streaming-plane.collision`, and `streaming-rubber.physmat`. The checked-in physics-only counterpart lives in `plugins/jolt/test/assets/`.

## Validation evidence

| Check | Result / evidence |
|---|---|
| Full Debug build | Passed; `build_3rd/physics-streaming-full-build.log` |
| Jolt unit/integration suite | 58 passed, including malformed-manifest rejection; `build_3rd/physics-streaming-jolt.log` |
| Framework | 208 passed; `build_3rd/physics-streaming-framework.log` |
| EditorCore / EditorShell | 184 / 144 passed; `build_3rd/physics-streaming-editor-{core,shell}.log` |
| Real DLL lifecycle | Passed two sessions, one metadata owner, retained worlds and render provider; `build_3rd/physics-streaming-module-lifecycle.log` |
| Recast module | 2 passed; `build_3rd/physics-streaming-recast.log` |
| Final targeted Framework regression | 36 passed after final cell-envelope changes; `build_3rd/physics-streaming-final-framework.log` |
| Terrain physics bridge | 2 passed; `build_3rd/physics-streaming-terrain.log` |
| Python package regressions | 8 passed; `build_3rd/physics-streaming-python-tests.log` |
| Existing-project offline cook | 18 products, 0 failures; loads JoltPhysics.Builder without JoltPhysicsModule; `build_3rd/physics-streaming-project-cook.log` |
| Native product closure audit | Passed with source access disabled; three physics roots in existing project products; `build_3rd/physics-streaming-package-audit.log` |
| Editor bootstrap | Existing project, 30 frames, exit 0; builder/runtime registration and Vulkan startup passed. This is startup smoke, not interactive authoring-tool validation. |
| OpenSpec | `openspec validate add-physics-collider-authoring --strict` passed |

The saved-project test cooks all three fixture sources, reads the serialized World in a fresh World, observes 6 bodies in the initial Active cell and no hit in the other Ready cell, then activates its independent triangle mesh and observes 7 bodies and the expected query hit.

The repeated-paging fixture runs 20 load/activate/unload cycles with two bodies sharing one Collision. Every active cycle has one shared prepared resource; every retired cycle returns resource/task/source-byte counters to zero. The first full parallel regression run recorded 940 ms across those 20 small-fixture cycles. This is not a large-scene performance benchmark or a measurement of total native/process memory.

On 2026-10-27, five opt-in `PhysicsStreamingBenchmark` cases were added using the same test executable and fixture. The final Debug build passed; ordinary selection retained 58 passing correctness tests and skipped the five benchmarks, while explicit selection executed all five successfully in 72.688 seconds. Workloads, phase timing, waiting overhead, sampled residency counters, invocation and limits are recorded in [benchmarks.md](benchmarks.md). This supplies reproducible benchmark cases; it does not close large-scene/native-memory or optimized/shipping-platform measurements.

## Issues corrected during implementation

1. Existing LoadAsset queued decoding but read and validated product bytes synchronously. Calling it directly from streaming preparation would still stall the host. Added LoadAssetAsync using the existing AssetExecutor and unchanged product loader; callers poll a publication-safe request.
2. Retirement originally consulted the last published event snapshot. A paused-world teleport erased current manifolds but left that snapshot until the next Step, pinning the old support forever. Retirement now consults live manifolds; a regression moves the dependent body away while paused and verifies unload completes.
3. Initial scene cloning used the raw MemoryArchive with JSON. Its byte/peek interface is not the stream archive contract, so JSON parsing failed. Cloning now uses the established OMemoryArchive/IMemoryArchive stream wrappers; the full serialized fixture exercises this path.
4. Cook-all treated `.world` manifests as product sources and failed because no World builder exists. They are now recognized as source scene manifests; their referenced physics products use normal builders and closure audit.
5. Prepared-resource budget ownership now lasts until the prepared Shape itself is released, including body-held references, rather than ending when a request consumer releases its ticket.

## Deliberate remaining scope

- No dedicated Collision asset window, viewport handles, automatic box/sphere/capsule fitting, DCC helper import or CoACD decomposition in this slice. Component bindings use the existing reflected inspector.
- Geometry products are portable validated CPU geometry. Native Jolt construction is asynchronous at runtime; native ABI-bound cooked blobs remain a future measured decision.
- WorldStreaming is a bounded explicit host, not a spatial WorldPartition scheduler. Bounds generation, player/projectile coverage, prefetch/unload hysteresis, automatic teleport gating and Terrain pager integration remain host-policy work. The API exposes Ready/Active/Failed/Retiring for those decisions.
- Legacy inline render-mesh references remain compatible with ordinary loading. Streaming requires independent Collision products; it explicitly rejects an unresolved legacy mesh instead of synchronously loading render dependencies.
- This verifies Windows Debug and the existing-project product closure, not a packaged shipping application on every platform. Large-scene native-memory/latency profiling remains open.

## Host usage

```cpp
#include <framework/world/WorldStreaming.h>

// The cell identities and actor definitions are loaded from the World manifest.
auto &streaming = world.GetStreaming();
streaming.Request(destinationCell, sky::WorldCellTarget::Ready);
// World::Tick advances loading and bounded safe-point work.
if (streaming.GetState(destinationCell) == sky::WorldCellState::Ready)
    streaming.Request(destinationCell, sky::WorldCellTarget::Active);
// Gate teleport/spawn until GetState(destinationCell) == Active.
// Request(Unloaded) may remain Retiring while support/constraints pin the cell.
```

PhysicsBody inspector: select Asset as shapeSource, assign collisionAsset and materialAsset UUIDs through the existing asset fields. Empty materialAsset selects the default. Cook the `.collision`/`.physmat` source before runtime ProductsOnly loading. `initialTarget: 1` preloads a cell; `initialTarget: 2` activates it as soon as its whole activation group is prepared.
