# Verification — 2026-10-30

## Scope and source review

This change edits only `engine/aurora/core/src/renderer/RendererRuntime.cpp` in production code. There are no CMake, target, directory, public API, ECS component, scheduler or kernel changes.

The existing ECS chain remains:

`FramePacket -> CpuSceneStore::Apply -> Applied resource/output synchronization -> Freeze -> BuildPreparedSceneProgram -> ScenePrepareScheduler -> WaitUntilUnfrozen -> CollectPreparedScene -> PreparedRenderFrame -> GPU graph/encoding/submission`.

`PrepareRenderObjects` still declares component reads and the derived `RenderObjectData` write. `ExtractView` still declares a read and an explicit dependency on the scene producer. GPU helpers receive prepared snapshots, not ECS store/registry access. Resource subscriptions still include changed scenes whose viewports are suspended.

Compared with the saved pre-refactor working baseline:

| Entry point | Body lines before | Body lines after |
| --- | ---: | ---: |
| Applied | 139 | 9 |
| Build (ECS program declaration) | 19 | 19 |
| Prepared | 37 | 23 |
| Pump | 93 | 14 |
| BuildFrame | 337 | 46 |

Twelve extracted but otherwise unchanged methods match the baseline after ignoring whitespace and the two member renames. The other six saved Party engine implementation/test files remain byte-identical. Packet, catalog, output and GPU stage statement ordering was also reviewed directly.

Two cleanup details are explicit in the refactor:

- A feature build failure clears the composer's borrowed pointer to its temporary prepared view before the helper returns.
- Upload preparation with no admitted work returns an invalid batch ID and registers no batch. The previous unconditional `Discard` of this ID was invalid and its error was ignored; that call is removed. Real batch cleanup still checks errors.

## Build and static checks

- Built `Aurora.Core.Static`, `Aurora.Render`, `Aurora.AdaptorTest` and `Aurora.Renderer.GpuSubmissionTest` against the final source in the existing `cmake-build-debug` VS 2022 cache.
- Built the nine CPU renderer test targets listed below, including the real core library dependency.
- `clang-format --dry-run --Werror` passed for the modified C++ file.
- `git diff --check` passed for the engine changes.
- Both `refactor-renderer-runtime-flow` and the existing `add-party-softbody-project` passed strict OpenSpec validation.

## CPU and adaptor regressions

Passed CTest targets: `Aurora.Renderer.ContractTest`, `ProductsTest`, `ChunkPrepareTest`, `ParallelPrepareTest`, `WriteViewTest`, `PrepareTest`, `StateTest`, `CoordinatorTest`, and `ResourceTest` (the shortened names share the `Aurora.Renderer.` prefix).

These cover declared access enforcement, producer/consumer ordering, parallel/chunk execution, dirty/resource invalidation, staged write publication, retained-reader drain, cancellation, scene failure and prepared-product collection.

The affected adaptor selection passed 14/14 cases: `RendererHost.*`, `RenderSceneProxy.*`, `LauncherSurfaceTest.*`, and `UiRenderFeature.*`. This includes the three project scene-source regressions and surface-loss/replacement handling.

## Native GPU regressions

Ran 197 cases on Vulkan dynamic rendering, Vulkan render-pass mode and D3D12: **187 passed, 10 skipped, 0 failures**, in 138.846 seconds. All skips are Vulkan-specific output checks instantiated with the D3D12 parameter.

| Suite | Cases | Passed | Skipped |
| --- | ---: | ---: | ---: |
| FrameFeatureRegistryTest | 2 | 2 | 0 |
| Windows/RendererHostNativeTest | 9 | 9 | 0 |
| Selected Windows/RendererRuntimeTest | 63 | 63 | 0 |
| NativeCompilers/FrameOutputs | 123 | 113 | 10 |

The runtime selection covers feature prepare/build failures, queued compositions/output versions, empty frames, readbacks, first-frame uploads, resource-only progress, suspended viewport updates, native/offscreen coexistence, scene failure isolation/recovery, encoding failure recovery, chunked uploads, shared subscriptions, hot materials, device loss, shadow receiver pixels and shared pass bindings. Output tests cover acquisition, resize/minimize, skipped outputs, compile/submission failure, presentation retirement and upload-only graphs. The exact selection is saved in `build/renderer-runtime-review/run-gpu-tests.ps1` and the XML report.

## Environment and intermediate findings

- Sandboxed parallel MSBuild exited without compiler diagnostics; a single-node attempt exposed MSVC C1902 for PDB-server access. Normal process permissions completed the build. The final build used `/m:1 /nr:false` and retained the original generator and third-party path.
- The broad sandboxed `Aurora.AdaptorTest` invocation exceeded its 120-second bound in unrelated skeletal asset/shader cooking. A focused host/source rerun initially failed at temporary shader-blob publication in fixture setup; the same 14 cases passed under normal process permissions. The entire adaptor suite is not claimed as verified by this change.
- An intermediate GPU run caught the invalid empty-batch discard described above. It was stopped, the uploader contract was checked in source, and the final regression selection was rerun after correction.

Local evidence is under `build/renderer-runtime-review/`: saved baseline/patch, `review.py`, `build-verified.log`, `cpu-tests.log`, `resource-tests.log`, `adaptor-tests.xml`, and `gpu-tests.xml`. These scratch artifacts are not part of the commit.

## Submission state

Prepared as a separate refactor commit after the earlier Party engine support/fixes, with each change's code and OpenSpec artifacts together. The concrete messages and exact file lists are in `build/renderer-runtime-review/commit-plan.md`. No commit, archive or unrelated index modification has been performed.
