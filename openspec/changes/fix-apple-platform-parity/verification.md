# Apple platform verification

Date: 2026-11-02. Baseline: `60309307` on `dev_refactor_rhi`; the requested fetch/rebase found `origin/dev_refactor_rhi` already current. Implementation is authorized. This record captures verification before the reviewed source commit; archive and push require their own authorization.

## Environment and evidence

Intel macOS host, Xcode 16.4, macOS 15.5 and iPhoneOS 18.5 SDKs. Existing `cmake-build-debug` uses Xcode and `build_3rd/MacOS-x86`; its generator and dependency root were preserved. The selected paired device is iPad mini 6. The existing Apple Development identity/team was reused; no certificate was created or revoked.

Transient logs and artifacts live under `output/apple-platform-fix-20261101/`. Source-controlled tests and this record describe their meaning; the output directory is intentionally ignored.

## Review scope

All implementation, regression cases and OpenSpec artifacts belong to this one change. Review in this order:

| Area | Behavior and source scope |
|---|---|
| Apple build and distribution | `python/third_party.py`, the OpenSSL/CPython builders, shared third-party CMake discovery, `python/package_project.py` and Apple launcher templates: SDK/architecture identity, selected configuration/version, pre-sign resource staging and validated archive/export. |
| Framework runtime | `engine/framework/platform/macos/` and `platform/ios/`: bounded inactive waiting, bundle resource paths, UIKit terminal retirement and window/scene teardown. Existing targets and module ownership are retained. |
| Native validation tools | Colocated Cocoa/UIKit tests, `python/tests/fixtures/ios_platform/`, the iOS runner and Python contracts: production target reuse, signed device execution and explicit automatic/manual modes. |
| Documentation and specifications | `docs/features/game-project-build.md`, `docs/architecture/platform-support.md`, this change's three domain deltas and the active owner/index updates. Main specs are not synced or archived in this review. |

The manual `Sky Input Test` homepage is a UIKit fixture in `engine/framework/test/platform/ios/main.mm`. Its labels/buttons use native controls and invoke the production platform input API. It does not exercise `engine/ui` rendering or introduce a shared cross-platform test homepage. The existing iPad navigation demo is a separate application.

Cleanup retains the tested native lifecycle code and signing order. Build products, development credentials and transient logs are excluded from the source scope. The final simulator rerun remains unchecked in tasks.md; physical keyboard/IME, GPU and release-distribution limits remain listed below.

The cleanup review on 2026-11-02 also aligned the build guide and platform matrix with the Resources layout, documented the actual archive/export CLI, reused the existing Apple target-option helper and removed duplicate runner mode expressions. Runner 24/24 and third-party 23/23 passed after these edits, including additional SDK/mode and signed-metadata rejection subcases. CLI help, all three documented distribution examples and Python syntax were checked. Native sources, lifecycle state and signing order were unchanged during cleanup, so the native results below remain the executed evidence rather than a claimed rerun.

## Executed checks

| Check | Result | Evidence under the output directory |
|---|---|---|
| Portable platform/package/Apple contracts | 146 tests, zero failures/errors, one Windows-only skip | `python-contracts-final.log` |
| Device runner after the final install-failure cleanup correction | 24/24 | `runner-final.log` |
| Precommit Win32 portability contracts | Third-party module 24/24 on Mac; includes Win32 option/tool isolation and legacy cache-key equivalence | `windows-portability-contracts.log` |
| Source ownership | 26 tests, one Windows-only skip; production targets also configured/built | `source-ownership-final.log`, native configure/build logs |
| Production macOS Framework + FrameworkTest | Build success; 303/303 tests, including 22/22 Cocoa, zero skips | `macos-final-build.log`, `macos-framework-final.log/.xml` |
| Pinned simulator dependency bootstrap | ios-cmake/SFMT/RapidJSON/LZ4 through the public entry, separate SDK/architecture output; Debug and Release SFMT/LZ4 built and validated | `simulator-bootstrap.json`, per-package bootstrap logs |
| Simulator native baseline | 21/21; uninstall/shutdown/delete exit 0 | `ios-native-runner.log` and bootstrap/native baseline records |
| Expanded simulator UI suite before final reentry fix | 31/32; the failing reentry assertion was reproduced on iPad and fixed afterward | `ios-native/test.log` |
| Final iPad native suite | 33/33: 24 native UIKit + 9 policy; COUNT=33, EXIT=0, launcher exit 0; automatic app removed | `ios-device-final.log`, `ios-device/automated-final/` |
| Persistent manual input app | Signed, installed and launched on iPad mini 6; kept installed; no automatic result claimed | `ios-manual-install.log`, `ios-device/device-result.json` |
| macOS development archive/export | Application archive, exported signed app, exact ID/version, resources and signature checks pass | `macos-export-final.log`, `AppleExportValidation/build/macos/distribution/` |
| Exported macOS application startup | From `/private/tmp`, loads resources/modules, initializes Metal and exits 0 after five frames | `macos-export-launch.log` |
| Formatting/source rules | Nine changed native files ASCII; clang-format dry run, no added exception/RTTI operations, diff check pass | `format-final.log`, `source-checks-final.json` |
| OpenSpec strict validation | 60/60: 28 changes + 32 canonical specs | `openspec-validation.json` |

The 146-test suite includes the new third-party cases (23), Apple export/distribution cases (15), native packaging cases (29) and runner cases (24). The subsequent runner-only rerun verifies the final cleanup correction. Real Mach-O CPU/SDK negative fixtures, unsafe symlink/executable rejection, archive identity rejection and ordinary resource staging are exercised. The single Windows-only skip is not reported as native Windows execution.

## Failures that drove the fixes

- The minimal UIKit fixture initially omitted production dependency setup and loaded GoogleTest below its version-owning project. It now uses shared discovery, generated plugin options and production LZ4, without compiling another target's implementation sources.
- New Cocoa timer tests initially failed despite event polling. Bounded CFRunLoop service plus event draining and quit wake fixed them; the final 303-case run includes the bundle/loose-tool path contract.
- The first expanded UIKit run passed the old 21 cases and failed every new dialog case. The test suite occupied a main-dispatch block, preventing nested UIKit transitions from completing. A one-shot native timer and fixture-only animation control restore real presentation and first-responder assertions.
- Reentrant reopening then failed on simulator and iPad because polling delivered a terminal before UIKit released its presenter. The iOS-private handle now claims the first terminal and posts it only after retirement. Hide/window/scene teardown flush the same decision synchronously. A further CHANGED callback case proves nested confirmation cannot bypass this boundary; stale callbacks cannot affect a replacement request.
- A focused passing iPad run lacked its final console markers. The host now returns to the native loop for 0.25 seconds after flushing markers before exiting. The runner still requires both markers, a positive count and successful launcher status.
- Manual-mode host cleanup initially cleared the scene used by automatic disconnect/reconnect injection. That failed run is retained as `ios-device-before-host-fix.log`. The two modes now keep their respective scene state, without weakening native assertions; the next full run passed all 33.
- The first signed macOS archive placed data below Contents/MacOS, where CodeSign treated it as nested code. Data now lives in Contents/Resources, with the runtime filesystem, staging and receipt following that location. The next archive exposed dependency install products producing a generic archive. Archive-mode SKIP_INSTALL plus ApplicationProperties validation now reject that result and produce a real application archive.
- The first extra exported-app startup check used a lean validation preset with Jolt disabled but a default module list still requiring Jolt. Only the ignored validation project's module list was aligned with its selected plugins, following the existing package smoke fixture; no production module-list behavior was changed. The project was then archived/exported again before the final successful startup check, preserving signature and receipt validity.

## Manual app and remaining boundaries

`Sky Input Test` (`com.skyengine.platform-tests`) is installed on the user-selected iPad mini 6. Single line, Multiline and Password invoke the production UIKit dialog. Password content is never shown, retained or logged by the manual host. Its compiled manual default also applies when opening it from the Home Screen. It is separate from the previously demonstrated navigation app. The user confirmed on 2026-11-02 that the app is visible and all three input modes, confirmation/cancellation and reopening work. This does not establish a separate physical-keyboard or Chinese-IME matrix; automatic marked-text/delegate injection is recorded separately.

The final 33-case patch was validated on the physical iPad. A final expanded simulator rerun was not started after the reported Simulator popups; the last simulator result remains 31/32 before the reentry fix. Existing Mac crash reports inspected at that point named services under launchd_sim; none identified the engine test host. That does not identify the exact popup with certainty. The passing iPad test log still contains teardown appearance-transition warnings; no crash or test failure is inferred away from those warnings.

Development signing/archive/export does not establish App Store/TestFlight acceptance, notarization, release-profile distribution or full macOS arm64 dependency builds. This run does not execute a signed iOS game archive/export; the iPad fixture independently verifies production Core/Framework, Xcode signing and actual install/launch. Full CPython cross-host builds remain explicitly unsupported. GPU surface retirement, rotation/long-duration presentation and the P02-IOS matrix remain with their existing owner; native platform tests and the earlier navigation demo do not close them.

## Windows regression assessment

Review found no known Windows production-code blocker. Framework runtime changes are limited to Cocoa/UIKit; no shared runtime ABI, Windows backend, editor UI or RHI source changed. Apple SDK/architecture and Mach-O checks are target-gated, and the Win32 dependency cache-key payload remains unchanged. The only tool-source validation applies to ios-cmake, which is not selected for Win32.

Windows still warrants a targeted native regression because this change also moves the shared third-party finder, adjusts the game template's resource deployment setup and propagates dependency build/cache failures across platforms. Slang host-generator identity/options also affect Android/HarmonyOS cross-builds hosted on Windows. Mac-hosted CMake contracts using a Windows system name do not exercise the VS2022 generator or real PE linking.

The precommit review found and corrected two Slang test fixtures that used an unsuffixed bootstrap filename on Windows. They now use the actual host's executable suffix. The third-party module then passed 24/24 on Mac, including explicit Win32 contracts for unchanged cache identity, no Apple CMake options, no Xcode inspection calls and native Win32 Slang bypassing the cross-host generator helper. This is portable contract evidence, not Windows execution.

The minimum Windows checks are:

1. Close SandboxEditor; retain the existing `cmake-build-debug` generator and `3RD_PATH`. Run `cmake -S . -B cmake-build-debug`, then build `Framework` and `FrameworkTest` in Debug. Run FrameworkTest and the source-ownership check.
2. Run the existing Win32 third-party strict preflight and one small dependency smoke (SFMT), verifying both cached reuse and build/install success before metadata publication. A full OpenSSL/CPython/Slang rebuild is not required for this change.
3. Generate an ignored validation game and use its native `windows`/`windows-develop` CMake preset. Build only Launcher; verify configs and products are deployed beside the executable, then update a fixture and rebuild to verify one staging owner and fresh content. Windows `package`/`generate` CLI support is not added by this change.

These checks remain unexecuted on a Windows host in this session and are tracked as task 6.4. Full DX12/GPU, editor UI and long-duration matrices are outside this targeted regression. A local source commit may preserve this reviewed Apple work with those limits explicit; it does not close Windows acceptance or authorize archive/push.
