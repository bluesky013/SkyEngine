## 1. Unified state and safe dispatch

- [x] 1.1 Append compatible visible/checkable/checked metadata and backend-independent state/snapshot types.
- [x] 1.2 Add registration generation/revision, copied state resolution and centralized guarded invocation.
- [x] 1.3 Route builder callbacks and accelerators through exact registration state; retain weak owner checks.

## 2. Chrome refresh and presentation

- [x] 2.1 Forward state to toolbar/menu and exclude hidden entries from paint, geometry and hit testing.
- [x] 2.2 Show disabled and checked states through theme/UiSkin painters.
- [x] 2.3 Refresh main Shell and Viewer state in place; preserve open menu across state refresh and structural entry replacement.
- [x] 2.4 Guard recursive refresh and callback-driven entry replacement/destruction.

## 3. Verification

- [x] 3.1 Add persistent CPU state/dispatch/draw/lifetime regressions.
- [x] 3.2 Run affected Core/Widgets/Shell tests normally and under ASan; record actual results.
- [x] 3.3 Run strict OpenSpec and whitespace checks. Do not commit/archive.

Windows remains a recorded follow-up. No RDG/RHI, renderer, GPU or whole-repository build is part of this change.

Implementation preparation (2026-10-16, macOS): 13 persistent EditorActionStateTest regressions are ready for the parent-controlled EditorShellTest build. Strict OpenSpec validation and whitespace checks passed. This subtask did not start a shared build; normal/ASan execution remains unchecked until integration results arrive.

Integration verification (2026-10-16): six portable CPU suites passed normally and under ASan
(`detect_leaks=0`): Core 117, Widgets 28, Shell 86, UI 150, Text 25, UI.Editor 3, **409 per profile**.
Shell contains all 13 new action-state cases and the Preferences/global shortcut integration.
The initial menu assertion incorrectly expected a gradient endpoint color; it now verifies the
checked glyph in its actual checkbox rectangle across unchecked/checked/unchecked and checks
disabled icon tint. The full suite then passed normally and under ASan.
Strict validation of all 18 related changes and tracked/untracked whitespace checks passed.
The 255-file excluded-scope hash is unchanged. No renderer targets, full default build, Windows
checks, commit or archive were performed. Scope-instance/DLL unload ownership remains T10-B.
