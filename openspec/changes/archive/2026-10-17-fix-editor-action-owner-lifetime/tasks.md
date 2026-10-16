## 1. Host callback lifetime

- [x] 1.1 Add a shell-owned action lifetime signal and expire it before child/member teardown.
- [x] 1.2 Guard all asset-viewer enabled/invoke callbacks with weak owner validity, including copied action records.
- [x] 1.3 Preserve newer owners' registrations and avoid process-wide registry/same-name scope cleanup.
- [x] 1.4 Clear the current viewer target before live-shell tree reconstruction so retained predicates cannot read a just-destroyed viewer.

## 2. Regression and verification

- [x] 2.1 Cover retained callbacks and independent viewer chrome construction after shell destruction without clearing registry state in the test.
- [x] 2.2 Cover older-shell destruction after a newer shell replaces the same action IDs, and verify the newer Close action remains functional.
- [x] 2.3 Build affected consumers and run the complete EditorShellTest suite under ASan; record actual results.
- [x] 2.4 Run strict OpenSpec/whitespace checks. Do not commit/archive.

## Verification preparation (2026-10-16, macOS)

- The parent full-suite ASan run exposed the stale owner through ActionBarBuilder::BuildToolBarItems → AssetViewerWidget::CanUndo after a previous shell's destruction.
- EditorShell action guards and two persistent EditorActionOwnerLifetimeTest regressions are implemented and clang-formatted. The tests deliberately retain process registry state and action copies across destruction; they do not clear globals to hide the lifetime failure.
- Strict OpenSpec validation and `git diff --check` passed. Rebuild/full-suite ASan execution is coordinated by the parent task; this subtask launched no shared build.

Integration verification (2026-10-16): final EditorShell/SandboxModule/SandboxEditor consumers
rebuilt successfully. The complete EditorShellTest suite passed all 59 cases normally and with
AddressSanitizer (`ASAN_OPTIONS=detect_leaks=0`), including retained action copies, independent
Viewer chrome after owner destruction, newer same-id owners, and live Shell rebuild.
The prior full-suite ASan stale-owner failure no longer occurs. Strict OpenSpec and whitespace
checks passed; no commit/archive was performed.
