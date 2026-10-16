## 1. Binding lifecycle

- [x] 1.1 Add cancel/abandon reset intent to the per-form widget seam and implement non-writing ColorPicker abandonment.
- [x] 1.2 Make Bind/Unbind/destruction abandon invalid bindings; expose explicit live-model CancelInteractions and preserve Refresh/Escape rollback.
- [x] 1.3 Expire per-binding weak callback tokens independently of notifier subscriptions and guard built-in color/inline callbacks.

## 2. Host integration

- [x] 2.1 Abandon WorldConfig bindings before resolving replacement config and cancel before valid-model subsystem selection/save.
- [x] 2.2 Cancel/detach AssetViewer forms before replacing settings; cancel close/reopen and destroy child forms before owned models.
- [x] 2.3 Migrate valid-model Preferences page-switch/close/snapshot restore and document Bind/Refresh compatibility requirements.

## 3. Regression and validation

- [x] 3.1 Add widget regressions for live cancellation, Escape, non-writing abandonment, released model Unbind/Bind/destruction, and token/subscription cleanup.
- [x] 3.2 Add real WorldConfig same-address reload and heap-backed AssetViewer reload/close/destruction regressions.
- [x] 3.3 Build affected consumers and run EditorWidgetsTest/EditorShellTest plus relevant existing regressions.
- [x] 3.4 Run the released-model/host lifecycle regressions under ASan and record the actual result.
- [x] 3.5 Run strict OpenSpec/whitespace checks and review the reset-mode/callback ownership contract. Do not commit/archive.

## Verification preparation (2026-10-16, macOS)

- Strict OpenSpec validation and `git diff --check` passed. Changed C++ files were formatted with the Xcode clang-format tool.
- Added three widget regressions plus the explicit-cancel migration of the previous rebind/destruction test, and two real host lifecycle regressions. Build/test/ASan execution is coordinated by the parent task; no build was launched by this implementation subtask.
- Bind/Refresh callers were reviewed: invalidation-driven inspector/preset/document notifications keep safe abandonment; live AssetViewer, WorldConfig selection/toggle, and Preferences mutation paths explicitly cancel before release or overwrite.
- At initial T03 preparation, stable asset history/persisted undo and shared history preparation were deferred to T04/T05. Their implementations now live in [`fix-asset-settings-history`](../fix-asset-settings-history/design.md) and [`fix-editor-history-preview-order`](../fix-editor-history-preview-order/design.md), including synchronous-observer/session-generation handling. This remains a historical T03 scope record; completion evidence belongs to each change's actual verification record.

Integration verification (2026-10-16): full macOS build and final affected-consumer rebuild passed.
EditorWidgetsTest 28 and EditorShellTest 59 passed normally and under AddressSanitizer
(`ASAN_OPTIONS=detect_leaks=0`), including released-model abandonment, live cancellation,
same-address config reload, viewer reload/close/destruction, and actual history dispatch.
The totals include related changes' tests; they are not all unique T03 lifecycle scenarios.
Strict OpenSpec/whitespace checks passed. Win32 and real native GPU validation remain T08;
no commit/archive was performed.
