## 1. Shared history boundary

- [x] 1.1 Add ordered removable before-history subscriptions without UI/asset dependencies.
- [x] 1.2 Prepare on Undo/Redo, reject recursive traversal, skip removed observers, and recheck stacks after callbacks.
- [x] 1.3 Subscribe forms to their injected service with weak lifetime protection and destruction cleanup.
- [x] 1.4 Route local key-down history through the service before inline editing, preserving normal text/Escape behavior.

- [x] 1.5 Fence commands by Clear generation so synchronous session replacement cannot reinsert an old command; guard local key refresh after observer-driven form destruction.

## 2. Regression and verification

- [x] 2.1 Add service ordering, empty history, reentrancy, observer mutation, and stack-clearing regressions.
- [x] 2.2 Add widget lifetime/empty-preview and Shell shortcut/menu/toolbar/direct-service final-value/cursor regressions.
- [x] 2.3 Run relevant editor tests and the actual asset history integration with T04.
- [x] 2.4 Run focused ASan tests, strict OpenSpec validation and whitespace checks, update task/document status, and record platform limits without commit/archive.

Integration verification (2026-10-16): EditorCoreTest 117, EditorWidgetsTest 28, and EditorShellTest
59 passed normally and with AddressSanitizer (`ASAN_OPTIONS=detect_leaks=0`). Actual Shell
shortcut/menu/toolbar/direct-service requests cover A→B plus preview C through Undo/Redo and
delayed events. T04's joint Viewer regression checks cancellation inside domain persistence
notification before Reload, persisted cook.json, Save state, and cursor. Modifier tests use
`kModCtrl`; ordinary Shift+Z remains text input and Ctrl+Shift+Z routes Redo. Full macOS build,
strict OpenSpec and whitespace checks passed. Win32/DXIL and real GPU checks remain T08.
No commit/archive was performed.
