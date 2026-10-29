# Verification

Date: 2026-10-27. Host: macOS, Intel Iris Plus Graphics 655. Existing Xcode Debug build and `3RD_PATH=build_3rd/MacOS-x86` were preserved. No commit, archive, target ownership or directory migration was performed.

## Physical Android compatibility follow-up — physics/package scope passed

Date: 2026-10-28. Evidence is under `build_ios/projects/AndroidPhysicsDemo/validation`; `validation-report.json` is the consolidated record. This follow-up is distinct from the original host evidence below and from the earlier emulator fixture. Its acceptance is limited to package integrity, native compatibility, observed physics behavior and the measured presentation interval; visual-quality and validation-clean acceptance remain open.

The first physical launch failed at the unconditional dynamic-rendering requirement. The selected Adreno (TM) 725 reports physical Vulkan 1.1.128, while the loader reports 1.3.0. The existing backend now negotiates the actual core/KHR features and commands, preserving mandatory timeline/synchronization2 rather than forcing feature bits. Runtime logging confirms `dynamic=0`, `render-pass2=1` and `frame compiler: render-pass` for AUTO. Explicit-mode and unsupported direct-rendering rejection remain enforced.

The final arm64 Debug/static APK is 45,422,822 bytes with SHA-256 `a47b4d26afae3b9ae316e79cde9fa026205a1b159d89860795868e5d990d6004`. All 40 archive checks passed: SDK signature/manifest and ABI, ELF dependency closure, flat product identity/version/length/checksum, complete binary World/source correspondence, shader inventory/bytes, Jolt license and source/compiler/builder/group/scratch exclusion. The World has seven actors, 18 components and four bodies (one static ground and three dynamic boxes). The 54 actual shader blobs contain SPIR-V 1.3 and all passed the SDK validator with `--target-env vulkan1.1`; metadata was not merely relabeled. Evidence: `apk-audit.json`, `apk-vulkan11-audit.log`, `package-vulkan11.log` and `spirv13-validation/report.json`.

The final host checks passed 119 selected native/C++ executions: RHI 31, actual Slang baseline compilation 2, Core 6, Adaptor 78 and native Metal context 2, with no failures or skips. The interface-only renderer contract exited 0. Python compatibility checks passed 30; the source-ownership gate passed 25 with one skip. Evidence: `compat-rhi-tests.xml/.log`, `compat-shader-tests.xml/.log`, `compat-core-tests.xml/.log`, `compat-adaptor-tests-final.xml/.log`, `compat-metal-context-tests.xml/.log`, `compat-renderer-contract.log`, `compat-python-tests.log` and `compat-source-ownership-tests.log`. The first Adaptor run had 77/78 passes: its new test reused a module after Shutdown had released its injected fake provider, inadvertently requesting explicit RenderPass from real Metal. The fixture now recreates that fake provider/module; the final 78/78 run passed without changing Metal's correct rejection or weakening the assertion. The initial failed log/XML remain intact and are not counted as passes.

The physical run lasted 121.478 seconds with the same PID 9540. At t=8.000 seconds the observer reported all three boxes had fallen and settled, with center heights `[0.4800, 0.4902, 0.4800]`; no physics FAIL log was recorded. At t=120 seconds it still reported four bodies, zero active bodies, the same heights and 7176 physics steps. Steady-state samples from 20 through 120 seconds show RSS 282036-282044 KiB. This is a bounded observation, not a general absence-of-leaks proof.

Presentation progress was measured separately: `presentation-progress.json` contains four successful observations of the same BLAST SurfaceView over 15.297 seconds, with strictly increasing real presentation timestamps from 53906650534122 to 53921878547085 ns. The driver's initial surface query failed and is retained separately; the physics/memory run is not relabeled as 120 seconds of presentation sampling. Scene captures and observer logs establish the scoped physics result, not complete image-quality acceptance.

Known limits remain: shadow striping is visible on cube faces and has not been isolated by controlled A/B; the legacy shadow algorithm was not changed in this follow-up. `VKDBGUTILWARN003` appears 72037 times, and the Khronos validation layer is unavailable, so this is not a validation-clean claim. BACK was consumed without exiting the application; final ADB force-stop stopped it, rather than demonstrating graceful application shutdown. `device-cleanup.json` records restored screen timeout 60000 ms, stay-awake setting 0 and removal of the probe. Runtime/termination evidence is in `device-runtime-driver.log`, `device-runtime-exit.log`, `runtime-logcat.log`, `runtime-progress.json` and `validation-report.json`.

Android Back, graceful shutdown and same-process recreation were subsequently fixed and independently verified in [fix-android-application-lifecycle](../fix-android-application-lifecycle/verification.md). That later result does not replace this artifact's historical termination or visual-quality evidence.

## Original host build — 2026-10-27

Third-party preflight reported zero missing/stale packages and zero missing outputs. Reconfigured the existing `cmake-build-debug` for new sources and removed APIs.

Built `Aurora.RHITest`, `Aurora.CoreTest`, `Aurora.PipelineTest`, `Aurora.AdaptorTest`, `Aurora.Renderer.ContractTest`, `EditorRenderTest`, `SandboxModule` and `SandboxEditor`, including their affected libraries/native backends. Final logs are `cmake-build-debug/framegraph-build-close.log` and the subsequent input-error correction build `framegraph-build-input-validation.log`; both succeeded. This was an incremental build, not a clean build.

The initial sandboxed Xcode attempt could not write DerivedData, and sandboxed Metal could not obtain a system device. The approved normal-access build/test runs succeeded. Those infrastructure failures are retained in the initial logs and are not counted as successful tests.

## Original regression evidence

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
