## Final implementation

Editable `.world` JSON now cooks through Content.Builder into a versioned, typed binary `World` product. AssetManager owns the builtin handler for each manager lifecycle; the existing product transaction, index, dependency headers and native closure audit are reused. Cook, binary decode and validation query static persistence schemas without constructing live components.

The ordinary GameApplication loads `game.defaultScene` with ProductsOnly after module Start has registered runtime services and before simulation starts. Simulation advances in the application frame loop. Aurora consumes the World through IGameWorld and only presents it. The fixture keeps the camera, Cube, Plane and lights in source data; its game module has no scene bootstrap.

Scene sessions have exact ownership tokens, real module leases, explicit main view/output/extent and retirement event forwarding. A replaced scene drains before the next generation starts. Internal file symlinks retain logical source identities in both packaging and the native scanner.

## Checks completed

- Host build: CoreTest, FrameworkTest, Aurora.AdaptorTest, AssetTool and ShaderCookTool succeeded.
- Framework: 233 tests passed, including typed binary roundtrip, malformed products, Any constructor capability, hierarchy ordering, subsystem/component attachment order, logical product paths, manager recreation, startup service ordering and failure teardown.
- Aurora adaptor: 77 tests passed, including registration ownership, callback lifetime, repeated entity retirement/reuse and default World presentation without simulation ticks.
- Core file scanning: 3 tests passed, including internal file symlink identity.
- Python packaging/config/fixture regressions: 33 tests passed.
- Real AssetTool integration: all 14 cases returned their expected status. Valid World-only, path roots and internal file aliases cook/audit successfully; invalid paths, missing/wrong products, unknown component schemas and malformed World JSON fail explicitly.
- Simulator: the final x86_64 app built and passed ad-hoc signature verification. Its real startup log confirms Metal initialization and automatic loading of `scenes/cube_plane.world` with 5 actors. LLDB confirmed a 402x874-point native view and 1206x2622-pixel Metal drawable, then detached. After manual unlock, Computer Use confirmed the Cube, Plane and visible lighting in portrait, landscape and restored portrait. Rendering adapted to the changed aspect ratio. The already-running final test application was used throughout; no editor relaunch was needed.
- A fresh standard project was created with `python/tests/create_package_smoke.py --scene cube-plane`. The public iOS package command produced its arm64 app and unsigned IPA and passed the native product dependency-closure audit. The refreshed archive audit passed all 22 checks: CRC, app/archive equality, binary World framing/identity/checksum, 54 Metal blobs, arm64 iOS Mach-O dependency closure and no authoring World/shader source or compiler/cook runtime leakage.

## Local evidence

Ignored project: `build_ios/projects/IOSWorldCookDemo`.

- `framework-tests.xml`, `adaptor-tests.xml`, `core-file-tests.xml`
- `package-unit-tests.log`, `asset-tool-tests.log`
- `ios-package.log`, `ios-package-audit.json`
- `simulator-configure.log`, `simulator-build.log`, `simulator-run.log`, `native-resolution.log`
- `validation-report.json`
- Device archive: `build/ios/IOSWorldCookDemo-unsigned.ipa`

## Boundaries

Source JSON remains the editor format. Binary v1 is strict: unknown/unregistered schemas and unsupported reflected containers fail explicitly. Custom game component metadata must be registered in the offline AssetTool process as well as runtime; this change does not add a generic project metadata loader. Directory symlinks are not recursively scanned; internal file aliases are supported.

The fresh fixture uses builtin Cube/Plane/materials; separate real AssetTool cases cover image-product cooking and dependency validation. Simulator dependencies use a matching x86_64 cache on this Intel Mac; the public recipe parameters also allow arm64 but that architecture was not built here. Device signing/provisioning and physical-device GPU execution are outside this verification. No commit or OpenSpec archive was performed.

## Initial Android follow-up completed

The same project, source World, empty template game module and public `package_project.py package` command produced debug-signed arm64-v8a and x86_64 APKs. Android 15/API 35 x86_64 Emulator 37.2.12 ran the final x86_64 APK through HVF and SwiftShader Vulkan 1.3. Its startup log confirms binary default World loading with 5 actors. ADB screenshots at 8 and 180 seconds show Cube, Plane and lighting with the corrected projection. The monitored run survived 197 seconds; after warmup, RSS stayed at 181,648 KiB (177.4 MiB) and mapping count at 2,958. The same PID was still alive during the final read-only check.

Four issues were fixed in their existing owners:

- Android's default asset filter discarded `.groups` while retaining product indexes. The first fix changed the launcher filter and verified the corrected member bytes. The visible-layout follow-up below supersedes that exception and restores Android defaults.
- Slang's optional SPIR-V source declarations used a source-language operand rejected by the emulator's software Vulkan parser. A checked private finalizer removes only `OpSource`/`OpSourceContinued`, preserves the remaining module and entry point, and publishes changes transactionally. The policy participates in the SPIR-V producer identity so existing artifacts recook; Metal/DXIL identities are unchanged.
- RendererRuntime and the final scene output mapping both applied the surface transform. Camera projections now remain logical and FrameComposer/SceneRenderFeature apply the output mapping once. A rotate-90 regression covers this contract on the native Windows test path; this Mac run validates the Android result through actual pixels, rather than claiming that Windows-only test ran here.
- Moving a small `std::function` did not guarantee that its original captured shared owner was cleared. The drain callback retained its own prepare state and merged products. Both dispatcher completion paths now use `std::exchange(callback, {})`, clearing the source explicitly while retaining an independent invocation owner. Temporary lifetime instrumentation was removed.

Final checks:

- CoreTest: 314 passed, including small captured-owner release for empty/nonempty batches and owner destruction during drain notification.
- PrepareProductsTest: 13 passed. The new lifecycle regression runs 256 serial and 256 parallel completions, proving kernel capture expiration and previous product retirement. It failed before the dispatcher fix and passed afterward.
- Aurora.ShaderTest: 60 passed, including real Vulkan and Metal shader creation and malformed-module, atomicity, policy and optimization/stage coverage.
- Python package/config/fixture regressions: 33 passed.
- Each final APK passed all 32 archive/signature/manifest/ABI/ELF/product checks. The World is binary `WRLD` v1, its identity/checksum and 5 actors/10 components match the source, and all 54 target shader artifacts match the cooked bytes.
- Each distribution passed 108 NDK `spirv-val` invocations: 54 cooked payloads plus the exact 54 packaged payloads. Indexes and shader artifacts match byte for byte. No source declarations, shader source, authoring World JSON or compiler/cook runtime leaked into either APK.

Evidence under the ignored project:

- `android-validation-report.json`, `android-x86_64-package-audit.json`, `android-arm64-package-audit.json`
- `android-spirv-validation.json`, `android-arm64-spirv-validation.json`
- `android-core-tests.xml`, `android-prepare-cpu-tests.xml`, `android-shader-tests.xml`, `android-package-unit-tests.log`
- `android-memory-final.json`, `android-final-runtime.log`, `android-final-process-logcat.log`
- `android-scene-final.png`, `android-scene-after-long-run.png`
- `build/android/IOSWorldCookDemo-debug-x86_64.apk`, `build/android/IOSWorldCookDemo-debug-arm64-v8a.apk`

The arm64 APK was built and audited; execution used the x86_64 software Vulkan emulator. Physical Android hardware, GPU performance and vendor driver coverage remain unverified.

## Why the earlier iOS run did not expose these two runtime issues

The memory retention is a shared Core bug, not an Android-only allocator defect. The actual iPhoneOS arm64 and iPhoneSimulator x86_64 SDK preprocessing both select libc++ ABI v1 without the optimized-function ABI. Their small-function move branch can clone the callable while leaving the original captures alive, just as the NDK does. A host libc++ reproduction and the failing-before-fix CPU regression confirm this ownership assumption was invalid. Android's Scudo guarded mappings made the accumulated products fail quickly: the earlier run grew from roughly 20,000 to 66,000 mappings and aborted around 82 seconds despite available guest RAM. The earlier iOS check covered startup, pixels and orientation, without a repeated-product lifetime assertion or a sampled memory curve; it cannot establish that iOS was immune. Differences in allocation and frame scheduling may alter the time to failure, but no iOS timing comparison was measured. The shared dispatcher fix applies to iOS as well. Evidence: `ios-function-abi-check.json` and the lifecycle regression.

The projection issue requires a non-identity surface transform. Vulkan exports the selected swapchain transform, and this Android surface exercised a quarter turn. MetalSwapChain explicitly exports `SurfaceTransform::IDENTITY`; iOS's native view/drawable handles orientation. Applying identity twice was visually harmless in the earlier Metal run, while applying the Android output rotation twice distorted the scene. Removing the duplicate camera mapping restores a single output-transform owner for both paths.

The packaging and shader parser failures also follow different platform paths: Android AAPT filters hidden asset directories, whereas the iOS bundle copy preserved them; iOS consumes Metal shader products and never passes SPIR-V source metadata to SwiftShader. The earlier iOS artifacts were not rebuilt or rerun after this Android follow-up, so these results do not claim a new iOS endurance test.

## Visible product-group layout follow-up

The product store now uses ordinary `groups/` on every platform. A private path helper supplies group, ownership and index/publish lock paths; no public API, target or binary schema changed. The Android Gradle template again uses its defaults and has no asset-filter override. The old hidden-root payloads are not read or automatically migrated. Obsolete ownership explicitly blocks stale flat-product fallback, including after failed first publication in the new layout. Re-cooking publishes a valid new owner/generation; existing current-layout flat-product migration and retained snapshot/bulk behavior remain covered.

The original hidden root was introduced by `add-skeletal-animation-workflow` D4a and commit `ce2d9087` (2026-10-26, `refactor: publish immutable asset product generations`). Transaction correctness depends on immutable generations and atomic manifest publication; directory hiding has no functional role.

Validation on 2026-10-28:

- Existing macOS Debug build: FrameworkTest, AssetTool and Content.Builder.Module succeeded.
- FrameworkTest: 234 passed, including new-layout disk paths, obsolete-owner rejection beside valid stale flat products, failed publication, recook recovery and retained bulk snapshots.
- Python package/config/fixture tests: 30 passed. Animation packaging: 3 passed, including actual source-free AssetTool group auditing. All 14 real AssetTool cook/audit integration cases returned their expected statuses; generated fixtures contain `groups`, with no `.groups` directories.
- The real SDK `aapt package` regression passes with no `--ignore-assets` argument. Group index, ownership, manifest and payload bytes survive; hidden files/directories and backups remain excluded. The template is checked for absence of a filter override.
- Fresh x86_64 and arm64-v8a APKs built through the public package command. Each passed 33 checks, including signature, ABI/ELF closure, binary World identity/checksum/5 actors, all 54 shader artifacts matching cooked bytes, visible group presence and obsolete hidden-group absence.
- Independent source review found no blocker. Strict OpenSpec validation and whitespace checks passed. No editor, emulator or application was launched for this layout-only follow-up; the earlier 197-second runtime evidence belongs to the previous APKs and is not represented as a runtime test of these new files. No new iOS build was performed.

Evidence under the ignored project: `groups-layout-validation.json`, `groups-{host-configure,host-build,framework-tests,python-tests,animation-tests,asset-tool-tests}.log`, `groups-framework-tests.xml`, `groups-android-{x86_64,arm64}-audit.json` and `groups-asset-tool-integration/asset-tool-batch-integration.json`.

New artifacts: `build/android/IOSWorldCookDemo-debug-x86_64-groups.apk` and `build/android/IOSWorldCookDemo-debug-arm64-v8a-groups.apk`. Earlier APKs/reports remain available as runtime evidence. The example's obsolete products were preserved outside the products tree under `validation-before-groups-layout/` before recooking. Other projects with old `.groups` products must recook and repackage; old/new publishers must not share a live bundle concurrently.
