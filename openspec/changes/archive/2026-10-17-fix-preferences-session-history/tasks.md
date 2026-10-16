## 1. Session and identity

- [x] 1.1 Add UI-free owned working-page session and independent history to EditorCore.
- [x] 1.2 Add stable registration generation and preserve heap working-object addresses across reorder.
- [x] 1.3 Apply reflected writable values safely; discard on Cancel/Close and clear history before storage release.
- [x] 1.4 Guard session reentrancy across notifications, including registry reallocation and replacement session history.

## 2. Dialog integration

- [x] 2.1 Inject local history into the existing reflected form and preserve cross-page Undo/Redo.
- [x] 2.2 Unbind before session replacement, destroy child forms before services, and defer transitions during edits.
- [x] 2.3 Complete internal Apply/OK state before external callbacks; protect reopen/destruction and event service lifetime.
- [x] 2.4 Document working-copy behavior, provider lifetime contract and deferred host/plugin persistence.

## 3. Validation

- [x] 3.1 Add persistent CPU regressions for deep values, baselines, global isolation, identity/reorder and reentrancy/lifetime.
- [x] 3.2 Build and run the affected portable editor CPU tests.
- [x] 3.3 Run those lifetime paths under ASan.
- [x] 3.4 Strictly validate this OpenSpec change and run diff whitespace checks.

Windows validation remains in the shared Windows backlog. No RDG/RHI/native GPU work, commit or archive is part of this task.

Integration verification (2026-10-16): targeted normal/ASan builds and the six portable suites
passed: EditorCoreTest 117, EditorWidgetsTest 28, EditorShellTest 86, UICoreTest 150, UITextTest 25,
UIEditorTest 3 (**409 per profile**). Shell includes 13 PreferencesSession tests and a real Shell
shortcut-routing regression: Preferences local Undo/Redo, key-up, Ctrl+S/F5 isolation and global
history after Cancel. ASan used `detect_leaks=0`; leak detection is unavailable on this host.
Both change validation and the full 18-change strict check passed. Tracked/untracked whitespace
checks passed. No RDG/RHI/renderer/Module/Core Event edits or full default build were made; the
255-file excluded-scope hash is unchanged. Windows/native module integration remains pending.
No commit/archive.
