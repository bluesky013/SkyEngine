# Verification - 2026-10-28

## Scope and current status

This report covers the flat-product successor, not the earlier ProductGroup implementation. Historical reports and failure logs remain intact. Evidence is under the ignored fixture project `build_ios/projects/IOSWorldCookDemo` unless another location is stated.

Framework and all production callers now use checked `SaveAsset`/`SaveProducts`, fresh build objects and loaded-instance identity. The final host rebuild includes the Navigation caller correction and removal of the unused `verifiedLegacyId` interface. There is no ProductGroup API, old-format product reader, owner manifest or generation-directory fallback. Development output is cleared and recooked; source UUIDs and stable generated child keys/UUIDs remain in the source catalog.

Before the rebase, host, macOS Metal, iOS and Android API 36 acceptance completed for the scope below. The then-final device archive and Simulator audits passed, and the Simulator short smoke loaded and displayed the default World. Android's restored x86_64 and arm64 packages each passed 33 archive checks. The same audited x86_64 APK completed sustained rendering on API 36 with the original engine synchronization path. API 35's retirement failure and the rejected diagnostic runs remain separate historical evidence; no engine workaround was retained. The merged-source checks are listed separately in the next section.

## Rebase integration checkpoint

The platform artifacts and runtime measurements below were collected before rebasing onto remote `ad207629`; they are not relabeled as execution of the merged source. The integration retains the remote offline `AssetProductData`/`SaveProductData` and asynchronous load interfaces, Jolt's shared codec/builder/adaptor paths, and checked World initialization/attachment. The raw and typed product paths now share flat publication. Configuration and affected native builds passed, including the final Jolt private-symbol rebuild. Evidence: `rebase-configure.log`, `rebase-host-build-verified.log`, `rebase-asset-tool-build.log` and `rebase-jolt-private-build.log`.

The scoped Jolt delta reconciles platform/consumer RTTI matching and native static-image SDK guard ownership needed to retain the remote functionality. Native SDK symbols and private implementation types/templates also require image-local visibility to prevent cross-image weak-symbol selection of another allocator or SDK state. C++ exceptions remain disabled, public Physics interfaces and process-shared registries remain visible, and metadata keeps its unique definition owner. No module, target or public SDK lifecycle API is added. The macOS SDK was rebuilt with matching RTTI before the final consumer rebuild; the final module lifecycle test exited 0 and explicitly reported two sessions, one metadata owner, retained worlds and a shared render provider. Its unsupported deterministic-mode messages are expected negative-case results. Evidence: `rebase-jolt-rtti-build.log` and `rebase-JoltPhysicsModuleLifecycle-private.log`.

The merged-source native results are:

| Test executable | Passed | Disabled | Skipped |
| --- | ---: | ---: | ---: |
| CoreTest | 315 | 0 | 0 |
| FrameworkTest | 239 | 0 | 0 |
| Aurora.AdaptorTest | 77 | 0 | 0 |
| Content.BuilderTest | 1 | 0 | 0 |
| EditorCoreTest | 184 | 0 | 0 |
| EditorShellTest | 148 | 0 | 0 |
| EditorWidgetsTest | 42 | 0 | 0 |
| SandboxProjectTest | 14 | 0 | 0 |
| JoltPhysicsTest | 58 | 5 | 0 |

These total 1078 passes, five disabled streaming benchmarks and no failures/errors. The disabled cases are not passes or runtime skips. Each executable exited 0. The final Jolt result is the `private` run at 13:53:20, not either earlier duplicate 58-test run. Framework coverage includes the merged raw-publication/async-load contracts and binary World initialization through the existing checked Build/attachment path. Evidence: `rebase-<executable>.log/.xml`, with `rebase-JoltPhysicsTest-private.log/.xml` for Jolt.

The final source-ownership gate ran 26 tests: 25 passed and one skipped (`rebase-cmake-source-ownership-final.log`). Pure Python package/fixture tests passed 41/41 without skips; two real AssetTool package-audit tests were excluded from that count and executed separately, both passing. All 17 real AssetTool batch cases returned their expected outcome, including required failures for negative cases. The 14-test focused package-boundary rerun is a subset and is not added again. Evidence: `rebase-python-tests.json/.log`, `rebase-real-package-audit-tests.json/.log`, `rebase-asset-tool-tests.json/.log` and `rebase-asset-tool-integration/asset-tool-batch-integration.json`.

Earlier integration failures remain intact: the cook-all World skip was caught by the real batch fixture before correction (`rebase-asset-tool-tests-before-world-skip-fix.json/.log`); earlier build/link failures and the Jolt lifecycle crash/LLDB records were retained. The lifecycle issue exposed both SDK header weak-symbol coalescing and private backend template construction selecting another image's allocator. The final image-local SDK/private visibility and SDK-state ownership passed the original lifecycle test and whole Jolt suite. These final host results do not imply fresh macOS/iOS/Android package or device runs after the rebase.

## Pre-rebase host and automated checks

The existing macOS Debug build configured and built the affected Framework, Core, AssetTool, Content/Aurora/Animation and Navigation targets. Final results:

| Test executable | Passed | Skipped |
| --- | ---: | ---: |
| FrameworkTest | 233 | 0 |
| CoreTest | 315 | 0 |
| Aurora.AdaptorTest | 77 | 0 |
| Aurora.CookTest | 51 | 0 |
| Content.AdaptorTest | 7 | 0 |
| Content.BuilderTest | 1 | 0 |
| Animation.BuilderTest | 4 | 0 |
| Animation.WorldTest | 10 | 0 |
| Aurora.Renderer.ModuleTest | 8 | 3 |
| Aurora.Renderer.StaticModuleTest | 9 | 3 |

The last eight executables total 167 passing executions, for 715 passes across the table. The six skips are the three shipped-shader fixture cases in each renderer module executable; they are not counted as passes. The final Framework count is one lower than the earlier 234-test run because the obsolete legacy-ID migration test was deleted.

Separately, macOS Vulkan native sync/submission checks passed 10/10 with the original `VulkanFence::Query` implementation restored. These cover signaled/unsignaled fences, binary and timeline semaphore submissions, host timeline signaling, multithreaded recording and queue idle. This validates the native synchronization/submission path on this Mac, not Android imported-fence handling or an Android window presentation endurance run. Evidence: `flat-macos-vulkan-sync.log/.xml`.

Framework coverage includes magic/version/UUID/length/checksum rejection, checked encoding and write/flush/readback failures, untouched destinations before replacement, batch invalidation after partial replacement or index failure, unsupported atomic storage, source-path ownership for generated batches, source-free inspection/loading, dependency failure completion and exactly-once loaded notification. Retained old assets keep their original bulk bytes after recook, while subsequent loads use distinct instance IDs. Bulk is now bound to owned memory rather than a long-lived `ifstream`, preserving content and avoiding a retained file descriptor or reopening the replaced UUID path.

Python tests passed 90/90 with no skips: the final platform/native-package/project-fixture run passed 35, the other packaging/shader/module suites passed 51, and animation packaging passed four, including the actual rebuilt AssetTool. The previous aggregate JSON records 89 before the incremental CMake regression was added; `flat-python-final-tests.log` supplies the updated 35-test result rather than overwriting that earlier evidence.

All 17 real AssetTool integration cases returned their expected status. Positive cases cover image/World cook, World-only cook, logical product roots and internal file aliases. Negative cases cover failed cook or missing builder, malformed/unknown World schemas, missing or ambiguous roots, unsafe paths, wrong type, unsupported product version, bad checksum and invalid product body. The real source-free closure test also exercises LZ4 payloads. No failed cook or failed dependency audit is accepted as a successful package.

Evidence: `flat-final-host-build.log`, `flat-host-final-build.log`, `flat-framework-tests.log/.xml`, `flat-core-tests.log/.xml`, `flat-<executable>.log/.xml`, `flat-python-final-tests.log`, `flat-additional-python-tests.log/.json`, `flat-animation-package-tests.log`, `flat-validation-summary.json` and `flat-asset-tool-tests/asset-tool-batch-integration.json`.

## Clean package and runtime checks

Packaging deletes old cooked output before full cook and deletes partial output after failure. The audited Apple bundles contain six ordinary UUID products, no `groups`/`.groups` tree and no publication scratch files. Each product has the 40-byte `SPRD` version-1 envelope with matching UUID, exact body length and FNV-1a64 checksum. The default World contains the complete typed `WRLD` v1 body, five actors and ten components. Logical defaultScene/index resolution and dependency closure agree. Each audited Metal distribution contains 54 matching shader blobs; authoring World JSON, shader/C++ sources and compiler/cook runtime images are absent.

| Platform/artifact | Completed evidence | Remaining acceptance |
| --- | --- | --- |
| macOS final app and ZIP | Fresh public package command; 25/25 archive/bundle checks; final Computer Use observation of Cube, Plane and lighting; bounded `--frames 1800` run stopped its renderer host and exited successfully. An earlier 6000-frame run also exited 0. | None for this scope. |
| iPhoneOS arm64 app and unsigned IPA | Final-source public packaging and re-audit passed 26/26 checks, including CRC, archive/app equality, Mach-O platform/dependencies and product/shader contents. The final IPA SHA-256 is `d9205b12e9cbdaaed9ba6f5758f8d2c386be80d63b260e8b1d757ad2da2b500d`. | None for archive acceptance; no physical-device execution or provisioning is claimed. |
| iPhoneSimulator x86_64 app | Final-source relink, ad-hoc signature and audit passed 25/25 checks. The final short smoke logged `Default World ready: scenes/cube_plane.world (5 actors)` and the final SDK screenshot visibly showed Cube, Plane and lighting. `simctl terminate` and Simulator shutdown both returned 0. In the earlier flat-product run, Computer Use confirmed portrait, landscape and restored portrait; the monitored run completed 185.55 seconds and was stopped afterward. Its 37 samples span 0.01-180.54 seconds; RSS stayed between 106896 and 121804 KiB and ended at 106944 KiB, with no accumulating trend in that interval. | None for the defined smoke scope. The prior endurance/rotation run is not relabeled as a run of subsequently rebuilt bytes. |
| Android x86_64 and arm64-v8a APKs | Both public package builds succeeded through the normal Gradle assemble flow. After removing all presentation experiments, restored packages each passed 33/33 checks for signatures, ABI/ELF dependencies, flat products, binary World and shipped shader contents. The identical audited x86_64 APK passed API 36 sustained rendering: 40 process samples over a 194.713-second window, 11 strictly increasing presentation observations and three visible-scene captures; SDK force-stop and emulator shutdown completed. | None for API 36 x86_64 emulator acceptance. API 35's four-frame stall remains a failed environment result; physical arm64 execution is not claimed. |

Apple evidence: `flat-macos-package-final.log`, `flat-macos-package-audit.json/.log`, `flat-macos-runtime-final.log`, `flat-macos-runtime.log`, `flat-ios-package.log`, `flat-ios-package-final.log`, `flat-ios-package-audit.json/.log`, `flat-ios-simulator-audit.json/.log`, `flat-ios-simulator-final-build.log`, `flat-ios-simulator-runtime-final.log`, `flat-ios-simulator-runtime.log`, `flat-ios-memory.json/.log`, `flat-ios-portrait.png`, `flat-ios-landscape.png`, `flat-ios-restored-long-run.png`, `flat-ios-final.png` and `flat-runtime-tested-binaries.json`. Final Apple re-audits are dated 2026-10-28 01:18 UTC. Computer Use supplied the final macOS visual check; no additional screenshot file is asserted here.

Android final restored artifacts:

- `build/android/IOSWorldCookDemo-debug-x86_64-restored.apk`: 30925662 bytes, SHA-256 `ece64539be1d426371702cd2d29222498c74a8c5b7a73411438313e46bfed961`; `flat-android-x86_64-restored-audit.json/.log`, 33/33 passed.
- `build/android/IOSWorldCookDemo-debug-arm64-v8a-restored.apk`: 48936286 bytes, SHA-256 `fb93313629afe4d37b7d3eabb291230d59f9b719d5f96c689c4e7b02b39ec6e1`; `flat-android-arm64-restored-audit.json/.log`, 33/33 passed.

The corresponding `flat-android-*-restored-package.log` files retain the public build results. The initial arm64 Gradle lock failure in `flat-android-arm64-before-sandbox-access.log` was a sandbox permission failure; the authorized retry succeeded. It is not classified as a source/build correctness failure.

## Android API 35 investigation: failed environment

The original API 35 x86_64 app loaded the binary default World with five actors and initially displayed Cube, Plane and lighting (`flat-android-ready.png`). Later captures were black (`flat-android-final.png`, `flat-android-longrun-2.png`). PID 1980 remained alive and foreground while SurfaceFlinger stopped advancing at frame 4. Forty samples over approximately 195 seconds showed RSS 177636-179172 KiB and mappings 2964-2965, with no repeated mapping growth. This is liveness/memory evidence only; a stopped presentation loop is not a successful rendering endurance test.

An Android-only temporary diagnostic identified the retirement gate: all four frames were `PRESENTED`, `gpuComplete=1` and `borrowerExpired=1`; acquire fences were `SIGNALED`, while every present fence stayed `PENDING` for more than 30 seconds. Thus the observed failure is native presentation retirement after successful flat-product loading. No product format, identity, checksum or dependency-closure failure was observed. Evidence: `flat-android-diagnostic-runtime.log`, `flat-android-surfaceflinger.txt` and `flat-android-memory.json/.log`.

Three experiments were rejected and removed:

- Switching the emulator host graphics backend still stopped at frame 4. The nominal `lavapipe` setup had actually selected SwiftShader; the bundled Lavapipe library was arm64-only on this Intel host. `flat-android-host-*.log/.txt` retain the single host-backend comparison.
- An additional independent acquire record allowed frame 5 but then stalled again (`flat-android-acquire-reserve-surface.txt`). The extra slot was reverted, not accepted as a forward-progress fix.
- Standard zero-timeout `vkWaitForFences` polling advanced frames, but the Android 15 reference implementation's WorkPool could return success on timeout while its external-fence task was still pending. The positive pictures/frame counts therefore could not establish safe synchronization or retirement. This experiment was stopped and rejected; `flat-android-fence-poll-rejected.json` explicitly records `accepted=false`, and its partial sample log is not counted as a passed endurance run.

All diagnostic logging, the extra acquire slot, zero-wait polling and associated test seams were removed. `FrameScheduler.cpp`, `VulkanSwapChain.cpp` and `VulkanFence.cpp` have no remaining diff from these experiments. No Android-specific bypass, forced fence completion or early resource recycling was retained.

The official Android 15 r1 source provides a matching explanation: AOSP WSI imports the release SYNC_FD into the present fence, gfxstream stores that imported payload in guest state, but its generated `GetFenceStatus` path queries the host fence instead. The host does not own the imported SYNC_FD. The reference WorkPool timeout path also returns its still-pending predicate as a success boolean. These are source findings, not a claim that the installed ranchu binary has been proven byte-for-byte equivalent to that tag. Sources: [Android 15 WSI](https://android.googlesource.com/platform/frameworks/native/+/refs/tags/android-15.0.0_r1/vulkan/libvulkan/swapchain.cpp), [guest fence import/wait](https://android.googlesource.com/platform/hardware/google/gfxstream/+/refs/tags/android-15.0.0_r1/guest/vulkan_enc/ResourceTracker.cpp), [generated status dispatch](https://android.googlesource.com/platform/hardware/google/gfxstream/+/refs/tags/android-15.0.0_r1/guest/vulkan_enc/func_table.cpp) and [WorkPool timeout handling](https://android.googlesource.com/platform/hardware/google/gfxstream/+/refs/tags/android-15.0.0_r1/guest/android-emu/aemu/base/threads/AndroidWorkPool.cpp).

## Android API 36 environment and runtime acceptance

An upstream fix explicitly describes this exact never-signaled present-fence query problem: [Mesa commit 7fb31361](https://android.googlesource.com/platform/external/mesa3d/+/7fb31361f443b8a968ec3f6a9fd2fa59f3b3ceb7). Android 16's stable `android-16.0.0_r1` Mesa gfxstream source includes the correction: it distinguishes an absent imported payload from an already-signaled `-1` payload, polls a real imported SYNC_FD with timeout 0, and only forwards unimported fences to the host. See [Android 16 fence status implementation](https://android.googlesource.com/platform/external/mesa3d/+/refs/tags/android-16.0.0_r1/src/gfxstream/guest/vulkan_enc/ResourceTracker.cpp).

The user subsequently explicitly accepted the SDK agreement and authorized installation and validation. The official stable `system-images;android-36;default;x86_64` revision 2 archive was installed into the isolated project SDK; its 844217077 bytes and SHA-1 `829c076e8ff448a336097ae25a355b495ba36e2c` matched the official metadata. `flat-android-api36-install.json` records the successful installation and new AVD creation. The earlier automatic approval rejection in `flat-android-api36-setup.json` remains an earlier, superseded snapshot; the rejected probe itself made no installation or launch.

The actual guest reported Android 16/API 36, build `BE2A.250530.026.D1`, incremental `13818094`, fingerprint `Android/sdk_phone64_x86_64/emu64x:16/BE2A.250530.026.D1/13818094:userdebug/test-keys`. The process mapped `/vendor/lib64/hw/vulkan.ranchu.so`; its pulled binary SHA-256 is `85b07a0e50ca242965619808690af90e4a85e1459df879bf72d2de2441a4d797`, ELF Build ID `2382e7136b40a899db31be33fce20645`. Its strings identify gfxstream and `external/mesa3d` paths. This records the driver actually used, but does not prove that the binary was built from the exact upstream commit or tag cited above. Evidence: `flat-android-api36-device.json/.log` and `flat-android-api36-device-maps.txt`.

`flat-android-api36-acceptance.json` reports `status=passed` with all 11 checks true. The installed APK SHA-256 exactly matched the restored 33/33-audited x86_64 APK listed above. Forty valid samples retained PID 2348 and process start ticks 13290 through the 195-second requested test, with a 194.713-second sample window and 196.628-second collector duration; all 172 ADB reads succeeded. Eleven observations of the same SurfaceView layer advanced strictly from frame 2951 to 7800. After the five-second warmup, RSS remained 190752 KiB, mapping count 3113 and thread count 31 throughout the measured interval.

The coordinator visually confirmed Cube, Plane and lighting in `flat-android-api36-ready.png`, `flat-android-api36-mid.png` and `flat-android-api36-final.png`. The final runtime log contains no fatal, renderer or Vulkan validation errors; the two recorded platform diagnostics concern deprecated ashmem pinning and an unavailable render node with software rendering. SDK `am force-stop` returned 0, subsequent `pidof` returned 1 with no PID, and emulator shutdown completed with process exit 0. This is controlled termination, not an autonomous application-shutdown test. The final process check found no remaining emulator, caffeinate or test macOS/Simulator processes. Supporting records: `flat-android-api36-memory-summary.json`, `flat-android-api36-installed-apk.json`, `flat-android-api36-cleanup.json` and `flat-android-api36-emulator-stop.log`.

Only the SDK system image changed for this successful comparison. The engine retained `vkGetFenceStatus`, the original presentation-record limit and the normal retirement gates; the three production files named above still have no experiment diff. The API 36 result completes the defined Android emulator acceptance without claiming that API 35 passed or that all Android/vendor environments are covered.

## Failures retained and corrected

- Android's existing CMake cache still referenced deleted `AssetProductGroup.cpp` after the source replacement. `flat-android-x86_64-before-configure-fix.log` preserves the real compiler failure. Framework source/header globs now use `CONFIGURE_DEPENDS`; the real CMake/Ninja regression builds, removes/replaces a source and renames a header using only subsequent build commands, then checks the regenerated inventory. An independent negative control removed `CONFIGURE_DEPENDS` from the extracted CMake text and correctly failed on deleted `OldProduct.cpp`; that control's output was console-only. The original Gradle assemble flow is retained rather than adding a forced configure step to packaging. `flat-python-final-tests.log` contains the passing regression and `flat-android-source-discovery-build.log` records the corrected build. This build result alone does not establish Android package/runtime acceptance.
- Earlier renderer module fixture failures remain in `flat-Aurora.Renderer.*-before-type-fix.*` and `*-before-canonical-name-fix.*`. The final module logs are listed separately above; the old failures were not erased or counted as passes.
- The earlier Android SPIR-V parser, surface-transform and prepare-owner retention investigations remain in `verify-ios-world-packaging/verification.md` and their original logs. The shared dispatcher ownership fix remains covered by Core/prepare CPU regressions. The flat-product iOS endurance run provides new measured evidence, without implying that earlier short iOS runs had established memory stability.

## Specification checks and boundaries

Strict OpenSpec validation passed for `simplify-asset-product-storage`, `refactor-asset-product-transactions`, `verify-ios-world-packaging`, `add-skeletal-animation-workflow` and `refactor-aurora-framegraph-contracts` using `OPENSPEC_TELEMETRY=0 openspec validate <change> --strict --no-interactive`. Scoped whitespace checks passed.

Windows was not executed for this final implementation. Historical Windows ProductGroup results are not reused as flat-product acceptance. iOS device signing/provisioning and physical iOS execution remain outside these checks. The physical Android follow-up below covers one Adreno device with explicitly limited physics/package acceptance; it does not establish general vendor GPU coverage. macOS and Simulator execution used this Intel Mac; no Apple Silicon runtime result is claimed.

Per-file replacement is atomic, with explicit failure and batch invalidation. There is no multi-file crash rollback or cross-process snapshot guarantee. Existing referenced CPU/bulk/GPU resources retain their own active data through the established lifetime protocols; this does not restore historical disk generations. No OpenSpec archive was performed; the user-authorized commit/rebase/push workflow is separate from these validation results.

## Physics World cook owner follow-up — host, SDK and APK checks completed

The host previously loaded `JoltPhysics.Builder` without the existing `JoltPhysicsModule` owner, leaving PhysicsBody/config schemas unavailable to checked binary World cooking. AssetTool now declares that existing module dependency and its build prerequisite; no reflection owner or builder link dependency was added. The load follows normal module Init/Start rather than a new metadata-only path.

The authored fixture `python/tests/fixtures/jolt_physics.world` includes every reflected Transform, PhysicsBody, inline ShapeDesc/filter and Physics config member. `run_asset_tool_package_tests.py --with-jolt` adds four real-tool cases: successful binary cooking for iOS and Android, rejection of unsupported deterministic config, and rejection of a missing PhysicsBody field. Success checks the actual checked flat product and its WRLD payload/schema identities; rejection requires a nonzero process exit and no published product. Runtime-generated product UUIDs are discovered rather than embedded in the fixture.

The final coordinated host build passed, then the real integration runner completed all 21 cases with their expected outcomes. The two physics success cases published checked binary Worlds for iOS and Android. The invalid deterministic config and missing-field cases returned exit 1 without publishing a product. Expected negative exits are counted as successful test outcomes, not successful cooks. New evidence is under `build_ios/projects/AndroidPhysicsDemo/validation`: `host-asset-tool-final-build.log`, `cook-tests-final.log`, `cook-tests-final/asset-tool-batch-integration.json` and the individual process logs referenced by that JSON. These final runs supersede the initial successful runs in `host-asset-tool-build.log` and `cook-tests.log` for this follow-up; repeated cases are not added together. The final invocation was:

```sh
python3 python/tests/run_asset_tool_package_tests.py --asset-tool output/bin/Debug/AssetTool --with-jolt --work-dir build_ios/projects/AndroidPhysicsDemo/validation/cook-tests-final
```

The final source-ownership gate completed 26 cases with 25 passes and one skip (`source-ownership-final-tests.log`), retaining the same outcome as `source-ownership-tests.log`. The shared CLI `load_descriptor` now resolves relative project-directory and `.skyproj` arguments before subprocess working-directory changes. Its focused regression passed for generate, package and shader-cook, verifying that the tool, shader preparation, bundle staging and native package paths retain the same absolute project root (`relative-project-path-test.log`). The complete `test_platform_package_configs` and `test_create_package_smoke` rerun then passed all 11 tests (`package-config-tests.log`); the focused case is a subset, not an extra twelfth test. This fixes relative CLI invocation without a platform-specific workaround or changes to stored asset-relative paths.

Android Jolt's existing manifest entry now sets `options_Android.CPP_RTTI_ENABLED=ON`, matching NDK Clang consumers, with `CPP_EXCEPTIONS_ENABLED=OFF`. Both arm64-v8a SDK configurations rebuilt successfully at the pinned Jolt revision with NDK `28.1.13356709` and Android API 31. `jolt-android-build.json/.log` record 130 AArch64 archive members in each configuration, RTTI/no-exceptions/PIC compiler flags, and defined Jolt type-info symbols: 290 in Debug and 287 in Release. These establish the SDK ABI build result; they do not establish a linked APK or device runtime result.

All five strict OpenSpec checks listed above were rerun successfully after the physics owner, Android ABI, CLI-root and shipping-builder delta updates; scoped whitespace checks also passed.

The earlier API 36 emulator scene results above remain historical evidence for that earlier artifact. Physical-device physics/package acceptance was subsequently executed with the Vulkan 1.1-compatible artifact recorded below; it is not inferred from the emulator or the initial archive-only checkpoint.

## Jolt shipping builder boundary follow-up

The initial Android physics package reached native step 304, the final link, which failed: generated static registration requested `sky_module_create_JoltPhysics_Builder`, while the unconditionally built authoring module only emitted the dynamic entry. The retained failure is `build_ios/projects/AndroidPhysicsDemo/validation/package-before-jolt-shipping-builder-fix.log`; it is not counted as an APK build success.

`plugins/jolt/CMakeLists.txt` now enables the builder directory only for `SKY_DEVELOP` or `SKY_BUILD_TEST`, preserving existing tool/sandbox option promotion to developer facilities and existing Jolt test dependencies. `PhysicsBuilderModule.cpp` uses `REGISTER_MODULE_TAGGED`. Runtime/render modules and the shared codec remain enabled. This removes the authoring module at the configuration boundary rather than adding another shipping filter or a factory to an otherwise unwanted module.

Two focused `JoltBuilderConfigurationTest` regressions passed (`build_ios/projects/AndroidPhysicsDemo/validation/jolt-builder-config-tests.log`). Their SDK-free fixture reads the actual Jolt parent/builder CMake files, shared options, module helpers and launcher registration block; only unrelated SDK implementations are stubbed. It configures shipping, developer, tool, sandbox and test modes, then checks actual target presence, launcher link inputs, generated static factories and factory-tag definitions. The second regression preprocesses the actual builder entry with the existing registration macros in dynamic/static modes and verifies that its emitted entry matches the required factory. These are configure/preprocessor checks; the subsequent coordinated host build also succeeded and the public Android package completed with exit 0 and `BUILD SUCCESSFUL in 20s` (`host-asset-tool-final-build.log`, `package-final.log`).

The earlier shipping-boundary `AndroidPhysicsDemo-debug.apk` was 39,766,312 bytes with SHA-256 `3a685aa2752cf61a9ad2b33077b644ce57ee5d3ac19d95afb20d1ca0224afe6d`; its initial audit passed 39 checks (`apk-audit.log`). That archive-only checkpoint did not establish device compatibility. Its first launch exposed the unconditional dynamic-rendering requirement; the final artifact below supersedes it rather than relabeling that earlier result.

At the earlier archive checkpoint the phone was disconnected. It was subsequently reconnected and authorized, and the following checks were actually executed. The prior missing-device state is not the current acceptance status.

## Physical Adreno follow-up — physics/package scope passed, visual quality open

The final compatible APK is 45,422,822 bytes, SHA-256 `a47b4d26afae3b9ae316e79cde9fa026205a1b159d89860795868e5d990d6004`. `build_ios/projects/AndroidPhysicsDemo/validation/apk-audit.json` and `apk-vulkan11-audit.log` report 40/40 checks true. The archive's checked flat products and complete binary World match the seven-actor/18-component source, including the enabled Physics config and four bodies (one static ground, three dynamic boxes). Signature, application identity, arm64 ELF dependency closure, Jolt notice, shader/product bytes and source/builder/compiler/group/scratch exclusion passed. All 54 actual Android SPIR-V 1.3 modules passed `spirv-val --target-env vulkan1.1` (`spirv13-validation/report.json`).

On the physical Adreno (TM) 725, the device reports Vulkan 1.1.128 and the loader 1.3.0; runtime logs record `dynamic=0`, `render-pass2=1` and AUTO selecting `render-pass`. The capability/target integration and final selected host results are detailed in `refactor-aurora-framegraph-contracts/verification.md`: 119 native/C++ passes, contract exit 0, 30 Python passes and 25 ownership passes with one skip. The initial 77/78 Adaptor fixture failure is retained; the corrected final run passed 78/78 without changing the production explicit-mode rejection.

The driver observed PID 9540 for 121.478 seconds. At eight seconds all three boxes had fallen and settled at center y `[0.4800, 0.4902, 0.4800]`, with no physics FAIL log; the 120-second heartbeat retained four bodies, the same heights and 7176 steps. RSS samples from 20-120 seconds stayed within 282036-282044 KiB. Separate SurfaceFlinger observations in `presentation-progress.json` captured four strictly increasing BLAST presentation timestamps over 15.297 seconds. The initial driver surface query failed; this does not constitute 120 seconds of successful frame sampling.

This passes the defined physics/package scope, not full visual-quality or validation acceptance. Visible shadow striping was not isolated by controlled A/B, and the legacy shadow algorithm was unchanged here. The log includes 72037 `VKDBGUTILWARN003` messages without an available Khronos validation layer, so no validation-clean claim is made. BACK did not exit the application; final ADB force-stop did. `device-cleanup.json` confirms the process stopped, screen timeout restored to 60000 ms, stay-awake restored to 0 and the probe removed. Full evidence and known issues are retained in `validation-report.json`; no graceful application exit or general device/vendor coverage is claimed.

Android Back, graceful shutdown and same-process recreation were subsequently fixed and independently verified in [fix-android-application-lifecycle](../fix-android-application-lifecycle/verification.md). That later result does not replace this artifact's historical termination or visual-quality evidence.
