# Tasks

- [x] 1. Add reflected settings structs (`GeneralSettings`, `EditorViewSettings`, `RenderingSettings`)
      + `RegisterEditorSettings` (reflection + labels/ranges/enums) in `engine/sandbox/core`.
- [x] 2. Add `PreferencePageRegistry` (cross-DLL `Singleton`) of `{ id, title, object, TypeNode }`.
- [x] 3. Remove the key/value `PreferenceTypes` / `PreferenceRegistry` / `PreferenceStore`.
- [x] 4. Rewrite `PreferencesDialog` to host one `ReflectedFormView` per page (category list + footer
      kept); OK/Apply persist, Cancel restores the open snapshot, Reset restores type defaults.
- [x] 5. `EditorShell::SetPreferences(PreferencePageRegistry*, onApplied)`; dialog wired in `Rebuild`.
- [x] 6. `SandboxModule`: register reflection + pages; read `editor.uiScale` from `EditorViewSettings`;
      persist/load the structs via reflection JSON (`editor-preferences.json`).
- [x] 7. Tests: drop `PreferenceStoreTest`; add `EditorSettingsTest` (form builds from reflection;
      page registry); update `EditorShellTest.PreferencesHostsDialog`.
- [x] 8. Docs: `docs/editor/preferences-and-settings.md`; update `editor-framework-design.md` and
      `ui-standards.md`.
