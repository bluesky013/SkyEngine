# Verification

Passed for the recorded host and physical-device lifecycle scope. Evidence is under the ignored `build_ios/projects/AndroidPhysicsDemo/validation/exit-fix-20261029/` directory (`E`), with final results in `E/upstream-final` (`U`). `U/verification-report.json` references the complete evidence set and records all checks passed.

Device results apply to the APK hash below. They preceded the final include/format cleanup; no rebuilt APK or repeated device run is claimed for that cleanup. Windows, other device/OS combinations and comprehensive visual acceptance were not tested by this change.

## Host and dependency checks

**889 unique gtests passed**, plus renderer contract exit 0. Python ran **34 tests: 33 passed, 1 skipped** (`U/python-tests.log`). Repeated executions are not added to these counts.

| Suite | Passed | Evidence under E |
| --- | ---: | --- |
| FrameworkTest | 251 | upstream-final/framework-tests.log |
| JoltPhysicsTest | 58 | session-final/JoltPhysicsTest.log |
| Animation.WorldTest | 10 | session-final/Animation.WorldTest.log |
| EditorCoreTest | 184 | session-final/EditorCoreTest.log |
| EditorShellTest | 148 | session-final/EditorShellTest.log |
| EditorWidgetsTest | 42 | session-final/EditorWidgetsTest.log |
| Aurora.CoreTest | 116 | session-final/Aurora.CoreTest.log |
| Aurora.AdaptorTest | 80 | session-final/Aurora.AdaptorTest.log |

The separate contract log is `E/session-final/Aurora.Renderer.ContractTest.log`. Framework covers checked Main-role release, explicit shutdown, late DESTROY, failed initialization and stable metadata ownership. Core/Adaptor cover terminal-event draining; Python covers public launcher materialization.

Actual Gradle dependencyInsight resolves **Activity 1.2.3 through AppCompat 1.3.0** (`U/androidx-activity-dependency.log`), so the explicit API 33 bridge remains necessary. GameActivity 4.4.2's native default key filter still accepts Back, so the legacy Back route remains necessary as well.

The actual official 4.4.2 AAR was inspected: `nativeHandleOwner` serializes enclosing NativeCode lifetimes and rejects obsolete Activity callbacks; GameTextInput remains process-shared. The launcher contains no parallel gate/proxy, old-version insets workaround or vendor source patch. Matched crash symbolization and replacement artifact evidence are retained in `E/session-final/gameactivity-insets-native-evidence.json`; its runtimeValidation references the final report.

## Package and physical-device results

The official public package passed **40/40 audit checks** (`U/apk-audit.json`): signature/archive integrity, ABI/native dependency closure, complete typed World/products and shader payloads, with source/compiler/group/scratch exclusion.

- Production APK: **72,430,302 bytes**, SHA-256 `69a7813c665d3a93708911a9a73ce1acc102b49f61148523c4d8e9e689af6ef5`.
- Application `com.skyengine.androidphysicsdemo`; arm64-v8a; minSdk 29, targetSdk 33; physical Adreno 725 on Android 15/API 35.
- The separately identified native-probe APK has SHA-256 `e95726ea836d79514dbc468fef699ac2cf72f3206f77d2ecaa451e03f7166ed6`. Probe observations are not attributed to the production APK.

| Verification | Result | Evidence under U |
| --- | --- | --- |
| Ordinary lifecycle | **64/64 passed** | ordinary/ordinary-result.json |
| Canceled/committed gestures | **32/32 passed** | gestures/gesture-result.json |
| Explicit exit, failed Init and recovery | **27/27 passed** | native-probe-final/result.json |
| Restored production-package smoke | **8/8 passed** | final-smoke-result.json |

Ordinary verification retained PID **24868** and process startTicks **1530655** across Back/relaunch, HOME and two clear-task replacements. Back performed ordered observer shutdown/renderer stop and Activity destruction. HOME paused physics for **23 seconds** without shutdown (CPU delta **86 ticks**), then restored the same Activity/session. Both clear-task replacements ordered old shutdown before new World/physics PASS without an owner assertion.

The canceled gesture retained that PID and ActivityRecord. The committed gesture performed clean Activity destruction, leaving the process alive. PID/device-epoch-scoped crash and lifecycle logs found no new native fatal, DEBUG trace, assertion, poll failure or role-release error. The probe separately verified exit through ISystemNotify.SetExit after physics PASS, one Shutdown/renderer stop/native DESTROY acknowledgement, deliberately failed module Init, and successful same-PID recovery (**29097**).

The three probe source files were restored byte-for-byte using their backup SHA records. The clean public rebuild (`U/package-clean-final.log`) produced the identical production APK hash; installed identity matched. The final smoke confirmed no temporary probe/controlled-failure/file-flag instrumentation in the binary, physics PASS and clean Back teardown. BLAST layer **#1505** actual-present timestamps advanced from **17341774380831** to **17342843610779** ns. Earlier container [0,0] samples remain diagnostics, not presentation-progress evidence; this is not a long-duration frame-rate claim.

## Failure evidence and acceptance boundaries

The original failures remain unchanged in ignored evidence. `ordinary-initial-driver-review.txt` records the corrected background-only Back assumption: adding MAIN/LAUNCHER did not change the observed system decision to finish. `ordinary-final-system-back` and `handoff/ordinary` retain the already-active World-provider and stale Main-role failures. The final implementation uses upstream native ownership and checked Main release instead of retaining the provisional gate.

`session-final/gesture-crash-buffer.log` records the old GameActivity UI-thread SIGSEGV through onApplyWindowInsets/GameTextInput_processImeInsets. Its original gesture JSON incorrectly accepted foreground departure as success; `session-final/gesture-review-correction.md` preserves and corrects that result. The final gesture driver requires crash-free ordered teardown, not just a vanished foreground Activity or PID.

Back follows the actual system dispatcher decision; backgrounding, Activity finish, application Shutdown and process termination are distinct. HOME tests background pause independently. NON_BLOCKING lifecycle pumping applies while observing renderer CPU/GPU completion; after shutdown, normal WAIT(-1) may await and acknowledge late DESTROY without spinning.

`U/cleanup.json` passed: temporary device flag/UI XML removed, application stopped and not foreground, original timeout **60000** and stay-on **7** restored. The official production APK remains installed; diagnostic evidence remains ignored. Strict OpenSpec validation and whitespace checks passed. No commit or archive was performed.

Commit preparation rebuilt FrameworkTest, Aurora.RHITest and Aurora.AdaptorTest after the include/format cleanup. The follow-up runs passed 251 Framework, 17 Vulkan capability/shader-environment and 23 renderer-host/surface cases; these repeated checks are not added to the earlier unique total. Logs and the final source/file manifest are under ignored `cmake-build-debug/commit-prep-20261029/`.
