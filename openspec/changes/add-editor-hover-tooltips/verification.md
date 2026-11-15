# Verification - 2026-11-15

## Host and build

macOS x86_64, existing `cmake-build-debug` Xcode cache and third-party configuration.

Final build passed for `SandboxModule`, `SandboxEditor`, `FrameworkTest`, `UICoreTest`,
`UITextTest`, `EditorCoreTest`, `EditorWidgetsTest`, `EditorShellTest` and `Animation.EditorTest`.
`SandboxEditor` also built its runtime manifest module dependencies. No new production target was added.

Evidence: `cmake-build-debug/editor-tooltip-final-build.log`.

The initial sandboxed build could not write Xcode's DerivedData cache. The authorized build succeeded.
The first test run exposed a stale CocoaWindow object from the native-input edit during compilation;
the final incremental build recompiled it and the full affected test set was rerun successfully.

## Automated tests

| Suite | Passed | Failed | Skipped |
| --- | ---: | ---: | ---: |
| FrameworkTest | 313 | 0 | 0 |
| UICoreTest | 194 | 0 | 0 |
| UITextTest | 26 | 0 | 0 |
| EditorCoreTest | 209 | 0 | 0 |
| EditorShellTest | 163 | 0 | 0 |
| EditorWidgetsTest | 59 | 0 | 0 |
| Animation.EditorTest | 9 | 0 | 0 |
| Total | 973 | 0 | 0 |

This change adds 20 tests: nine host tests, six provider/catalog tests, four shell integration
tests and one Cocoa native-entry test. Existing retained-view teardown coverage also exercises mouse leave.

Coverage includes stationary delay, changed targets, binding and attachment expiry, content-build expiry,
popup priority, transforms and clips, long-path wrapping and edge placement, pointer/button cancellation,
metadata propagation, missing/type-mismatched assets, current catalog lookup, rebind, toolbar replacement,
modal blocking, surface isolation, menu blocking and native leave window identity.

Evidence:

- `cmake-build-debug/editor-tooltip-ctest.log` and `editor-tooltip-ctest.xml`: seven CTest suites passed.
- `cmake-build-debug/editor-tooltip-validation/tests.log`: full final test output.
- `cmake-build-debug/editor-tooltip-validation/tests.json`: per-suite counts.
- `cmake-build-debug/editor-tooltip-source-ownership.log`: 25 passed; one Windows path-identity test skipped on macOS.
- `git diff --check`, changed-source ASCII/forbidden-token checks, scoped clang-format and strict OpenSpec validation passed.

## Editor visual verification

The final executable was copied into the existing local validation app bundle. Computer Use verified:

- Editor launch and project-manager interaction.
- Opening the Test project and framegraphlighting.world.
- Scene rendering, Cube selection and the reflected Inspector.
- Builtin material/mesh path display retained from the preceding asset-path change.

Screenshot: `cmake-build-debug/editor-tooltip-validation/editor-ready.png`.

Actual hover popup screenshots remain pending: this session's native Computer Use API exposes click,
drag and scroll, but no pointer-move/hover operation. A user hover was requested; no synthetic screenshot
is presented as editor evidence. The editor remains open on Cube's asset fields for that check.

After the user's hover confirmation, two further captures still showed the Inspector without a
tooltip. A timed capture was also taken after requesting that the editor stay in the foreground;
it did not show a tooltip either. These captures do not establish whether the intended hover/focus
state reached the editor. The user subsequently confirmed that the tooltip appeared in the live
editor. This confirms basic hover display by user observation; a captured tooltip image and the
remaining visual cases are still pending.

Detailed tooltip visual inspection, focused-window leave behavior and floating-window
visual cases are not marked as completed. Their underlying timing/lifetime/placement behavior is covered
by automated tests. Windows and Linux native branches were reviewed but not built or executed on this host.

No commit or OpenSpec archive was performed.

## Review remediation verification (2026-11-15)

UI hit testing now blocks lower siblings without tooltip content; only the hit branch and its ancestors are queried. Paint revalidates the retained candidate instead of rediscovering it. Asset label lookup uses one catalog item while deferred tooltip construction remains lazy.

Final results: EditorWidgetsTest 62/62, EditorShellTest 168/168, UICoreTest 194/194, EditorRenderTest 48/48. The logged-in desktop rerun passed all 23 CocoaPlatformTest cases, including mouse-leave and retained-view teardown. All processes exited 0. Logs are `cmake-build-debug/review-cleanup-<target>.log`; the native log is `review-cleanup-CocoaPlatformTest.log`.

The editor was rebuilt and checked for project settings, creation, asset paths, rendering, undo and save. Key screenshots and the complete 1175-test result are recorded in `../project-object-creation-defaults/verification.md`. The remaining hover/floating/leave visual limitations above still apply; tasks 4.5 and 4.7 remain open for those manual cases.
