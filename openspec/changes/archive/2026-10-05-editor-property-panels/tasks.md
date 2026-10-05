## 1. Enum widget

- [x] 1.1 Add `EnumReflectedWidget` (shell) with the enum dropdown (open/select/hover/close/escape),
      registered for `PropertyEditorKind::Enum`; the view delegates enum fields to it.
- [x] 1.2 Remove the now-dead inline enum popup/case from `ReflectedFormView` (generic enum stub in
      `GenericReflectedWidget` left harmless/unused).

## 2. Inspector consumer

- [x] 2.1 Make `InspectorPanel` a `ReflectedFormView` subclass (or host one) that binds the selection.
- [x] 2.2 Resolve the selection through an `IEditorPropertySource` (fallback: `RegisteredPropertySource`);
      rebuild on selection change; empty state when unresolved.

## 3. Global config consumer

- [x] 3.1 Add a global-config panel (new panel id) rendering each `NamedPropertyObject` from
      `IEditorConfigSource` as a collapsible section through `ReflectedFormView`.
- [~] 3.2 Added `World::GetMutableConfigByName`; `IEditorConfigSource` impl over World is blocked (the
      editor host owns no `World` instance yet). Panel degrades to empty.

## 4. Module wiring

- [x] 4.1 Inject the default property source and the config source in `SandboxModule::Init`.
- [x] 4.2 Register the config panel id + default layout (next to the inspector).

## 5. Build and verification

- [x] 5.1 Build `SandboxEditor`, confirm the inspector shows selected data and the config panel renders
      named configs; edits undoable.
- [x] 5.2 Confirm `editor-core` forbidden-dependency guard still passes (no world/render/ui include).
