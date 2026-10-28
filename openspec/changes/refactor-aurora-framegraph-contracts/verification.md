# Verification

Date: 2026-10-27. Host: macOS, Intel Iris Plus Graphics 655. Existing Xcode Debug build and `3RD_PATH=build_3rd/MacOS-x86` were preserved. No commit, archive, target ownership or directory migration was performed.

## Build

Third-party preflight reported zero missing/stale packages and zero missing outputs. Reconfigured the existing `cmake-build-debug` for new sources and removed APIs.

Built `Aurora.RHITest`, `Aurora.CoreTest`, `Aurora.PipelineTest`, `Aurora.AdaptorTest`, `Aurora.Renderer.ContractTest`, `EditorRenderTest`, `SandboxModule` and `SandboxEditor`, including their affected libraries/native backends. Final logs are `cmake-build-debug/framegraph-build-close.log` and the subsequent input-error correction build `framegraph-build-input-validation.log`; both succeeded. This was an incremental build, not a clean build.

The initial sandboxed Xcode attempt could not write DerivedData, and sandboxed Metal could not obtain a system device. The approved normal-access build/test runs succeeded. Those infrastructure failures are retained in the initial logs and are not counted as successful tests.

## Current regression evidence

| Suite | Result | Evidence under cmake-build-debug |
|---|---|---|
| RHI, including native Metal/Vulkan, group/use/state contracts | 186 passed, no skips | `framegraph-rhi-final.xml` |
| Core, including five composition/cache/input regressions | 111 passed, 1 skipped, no failures | `framegraph-core-close.xml` |
| Pipeline | 43 passed, no skips | `framegraph-pipeline-final.xml` |
| Adaptor | 66 passed, no skips | `framegraph-adaptor-final.xml` |
| Editor rendering | 45 passed, no skips | `framegraph-editor-final.xml` |
| Public interface-only renderer contract | Exit 0 | Final built `Aurora.Renderer.ContractTest` |
| Parameter bindings after the last error-classification fix | 11 passed; overlaps the Pipeline suite | `framegraph-parameter-close.xml` |

The five full gtest suites cover 452 unique cases: 451 passed and one was skipped. The standalone contract executable and repeated targeted checks are not added to that count. The skipped Core case, `ShaderDistribution.RelocatedProductsResolveWithoutSourcesOrCompiler`, requires a relocated shader product package absent on this host.

The first full Core run reported 110 passed, one skipped and one failed: the new mismatched-resource test found that a service input error was classified as COMPILE_FAILED. The service boundary now reports INVALID_ARGUMENT for this structural input error. A complete Core rerun passed with the counts above; the original `framegraph-core-final.xml/log` remain as failure evidence.

Covered changed behaviors include:

- Pure pre-cull declaration validation, effective dynamic ranges, Custom order and multi-role raster storage use facts.
- Canonical group equivalence, resource/view/layout isolation, separate per-use access/offsets and failed publication preserving the old group.
- Uniform budget reuse, shader identity isolation, nonzero-to-zero dynamic offset cache hits, stale leases and resource mismatch rejection before allocation.
- Native Metal color/depth STORE and DONT_CARE resolve, subsequent LOAD, foreign/forged plan rejection, consumed-plan failure cleanup and no successful receipt on native recording errors.
- Native Metal dynamic uniforms, multiple in-flight slots and depth-array cached binding refresh after a producer changes contents.
- Named parameter updates through their distinct interface, typed/opaque state validation and failure atomicity.

## Static and specification checks

Scoped clang-format, changed-file ASCII inspection, added forbidden-pattern inspection and `git diff --check` passed. No new throw, try/catch or dynamic_cast was found. Strict validation of `refactor-aurora-framegraph-contracts` passed. The host has no clang-tidy executable; no tidy-clean claim is made.

Windows-only call signatures, group root addresses, descriptor table mapping, both Vulkan compiler call chains, test Draft/Set migrations and temporary-container iterator use received an independent source review. No new definite issue was found; source review does not replace native Windows execution.

## Remaining platform checks

This host cannot build/run DX12 or the Windows-only `Aurora.Renderer.GpuSubmissionTest`. Its new native cases remain unverified:

- `GpuProducedStorageCombinesVertexAndShaderReadSynchronization` for copy and compute producers, real vertex/storage consumption and combined native barriers.
- `BindingGroupsReuseDescriptorsWhileDynamicOffsetsAndBaseRangesStayIndependent` for both Vulkan modes, and the corresponding DX12 `BindingGroupsReuseTablesWhileDynamicOffsetsAndBaseRangesStayIndependent`.
- Existing Windows GPU retirement, failure/rollback and host regressions against the migrated API.

Run those with the existing Windows cache/toolchain and debug validation layers. Native Metal evidence above is current; it is not a substitute for these checks. Mobile platforms, a source-free release package and a clean build were not part of this host verification.

## Manual editor validation

Computer Use accessibility and screenshots were verified before starting the editor. In the existing Test project, the editor UI created and saved `assets/framegraphlighting.world` with a builtin lit Cube (Y rotation 30 degrees), Plane (Y = -0.5), Directional Light and Point Light (position -2, 2, 2). The Metal viewport visibly rendered both meshes, directional lighting, point-light falloff and a ground shadow. The saved document was checked for all four actors and their edited transforms. The editor then closed normally with process exit 0; `cmake-build-debug/cube-plane-ui.log` records creation, save and renderer shutdown.

The default, unrotated Cube exhibited angle-dependent self-shadow striping. The scene's current orientation rendered cleanly, without disabling shadow receipt. Existing shadow sampling uses a fixed bias, 512-pixel map and nearest single-sample comparison; this manual check does not claim that the shadow-quality issue is fixed. macOS UI DPI corrections discovered during this run are separate from the FrameGraph contract refactor.

After the UI corrections, the saved world was reopened through File > Open World using Computer Use. All four actors and the lit scene rendered again; the editor exited with code 0. The reload/shutdown log is `cmake-build-debug/cube-plane-final-ui.log`. UI-specific build and 190 passing shell/widget regressions are recorded in `docs/editor/ui-sizing-and-scaling.md` and are not added to the FrameGraph suite count above.
