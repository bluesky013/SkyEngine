# Design: Editor preferences via reflected settings structs

## Context

AGENTS requires config/settings UI to go through the generic reflected form (`ReflectedFormView`
+ reflection registration + `CommandService`), not per-type controls, and names `PreferencesDialog`
as a legacy exception to migrate.

`ReflectedForm` is driven by compile-time reflection. `TypeMemberNode` holds raw
`GetterFn (*)(void*)` / `SetterFn (*)(void*, const void*)` function pointers; the getter/setter only
receives the object pointer (identical for every member), so a member cannot recover a **runtime
key**. The old preference model was exactly that: a runtime key/value schema
(`PreferenceRegistry` pages/sections/entries keyed by strings). A literal port was therefore
impossible without changing the reflection framework.

## Decision

Express settings as **reflected structs with metadata** and let a **generic form** render them:
the same model Unreal (`UDeveloperSettings` + `UPROPERTY(meta=...)`), Godot
(`EditorSettings` + `PROPERTY_HINT_*`) and Blender (RNA + UI metadata) use.

- One reflected struct per page: `GeneralSettings`, `EditorViewSettings`, `RenderingSettings`
  (`engine/sandbox/core/preferences/EditorSettings.h`). Plain, trivially-copyable data.
- Metadata (`LABEL`, `ORDER`, `RANGE_MIN/MAX/STEP`, enum names) is registered through the
  reflection attribute map (`CommonPropertyKey`), which `PropertyEditorRegistry::Resolve` already
  turns into an `EditorControl` (bool / integer / float / Enum / …). No UI lives in core.
- `PreferencePageRegistry` (cross-DLL `Singleton`) holds the ordered `{ id, title, object, TypeNode }`
  page list. **Dynamic is the page list; each page's fields are a fixed reflected type.**

This is the AGENTS-sanctioned "dynamic, schema-driven settings" case: schema-driven at the page
granularity, reflection-driven at the field granularity — so the controls come from reflection, not
from a hardcoded per-type `switch`.

## Dialog

`PreferencesDialog` keeps its category list + footer (OK / Cancel / Apply / Reset) and hosts one
`ReflectedFormView` bound to the selected page's object.

- Edits go through the form's `CommandService` path (undoable).
- OK / Apply: `onApplied()` (host persists), then re-snapshot. Cancel / Esc: restore the snapshot
  (raw byte copy — the structs are trivially copyable). Reset: `ReflectedForm::ResetToDefault` per
  top-level field (type default).
- Cancel restores by `memcpy` because live objects are edited in place; this keeps the form's undo
  commands pointing at stable storage (a dialog-local working copy would dangle them on close).

## Persistence

Settings are serialized by reflection JSON (`JsonOutputArchive` / `JsonInputArchive` +
`SaveValueObject` / `LoadValueById`) to the per-user `editor-preferences.json`, keyed per page id.
Unknown keys are ignored on load (forward-compatible); defaults come from the struct initializers.

## Rejected alternatives

- **Synthetic `TypeNode` from the runtime schema**: infeasible — getter/setter can't be bound to a
  runtime key (see Context).
- **Keep the key/value store**: keeps the hand-drawn dialog and the per-type switch (the violation).
- **Fixed single `EditorPreferences` struct**: would work, but makes plugin-contributed pages
  impossible; per-page structs keep the dynamic page list.
