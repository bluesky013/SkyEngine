# Verification: sandbox-viewport-picking

Scope: original phase A/B acceptance covered 35 tasks on 2026-10-23. The geometry extension and final Overlay review bring completion to 39/39. GPU phase C remains a future design and `VisibleSurface` returns Unsupported.

## Final Overlay review (2026-10-23)

Overlay painting and picking now share priority and stable target ordering. The winner is painted last; screen-center error and world distance no longer reorder Overlay ties independently of the visual stack. World-surface and depth-tested proxy arbitration are unchanged. The regression covers reversed provider registration, different priorities, equal priority with different screen/depth distances, priority changes, and locked occluders.

The final EditorRender suite passed 47/47 in both Develop and Debug; native Vulkan dynamic/render-pass plus DX12 acceptance passed 9/9 in each configuration. Finalization logs/XML are `output/pick-overlay-final-{develop,debug}.*` and `output/pick-overlay-native-{develop,debug}.*`.

An initial Debug run and isolated retry exposed two time-sensitive performance fixtures: a warm query asserted immediate Hit despite the documented Pending contract, and the 100,000-instance setup hit its ten-second fixture timeout. Warm measurement now resumes the same query until completion and records total elapsed time; large-scene setup has a separate sixty-second timeout. Exact Hit/ResourceLimit and structural counter assertions remain. No production query budget was increased. The final Debug suite passed with these test corrections; earlier performance distributions remain historical measurements, not fresh claims.

## Geometry query extension follow-up (2026-10-23)

Implemented `IViewportPickGeometry::Raycast`, provider-owned cancellation cursors, cooperative work/time budgets and custom world geometry entries in the existing provider snapshot. Existing EditorRender composes their bounded AABB screening and exact surface results with mesh/proxy selection. No business geometry backend, target, module, RHI change or new BVH algorithm was introduced. The test provider uses an analytic plane; production providers remain responsible for their own geometry/index/cursor allocation and budget accounting.

Nine new regressions cover bounded continuation; cancellation, request replacement and document changes; snapshot revisions and unregister; cursor/query destruction before module lease release; bounds pruning, locked/ignored occlusion, geometry-only/visible-surface policy, mixed mesh/provider priority, malformed results/metadata and capacity. Provider first-null access violation and recovery to the same prior snapshot are repaired. Mesh budget rejection now invalidates waiting traversal state and returns Error/ResourceLimit through Submit/Poll instead of permanent Pending.

| Suite | Develop | Debug |
| --- | --- | --- |
| EditorCoreTest | 177/177 | 177/177 |
| EditorShellTest | 140/140 | 140/140 |
| EditorRenderTest | 45/45 | 45/45 |
| Existing native Vulkan dynamic / render pass / DX12 acceptance | 6/6 | 6/6 |
| Total | 368/368 | 368/368 |

SandboxModule and affected tests compile in Develop/Debug. The native executable was rebuilt against the extended header; these six native cases retain previous scene/proxy acceptance, while the custom geometry extension is exercised by the CPU fixtures. Logs/XML are `output/pick-extension-{Develop,Debug}-<suite>.*` and `output/pick-extension-native-{Develop,Debug}.*`. OpenSpec strict validation and `git diff --check` pass. No fresh performance or other-platform claim is made.

Third-party preflight (`python python/third_party.py -p Win32 -o build_3rd --check`) reports seven existing metadata/version issues: stale miniaudio/enet/slang and missing metadata for vulkan_headers/nanosvg/openssl/cpython, with zero output-missing entries. Dependencies were not changed; the affected builds and tests above pass using the current cache. Full preflight log: `output/pick-extension-third-party.log`.

At the geometry-extension milestone, the Overlay mismatch remained open outside the interface-only request. The final review above closes it; the earlier 35/35 acceptance result remains historical evidence.

## Phase B build and correctness

Windows x64, VS 2022, Intel Core i7-12700KF, NVIDIA RTX 3080; existing `cmake-build-debug`, third-party root `build_3rd/Win32`. SandboxEditor was closed before configuring/building. SandboxModule and SandboxEditor build in both Develop and Debug. Debug GPU validation is enabled; final native logs have no Vulkan validation errors and the DX12 info queue has no error/corruption messages.

| Suite | Develop | Debug |
| --- | --- | --- |
| CoreTest | 311/311 | 311/311 |
| EditorCoreTest | 177/177 | 177/177 |
| EditorShellTest | 140/140 | 140/140 |
| EditorRenderTest | 36/36 | 36/36 |
| Aurora.AdaptorTest | 36/36 | 36/36 |
| UICoreTest | 190/190 | 190/190 |
| EditorHostNativeTest: Vulkan dynamic / render pass / DX12 | 6/6 | 6/6 |

Each configuration passes 890 CPU/UI/adaptor tests plus 6 native tests. New assertions include:

- Binned BVH vs exhaustive bounds; resumable traversal; cancellation before build and during bounds preparation with no partial publication; existing Core BVH consumers.
- 4096 randomly generated instance bounds with 768 rays across movement/removal/reinsertion, compared with an independent exhaustive oracle. Another 512 rays compare transformed triangles against an analytic world-space oracle, including negative and nonuniform scales.
- Locked/filter-excluded surfaces still occlude; ignored Actors do not. Unknown bounds, invalid/non-affine transforms, tied surfaces, unsupported coverage, stale/duplicate tokens, cancellation, eight-view fairness/capacity and cache-pressure retry are explicit.
- 10k-instance sparse deltas update exactly 100 inverses without rebuilding shared mesh geometry; camera-only updates do not touch the world index. Interrupted/lost deltas resynchronize; source readiness can advance without expiring the semantic request.
- Provider registration identity/capacity, ownership, module lease destruction order, unregister with outstanding requests, rectangle and near-clipped segment layout, triangle proxies, equal-depth provider resolution, projection-cache reuse and view invalidation.
- All three native paths use a real authored material through SceneAssetSource and actual rendering; explicit capabilities enable picking, material invalidation preserves the mesh identity, and subsequently unsupported coverage preserves selection. The floating Shell selects a visible non-mesh icon through SelectionService and retires it after provider removal. World dirty/undo remain unchanged. The fixture uses `ModuleManager::RetainModule` for a module-declared provider.

The initial sandboxed filesystem regressions failed with Access denied in the sandbox temp directory. The unchanged Core/editor suites pass outside that sandbox. A separate real adaptor failure exposed repeated readiness updates for newly allocated owners with identical immutable resource keys; the implementation now retains the existing owned resource and its no-change snapshot, and that regression passes.

## Phase B measurements

Measurements are elapsed CPU times in Develop, with no concurrent builds. They do not include asset/shader loading in the synthetic geometry fixture and are not renderer frame times.

Twenty independent cold processes each build 20k triangles shared by 200 instances, then execute 1000 warm queries. The pre-change 20-run fixture had total-test p50/p95/max 363/366/367 ms; its last worker-build sample was 335.216 ms (the earlier phase A sample below was 358.975 ms). The old repeated run retained only its final worker property, so it does not supply a cold-worker percentile distribution.

| 20k triangles / 200 instances | p50 | p95 | max |
| --- | --- | --- | --- |
| Worker cold build, 20 samples (ms) | 19.529 | 20.910 | 22.249 |
| Per-run warm-query p95, 20 x 1000 queries (ms) | 0.02445 | 0.03980 | 0.04110 |

Warm median is about 0.0224 ms; largest individual warm sample is 0.1623 ms. The warm 200-instance workload is approximately unchanged from phase A; the primary improvements are cold construction and large-scene broad-phase work.

Cold stage medians: decode 0.551 ms, bounds/centroids 0.645 ms, binned partition 18.066 ms, BVH copy/node reservation 0.165 ms, host publication 0.0094 ms. Triangle allocation is included in decode and scratch allocation in bounds; these timers are not a general allocator profiler. Partitioning remains the largest CPU component. Resident mesh capacity is 2,240,000 bytes; conservative mesh preparing/resident peak is 4,720,000 bytes. Maximum sampled process peak working set is 11,538,432 bytes (11.0 MiB).

Five independent large-scene runs each execute 1000 sparse queries, 1000 camera-only preparations, 800 completions across eight request slots and 20 batches of 100 transform updates:

| Workload | Median run | Worst run / sample |
| --- | --- | --- |
| 10k sparse queries, p50 (ms) | 0.0029 | 0.0041 |
| 10k sparse queries, p95 (ms) | 0.0042 | 0.0053 |
| Individual sparse query max (ms) | 0.0173 | 0.0387 |
| 10k scene, 1% transform batch, p50 (ms) | 0.2801 | 0.2884 |
| 10k scene, 1% transform batch, p95 (ms) | 0.3823 | 0.4275 |
| 10k completely overlapping query, total CPU (ms) | 5.7001 | 5.9114 |
| Overlap query continuation slices | 6 | 6 |
| Eight-view CPU completion p50 (ms) | 0.0200 | 0.0203 |
| Eight-view CPU completion p95 (ms) | 0.0334 | 0.0402 |
| Eight-view individual completion max (ms) | 0.0496 | 0.0676 |

Sparse queries visit 2 candidate leaves on average (two-pass stable tie resolution), well below the 5% target. Index resident estimate is 7,351,920 bytes. Sparse-update timing includes the completion query and all maintenance slices, but excludes fixture snapshot cloning and OS frame pacing. Camera-only preparations produce zero world-index/inverse updates; p50 is below the recorded 0.0001 ms timer resolution, p95 is 0.0001 ms and maximum 0.0011 ms. Eight-view timings include submission, preparation and round-robin Poll completion, with no renderer/frame pacing; all 800 requests per run complete and are cancelled/released.

The 100k scene explicitly returns ResourceLimit under the default 32 MiB instance budget; recorded resident estimate is 25,133,775 bytes, with remaining budget reserved for node growth and transient overhead. This is a capacity result, not a claim of 100k query throughput. Maximum large-fixture process peak working set is 61,517,824 bytes (58.7 MiB), including its source snapshot. It is not GPU residency or a process-wide limit for the editor.

The native host records seven scripted click completions per backend, including cold mesh, warm mesh, floating viewport, misses, provider icon and unsupported material. These small mixed samples include host polling/frame pacing and are acceptance evidence, not a broad latency benchmark:

| Configuration / backend | clicks | p50 ms | p95 ms | max ms |
| --- | --- | --- | --- | --- |
| Develop / Vulkan dynamic | 7 | 2.475 | 29.671 | 29.671 |
| Develop / Vulkan render pass | 7 | 3.100 | 31.040 | 31.040 |
| Develop / DX12 | 7 | 15.152 | 45.079 | 45.079 |
| Debug / Vulkan dynamic | 7 | 3.196 | 32.120 | 32.120 |
| Debug / Vulkan render pass | 7 | 3.474 | 31.386 | 31.386 |
| Debug / DX12 | 7 | 15.169 | 45.913 | 45.913 |

Raw evidence lives under `output/pick-hardening-measurements/`: `cold-1.xml` through `cold-20.xml`, `large-1.xml` through `large-5.xml`, per-process logs, peak-working-set JSON, and `cold-summary.json` / `large-summary.json`. Final suite XML/logs are `output/pick-hardening-{Develop,Debug}-<suite>.*`; native evidence is `output/pick-hardening-native-{Develop,Debug}.*`.

## Phase B review and implemented limits

- Existing EditorCore / EditorRender / Aurora adaptor targets and dependency directions remain. No new module, plugin, load entry, schema, directory relocation, exception path or RTTI downcast was added. Existing unrelated exceptions/RTTI were not migrated. No additional renderer event consumer or GPU readback/wait path was introduced.
- Dynamic instance height balancing and bounded resynchronization replace the proposed background second tree/overlay. Oracle and sparse-update evidence justify deferring that extra ownership/publication mechanism. A partial index produces Pending, never a false Miss. Provider proxies use a bounded per-view 2D AABB cache and shared draw/hit projection.
- Default limits: 64 MiB / 128 mesh entries including worker scratch/payload reservation; 32 MiB instance/index budget including conservative container/growth charges; 16 MiB provider/projection envelope with 32 registrations, 4096 total retained proxy slots and 8 view caches; 8 outstanding view requests, three filters each limited to 1024 Actors; 256 parts / 512 payloads per instance. These conservative limits fail explicitly and preserve selection. Source cache's existing 256 MiB limit remains separate; owned source snapshots can outlive source-cache eviction, so it is not a hard total-process memory cap.
- Host maintenance/query slices target 1 ms with bounded continuation, not a hard real-time deadline. Provider Snapshot callbacks must themselves return bounded owned data without blocking. Latest requests immediately replace old CPU cursors; shared geometry jobs remain coalesced. Module provider/snapshot destructors run while their lease is alive.
- Authored material support is explicit and checked against the converter's supported depth/blend state. SceneAssetSource still supports only CullMode NONE; external producer facts may declare tested one-sided geometry. Unknown/deformed/alpha-test/blended coverage and VisibleSurface remain Unsupported. No arbitrary shader introspection or promised presented-frame pixel identity.
- Overlay proxies are drawn by SceneViewport from the same cached logical layout used for picking. Depth-tested proxies require a matching producer-rendered visual; CPU testing does not render arbitrary depth-tested overlay visuals. Transform Gizmo capture keeps precedence.
- Windows Vulkan/DX12 were exercised; Metal/macOS/mobile host integration remains unverified. No GPU ID pass, box/multi-selection, file watcher or terrain/vegetation adapter is implemented.

Reproduction (close SandboxEditor first):

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Develop --parallel 8 --target CoreTest EditorCoreTest EditorShellTest EditorRenderTest Aurora.AdaptorTest UICoreTest Aurora.Renderer.GpuSubmissionTest SandboxModule SandboxEditor
# Repeat with --config Debug, then run each suite from the repository root.
output/bin/Develop/EditorRenderTest.exe --gtest_filter=CpuViewportPickerTest.RecordedColdWarmWorkload --gtest_repeat=20
output/bin/Develop/EditorRenderTest.exe --gtest_filter=CpuViewportPickerTest.RecordedLargeSceneAndOverlapWorkloads
Push-Location output/bin/Develop
./Aurora.Renderer.GpuSubmissionTest.exe --gtest_filter=*EditorHostNativeTest*
Pop-Location
openspec validate sandbox-viewport-picking --strict
```

## Preserved phase A baseline (historical)

The following record predates phase B; its material/provider limitations and timing are historical, not the current support matrix.

Date: 2026-10-23. Windows x64, VS 2022 / CMake 3.31.11, Intel Core i7-12700KF, NVIDIA GeForce RTX 3080. Existing `cmake-build-debug`, third-party root `build_3rd/Win32`; no new target or module.

## Build and tests

Closed SandboxEditor before configuration/build. Develop SandboxModule and SandboxEditor rebuilt successfully. Debug native tests use `SKY_GPU_VALIDATION=AUTO` -> validation ON; Develop uses validation OFF. Final build logs contain no compiler errors or warnings. Both native runs passed; the Debug log contains no Vulkan validation errors and the DX12 info queue reported no error/corruption messages.

| Suite | Configuration | Passed |
| --- | --- | --- |
| CoreTest | Develop | 308/308 |
| EditorCoreTest | Develop | 177/177 |
| EditorShellTest | Develop | 140/140 |
| EditorRenderTest | Develop | 20/20 |
| Aurora.AdaptorTest | Develop | 36/36 |
| UICoreTest | Develop | 190/190 |
| EditorHostNativeTest (Vulkan dynamic / render-pass / DX12) | Debug | 6/6 |
| EditorHostNativeTest (Vulkan dynamic / render-pass / DX12) | Develop | 6/6 |

The CPU/UI/adaptor suites contain 871 passing cases. Six native cases run in both configurations (877 distinct cases across these selected suites). The new picking coverage is 12 EditorRender cases, 4 SceneViewport cases, 3 adaptor cases, 2 BVH cases, and 3 backend-parameterized native cases.

Reproduction from repository root:

```powershell
Get-Process SandboxEditor -ErrorAction SilentlyContinue | Stop-Process -Force
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Develop --parallel 8 --target SandboxModule SandboxEditor CoreTest EditorCoreTest EditorShellTest EditorRenderTest Aurora.AdaptorTest Aurora.Renderer.GpuSubmissionTest UICoreTest
cmake --build cmake-build-debug --config Debug --parallel 8 --target Aurora.Renderer.GpuSubmissionTest
$names = @('CoreTest','EditorCoreTest','EditorShellTest','EditorRenderTest','Aurora.AdaptorTest','UICoreTest')
foreach ($name in $names) { & ("output/bin/Develop/" + $name + ".exe") }
Push-Location output/bin/Debug
./Aurora.Renderer.GpuSubmissionTest.exe --gtest_filter=*EditorHostNativeTest*
Pop-Location
Push-Location output/bin/Develop
./Aurora.Renderer.GpuSubmissionTest.exe --gtest_filter=*EditorHostNativeTest*
Pop-Location
openspec validate sandbox-viewport-picking --strict
```

Local ignored evidence: `output/pick-build-review-{develop,debug}.log`, `output/pick-build-final-develop.log`, `output/pick-{CoreTest,EditorCoreTest,EditorShellTest,EditorRenderTest,Aurora.AdaptorTest,UICoreTest}.{xml,log}`, and `output/pick-native-{debug,develop}.{xml,log}`.

## Behavior exercised

- U16/U32 indices, submesh part identity, exact triangle holes, front/back two-sided hits, near/far clipping, overlapping instances, negative/non-uniform transforms, parent transforms in scene extraction, degenerate triangles, invalid ranges/positions, singular transforms and parallel-axis boundary rays.
- Shared BVH reuse across 100 instances and transform changes; geometry revision rebuild; bounded LRU eviction, working-set overflow, a later request recovering from budget rejection, stale document completion discarded, explicit worker drain on reset.
- Pending/Unsupported/Error preserve selection. Known off-ray invalid geometry and farther unresolved bounds do not block a nearer complete hit. Unknown bounds remain conservative.
- Main and floating Shell routes; matching pointer/button, max-travel out-and-back cancellation, competing mouse buttons, DPI, layout, camera, surface, tool, document, Play and explicit cancellation; no selection of a deleted target; Q/W/E/R all pick outside handles. Actual mouse-capture priority regressions remain passing.
- Native EditorRenderer uses the production SceneAssetSource and RenderSceneBridge, real Vulkan dynamic rendering / Vulkan render pass / DX12. Two overlapping cube Actors have only Transform + StaticMesh components (no Collider). The nearer Actor is selected; moving it away selects the farther one. A rotated cube's world-bounds corner misses. The floating viewport uses the same selection and correct window coordinates. Inspector binding, Outliner selected-row paint and Gizmo geometry reflect that selection. Empty click clears; World dirty and command history remain unchanged.

## Performance sample

`CpuViewportPickerTest.RecordedColdWarmWorkload` constructs a 20,000-triangle grid shared by 200 instances and measures 100 warm queries. Figures are elapsed CPU wall times, not frame times or promised latency; asset/shader load time is outside this geometry-only fixture.

- BVH worker build: **358.975 ms**. The click callback does not do this work; Pending preserves selection until preparation completes.
- Warm query mean: **0.022949 ms**; maximum: **0.035800 ms**.
- Resident BVH capacity: **2240000 bytes** (2.136 MiB), one BVH for all 200 instances.
- Default cache: 64 MiB / 128 entries; one cancellable worker and one pending geometry request. Cache plus reserved geometry preparation stays within its budget. Existing asset-source memory has its separate 256 MiB budget; shader payloads and the scene snapshot are shared source data, not duplicated into cache entries.

## Review and limitations

- Production dependency directions preserved: EditorCore declares the consumer contract; EditorRender implements it; Aurora exposes owned geometry without Sandbox headers. Only the existing native test target adds an EditorShell test dependency.
- No new throw, try/catch or dynamic_cast; result statuses and source invalidation's bool result are explicit. No new singleton, GPU wait/readback path, asset decoder, schema change, module or directory move.
- Review fixes include the BVH parallel-slab bug and pathological split depth, floating GetPanelView returning a duplicate view, cancellation/worker drain, and rejecting extraction failures. The older B14 native test consumed renderer events directly and stole FrameCompleted from host pacing. It now observes the host's event callback; its original readback and retirement assertions pass again with one event consumer.
- Supported material subset is the generated built-in opaque, double-sided Lit/Unlit path. Authored materials, shader deformation, skinning, alpha/transparent pixel tests, terrain/vegetation and non-mesh icons are not claimed supported. Unsupported candidates preserve selection and report status. There is no box/multi-selection or GPU ID pass.
- Asset reset/reload events and explicit source invalidation advance geometry versions. Automatic file watching is outside scope because the current AssetManager has no such reload entry point. CPU geometry selection is not a guarantee of delayed presented-frame pixel identity.
- Windows Vulkan/DX12 are validated above. Non-Windows hosts return an unavailable picker; Metal/macOS and mobile are unverified.
- OpenSpec strict validation and `git diff --check` passed. Unrelated research documents were not changed by this work. No commit, archive, rebase or push performed for this change.
