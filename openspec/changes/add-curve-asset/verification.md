# Verification — architecture review fixes, 2026-11-10

## Build

Used the existing `cmake-build-debug` Xcode Debug configuration and preserved its `3RD_PATH` (`build_3rd/MacOS-x86`). No commit or archive was performed.

- `cmake -S . -B cmake-build-debug`: passed, including source ownership checks.
- Built Curve core/assets/builder/editor libraries and modules, Framework, EditorCore/Widgets/Shell, UI, Animation.Editor, Particle.Editor/Builder, SandboxModule, SandboxEditor and AssetTool, plus the affected test executables: passed.
- Incremental builds after final dependency wiring and startup-failure cleanup, including Aurora.CoreTest, FrameworkTest, SandboxEditor and AssetTool: passed.
- Build logs: `/tmp/sky-review-configure.log`, `/tmp/sky-review-build.log`, `/tmp/sky-review-build-final.log`, `/tmp/sky-review-build-cleanup.log`.

Xcode initially could not write its existing DerivedData cache under restricted execution. The authorized build succeeded. No generator or dependency cache was replaced.

## Native tests

| Executable | Passed | Skipped | Failed |
|---|---:|---:|---:|
| Animation.EditorTest | 9 | 0 | 0 |
| Aurora.CoreTest | 128 | 0 | 0 |
| Curve.AssetsTest | 9 | 0 | 0 |
| Curve.BuilderTest | 1 | 0 | 0 |
| Curve.EditorTest | 19 | 0 | 0 |
| CurveTest | 8 | 0 | 0 |
| EditorCoreTest | 190 | 0 | 0 |
| EditorShellTest | 149 | 0 | 0 |
| FrameworkTest | 288 | 22 | 0 |
| Particle.BuilderTest | 6 | 0 | 0 |
| Particle.EditorTest | 7 | 0 | 0 |
| Particle.WorldTest | 8 | 0 | 0 |
| UICoreTest | 194 | 0 | 0 |

The 22 skipped Framework tests are opt-in Cocoa window tests. Aurora.CoreTest initially lacked Metal access in restricted execution; all 128 cases passed after rerunning with system graphics access. XML and logs are under `/tmp/sky-review-tests`; the unsuccessful restricted Aurora run is retained in its `sandbox/` subdirectory.

Total: 1,038 cases, 1,016 passed and 22 skipped, with no failures. FrameworkTest was rerun after the final startup-cleanup change.

Regression coverage includes canonical checkpoint restoration, failed reload/save preservation, no-op edits, real source-file bounds/atomic replacement/staging cleanup, input channels/size/depth, extreme interpolation, exterior channel normalization, drag/history/model replacement, primary selection, out-of-bounds release, virtual-list scrolling, command shortcut eligibility, preview layout and compatible draw batching.

## Native cook, audit and editor

- `python3 python/tests/run_asset_tool_package_tests.py --asset-tool output/bin/Debug/AssetTool --work-dir /tmp/sky-review-assettool --with-jolt`: 21 cases passed, including expected nonzero exits for invalid products/sources and missing builders. Report: `/tmp/sky-review-assettool/asset-tool-batch-integration.json`.
- Additional isolated Curve/manifest matrix: 7 cases passed. Valid Curve cook succeeds; channels=5 recook fails and preserves the old product byte-for-byte; product audit succeeds after removing engine/project sources and loads no modules. Missing manifest, malformed JSON, unknown build option and missing selected module fail startup without falling back to cook. Commands and logs: `/tmp/sky-curve-native-review-boo3_1wv/results.json`.
- `SandboxEditor --project output/ParticleDemo/ParticleDemo.skyproj --asset CE6A10E6-C9E1-49BE-B8A6-7FFD5A94299A --frames 60 --console`: exited 0; Curve.Assets, Curve.Builder and Curve.Editor loaded, and the Curve document opened. Log: `/tmp/sky-review-editor-smoke.log`.
- Interactive edit semantics are exercised through actual widget pointer/key/command tests; the bounded native run verifies startup and asset opening, not a manual drag/save session.

## Sanitizers and configuration

AddressSanitizer reproducers were rebuilt against the fixed Curve sources. `/tmp/curve_fixed_probe`, its `overflow` mode and `/tmp/curve_fixed_depth_probe` all exited 0 without sanitizer errors. Invalid channels are rejected, 120,000 nested arrays parse without a call-stack overflow, oversized sources are rejected, unused exterior channels are zero and the representable extreme-value midpoint is zero. Logs: `/tmp/curve_fixed_probe.log`, `/tmp/curve_fixed_channels.log`, `/tmp/curve_fixed_depth_probe.log`.

- `python3 python/tests/test_module_configuration.py`: 5 tests passed, including 17 shared C++/CMake selection fixtures, shipping/development/test targets, link visibility and hybrid static factory/deployment wiring.
- `python3 python/tests/test_cmake_source_ownership.py`: 25 passed, 1 Windows-only skip.
- `openspec validate add-curve-asset --strict`: passed.
- `git diff --check`: passed at the integration checkpoint.

## Limits

The third-party preflight reported 25 stale cache fingerprints and no missing outputs. The successful builds/tests used those existing outputs; dependencies were not rebuilt in this task. Windows and a fully static native tool/game package were not executed. Static factory lifetime is tested natively through ModuleManager; shipping/static target selection is covered by CMake fixtures. Development AssetTool retains legacy shared modules as explicitly documented hybrid dependencies.

Existing Particle inline curve migration, gradient assets, additional tangent modes and LUTs remain outside this change.
