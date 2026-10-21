## Implementation

- [x] 1. Connect the editor's sole packet producer and active World after simulation tick.
- [x] 2. Add create/delete actions and owned undo lifetimes.
- [x] 3. Bind World actors/components through the shared outliner and reflected inspector.
- [x] 4. Track viewport extent and camera changes with correct output revisions.
- [x] 5. Validate create/edit/undo/delete/save/reopen with automated and native UI checks.
- [x] 6. Build affected targets and validate specs, formatting and regression tests.

## Actor-only Outliner correction

- [x] 7. Remove component outline IDs and display the real Actor hierarchy.
- [x] 8. Bind all inspectable components through grouped reflected form sections.
- [x] 9. Verify grouped edits/defaults/undo, selection invalidation, native Inspector layout and affected regressions.

2026-10-21 Outliner correction evidence: EditorCoreTest 166/166, EditorWidgetsTest 34/34, EditorShellTest 129/129 and SandboxProjectTest 11/11 passed. Native UI shows Actor-only rows and grouped Transform/StaticMesh or Transform/Environment sections. Toggling StaticMesh castShadow updates rendering, undo restores it, and switching Actors replaces all component fields. Grouped defaults, nested field lifetimes, hierarchy ordering and selection invalidation are covered by regression tests.

2026-10-21 final evidence: targeted SandboxModule build refreshes configured runtime DLL dependencies through the existing dependency helpers. World open/save/reopen, Environment edit/undo, shadow toggle and corrected scene orientation passed native UI verification. EditorShellTest 129/129, SandboxProjectTest 10/10, GPU submission 892 passed / 3 backend-specific skips and RHITest 133/133 passed. Changed C++ formatting, git diff --check and strict OpenSpec validation passed. Static-analysis warnings remain tracked under aurora-scene-bridge 5.3; Metal/mobile native execution is unverified. No archive or commit was performed.
