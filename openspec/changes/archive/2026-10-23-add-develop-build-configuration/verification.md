# Verification — 2026-10-23

Baseline: `0bf7b53f` (`fix: avoid repeated Vulkan surface option queries`). Develop implementation remains uncommitted and unarchived pending user confirmation.

## Environment and configuration

- Windows x64, VS 2022/MSVC 14.44, NVIDIA GeForce RTX 3080. Existing `cmake-build-debug` reused.
- Actual cache uses `D:/Code/Engine/SkyEngine/build_3rd/Win32`. The older AGENTS example path `D:/Code/sky3rd_win32_output/Win32` does not exist on this host.
- Root `cmake --preset windows-develop` succeeded. Generated MSVC projects were inspected: `/O2`, `/Ob2`, `/Zi`, `NDEBUG`, Release `/MD`, `SKY_BUILD_DEVELOP=1`, `SKY_ENABLE_ASSERT=1`, `SKY_ENABLE_GPU_VALIDATION=0`, resource names on. No `_DEBUG` or Debug third-party archives in Develop links. Executable/module PDB files exist.
- SDK-free Ninja/Ninja Multi-Config tests exercise AUTO policies, explicit overrides, invalid values, exact Develop versus Release imported-target fallback, multi-library dependency selection, and game preset/tool-discovery compatibility.

## Build and runtime evidence

| Check | Result |
|---|---|
| Develop SandboxEditor + SandboxModule + manifest dependencies | Built; Vulkan project opened, 120 frames, exit 0 |
| Develop CoreTest | 306 passed, including evaluate-once and failing-invariant death test with NDEBUG |
| Develop EditorCoreTest / EditorShellTest / EditorRenderTest | 177 / 136 / 8 passed |
| Develop native FrameOutputs suite | 111 passed; 9 expected Vulkan-only cases skipped for DX12; 0 failures |
| Debug CoreTest + Aurora.Vulkan build | Built; 306 Core tests passed |
| Release CoreTest + Aurora.Vulkan build | Built; 306 Core tests passed |
| RelWithDebInfo CoreTest + Aurora.Vulkan build | Built; 306 Core tests passed |
| Develop AssetTool + runtime builders | Built; cook-all produced 5 products, 0 failures, exit 0 |
| Develop ShaderCookTool | Built; automatic discovery selected Develop; cooked and self-validated 8 Vulkan entries |
| Generated DevelopSmoke game | Independent configure/build/deploy succeeded; launcher and game/engine modules under build/windows/bin/Develop |
| Generated game execution | Loaded game module and renderer, created swapchain, ran 120 frames, normal shutdown/exit 0 |
| Develop configuration/tool Python suite | 5 passed |
| Slang runtime import suite | 2 passed; 1 Apple symlink test skipped on Windows |
| Shader project suite | 4 passed |

The generated game initially failed to link because its Windows GUI target supplied `main` without the appropriate CRT entry option. The launcher template now uses the same `mainCRTStartup` option as SandboxEditor. Runtime verification used a temporary BindTick callback in the ignored generated project to request exit after 120 frames; original generated source was restored and the normal executable rebuilt afterward. No probe code remains in tracked sources or the generated launcher.

The editor log confirms `VK_EXT_debug_utils` is enabled while no validation layer or debug messenger is enabled under the Develop AUTO policy. Thus resource naming no longer depends on enabling validation. The native FrameOutputs harness independently requests validation for its tests.

Raw logs, gtest XML, compiler-policy inspection, generated project and shader products are under ignored `output/develop-verification/`. Relevant logs: `build-develop-final.log`, `editor-vulkan.stdout.log`, `gpu-tests.log`, `CoreTest-<config>.log`, `asset-cook.*.log`, `DevelopSmoke/build/shader-cook/default.log`, and `game-bounded.*.log`.

## Limits and existing issues

- Dependency preflight on the actual cache reports 7 missing/stale metadata entries: miniaudio, enet and Slang pins differ; vulkan_headers, nanosvg, OpenSSL and CPython are absent from metadata. It reports no missing outputs among recorded packages. The configured Windows targets compile/link/run, but this does not certify a fully refreshed third-party cache; CPython is disabled and was not natively built.
- Existing `test_native_package_artifacts.py` has 4 passes, 2 failures and 3 errors on this Windows host. The exact same five cases fail against the committed baseline: POSIX path assumptions, Apple executable permission checks, and symlink privileges. No new failure was introduced; unrelated packaging/error-handling code was left intact.
- Apple/Android/Linux native builds, signing and packaging were not run. Portable configuration and Slang mapping fixtures are not a substitute for platform SDK/device validation. Existing platform launcher coverage is unchanged. Windows publishing packages remain outside this change.
- `SKY_ASSERT`/`SKY_UNEXPECTED` use the new policy. Legacy standard-library `assert` calls still follow NDEBUG; this change does not migrate unrelated allocator contracts.
- This verifies the build/runtime mode, not a new right-drag frame-time benchmark. Prior Vulkan timing measurements remain documented in `optimize-vulkan-surface-queries`; no new latency percentage is inferred from test duration.

## Review

No module/target ownership change, directory move, exception handling, RTTI cast or new product target was introduced. Diagnostics are independent of CRT selection; descriptor-layout-affecting resource-name settings are documented as requiring a consistent rebuild. Usage is in `docs/features/develop-build.md`. Unrelated research documents and `docs/README.md` edits are excluded from this change.

## Visual Studio solution follow-up

The user reported duplicate Core items when opening `build/SkyEngine.sln`, followed by missing `../../core/Develop/Core.lib`. Inspection found both native Core and Framework projects missing from that saved solution. Fresh generated solutions had two same-parent name collisions: the Core/Framework library projects and virtual folders created by their test targets under `Engine/Foundation`. This also affected the prior `cmake-build-debug` solution; target-oriented builds had not exercised VS solution loading.

The two test `CURRENT_FOLDER` values now use `Engine/Foundation/Tests/Core` and `Engine/Foundation/Tests/Framework`. No source directories, target names or dependencies were changed. Regenerated `build` with its original CLion-bundled CMake 4.0.2 and `cmake-build-debug` with CMake 3.31.11. Checked unique same-parent names, restored project entries, matching GUID references and Develop Build.0 mappings.

Executed Visual Studio 2022 `devenv.com build/SkyEngine.sln /Build "Develop|x64"`: exit 0, **88 succeeded, 0 failed, 42 up-to-date, 3 skipped**. Both missing static libraries now exist in `build/engine/{core,framework}/Develop`; ShaderCookTool, UIShaderTest, SandboxModule and SandboxEditor linked successfully. Evidence and pre-fix solution snapshot: `output/develop-verification/vs-solution/` (`vs-build.log`, `audit.json`, `SkyEngine-before.sln`).
