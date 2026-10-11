# Change: Reflect editor preferences through the generic property form

## Why

The Preferences dialog (`PreferencesDialog`) hand-draws a per-`PreferenceType` control for every row
(`switch` in `OnPaint`), which violates the editor rule that config/settings UI must go through the
generic reflected form (`ReflectedFormView`) and must not hardcode per-type controls. AGENTS flags
`PreferencesDialog` as a legacy exception to be migrated.

`ReflectedFormView` is driven by compile-time reflection (`TypeNode` / `TypeMemberNode` with raw
getter/setter function pointers). Preferences were a runtime key/value schema (`PreferenceRegistry`
with stable string keys), which that form cannot express without a framework change. This change
adopts the industry pattern (Unreal `UDeveloperSettings` / Blender RNA): **settings are reflected
structs with UI metadata; the UI is generated**. The dynamic part is the *page list* (registered
types), not the *fields*.

## What changes

- Replace the key/value preference model (`PreferenceTypes` / `PreferenceRegistry` /
  `PreferenceStore`) with reflected settings structs (`GeneralSettings`, `EditorViewSettings`,
  `RenderingSettings`) in `engine/sandbox/core` + a `PreferencePageRegistry` (cross-DLL) of
  `{ id, title, object, TypeNode }` pages.
- Register the settings types + metadata (LABEL / ORDER / RANGE_MIN/MAX/STEP / enums) via the
  reflection context.
- Rewrite `PreferencesDialog` to host one `ReflectedFormView` for the selected page; edits are
  undoable (CommandService); OK/Apply persist, Cancel restores a snapshot, Reset restores type
  defaults.
- Persist the settings structs by reflection (JSON), per user.
- `EditorShell::SetPreferences` now takes the `PreferencePageRegistry*`.

## Impact

- Affected specs: `editor-preferences` (typed key/value model → reflection-driven settings).
- Affected code: `engine/sandbox/core/preferences/*`, `engine/sandbox/shell/PreferencesDialog.*`,
  `engine/sandbox/shell/EditorShell.*`, `engine/sandbox/module/SandboxModule.*`,
  `engine/sandbox/test/*` (drop `PreferenceStoreTest`, add `EditorSettingsTest`).
- No UI-toolkit or render types leak into core; data/registration stay in core, the shell only binds.
