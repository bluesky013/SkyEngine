# Editor Preferences & Settings

The Preferences dialog renders and edits editor settings through the **generic reflected form**
(`ReflectedFormView`), not per-type controls. Settings are reflected structs with UI metadata — the
same model Unreal (`UDeveloperSettings`), Godot (`EditorSettings` + `PROPERTY_HINT_*`) and Blender
(RNA) use. See the archived change
`openspec/changes/archive/2026-10-11-editor-preferences-reflected/`.

## Model

- `engine/sandbox/core/preferences/EditorSettings.h` — one plain (trivially-copyable) struct per
  page: `GeneralSettings`, `EditorViewSettings`, `RenderingSettings`, plus enums
  (`EditorLanguage`, `EditorTheme`).
- `RegisterEditorSettings(SerializationContext*)` — registers the types and their metadata
  (`LABEL`, `ORDER`, `RANGE_MIN/MAX/STEP`, enum names) through the core `CommonPropertyKey`
  attribute map, which `PropertyEditorRegistry::Resolve` turns into an `EditorControl`.
- `PreferencePageRegistry` (cross-DLL `Singleton`) — the ordered `{ id, title, object, TypeNode }`
  page list. Dynamic is the **page list**; each page's fields are a **fixed reflected type**.

Data and registration live in core (UI-free); the shell only binds.

## Why reflected structs (not a key/value schema)

`ReflectedFormView` is driven by compile-time reflection: `TypeMemberNode` holds raw
getter/setter function pointers that only receive the object pointer, so a member cannot resolve a
**runtime string key**. A runtime key/value schema (the previous `PreferenceRegistry`) therefore
could not be rendered by the reflection form; expressing settings as structs does. This is the
AGENTS "dynamic, schema-driven settings" case — schema-driven at the page level, reflection-driven
at the field level, so the controls come from reflection rather than a hand-written per-type switch.

## Dialog

`PreferencesDialog` keeps its category list and footer (OK / Cancel / Apply / Reset) and hosts a
single `ReflectedFormView` bound to the selected page's object.

- Edits are routed through the form's `CommandService` path (undoable).
- **OK / Apply** → `onApplied()` (host persists) + re-snapshot. **Cancel / Esc** → restore the
  snapshot taken on open. **Reset** → `ReflectedForm::ResetToDefault` per top-level field.
- Cancel restores by byte copy (structs are trivially copyable) because the form edits the live
  objects; a dialog-local working copy would dangle the form's undo commands on close.

## Persistence

`SandboxModule` serializes the structs by reflection JSON (`JsonOutputArchive` /
`JsonInputArchive` + `SaveValueObject` / `LoadValueById`) to the per-user
`editor-preferences.json`, keyed by page id. Unknown keys are ignored on load; defaults come from
the struct initializers.

## Adding a settings page

1. Add a struct in core (or a plugin) and register it (members + `LABEL` / `RANGE_*` / enum names).
2. `PreferencePageRegistry::Get()->Add({ id, title, &instance, GetTypeNode(TypeInfo<T>::RegisteredId()) })`.
3. The dialog renders it automatically — no per-page UI code.
