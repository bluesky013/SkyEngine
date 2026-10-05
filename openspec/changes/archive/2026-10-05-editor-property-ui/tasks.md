## 1. Core form model, attributes, and editor-kind registry (`engine/sandbox/core`, `engine/framework`)

- [ ] 1.1 Add `PropertyEditorKind` (`Bool`, `Integer`, `Float`, `String`, `Enum`, `Color`, `Vector`, `Struct`,
      `Sequence`, `Unknown`) and `EditorControl { kind, componentCount, readOnly, options, hasRange, rangeMin,
      rangeMax, rangeStep, editorHint, assetType }`.
- [ ] 1.2 Extend `CommonPropertyKey` (`framework/serialization/PropertyCommon.h`) with UI attribute keys
      (`LABEL`, `TOOLTIP`, `ORDER`, `CATEGORY`, `READONLY`, `MULTILINE`, `RANGE_MIN`, `RANGE_MAX`, `RANGE_STEP`,
      `EDITOR_HINT`, `EDITOR_KIND`, `ENUM_FLAGS`, `COLOR_SPACE`) **appended after the existing values** (so
      `VISIBLE`/`LABEL_*`/`ASSET_TYPE`/`REPLICATED` keep their numbers), plus convenience registration macros.
- [ ] 1.3 Add `PropertyAttributes` (`editor/core/property/PropertyAttributes.*`) parsing `member->properties`
      (`serialize::PropertyMap`) into typed defaults using the defined encodings (bool / double / `string_view` /
      `int32_t`), ignoring missing/mistyped entries.
- [ ] 1.4 Add `PropertyEditorRegistry` resolving `EditorControl` with precedence **explicit attribute
      (`EDITOR_KIND`/`EDITOR_HINT`) → registered type handler → type inference** (container/enum/bool/string/
      integer/float/struct/unknown); expose enum options from `TypeNode::enums`; extension seam
      (`RegisterKindResolver` / `RegisterTypeHandler`).
- [ ] 1.5 Add `PropertyField` (with `parent`, `ownedValue`, `elementIndex`, `isSequenceElement`) + `FormSection`
      + `ReflectedForm` (`editor/core/property/ReflectedForm.*`) building a layout-neutral form from one or more
      `PropertyObject`s, grouped into sections by category, recursively covering scalars, structs
      (`TypeNode::members`), and sequences; support rebuild. Add an additive
      `PropertyDescriptor::GetElementIndex()` so sequence fields can target add/remove.
- [ ] 1.6 Implement `ReflectedForm::Edit`: scalar/enum via `MakeEditCommand` (enum-typed `Any`), struct via
      working-copy write-through through the parent member, sequence add/remove via the existing commands; honor
      `readonly`.
- [ ] 1.7 Expose the registry through `EditorCore` (`GetPropertyEditors()` beside `GetCommandService()`).

## 2. Provider seams (`engine/sandbox/core`)

- [ ] 2.1 Add `IEditorPropertySource` (`Resolve(const SelectionItem &) -> std::vector<PropertyObject>`).
- [ ] 2.2 Add `NamedPropertyObject` + `IEditorConfigSource` (`GetConfigs() -> std::vector<NamedPropertyObject>`).

## 3. Shell control factories and auto-layout (`engine/sandbox/shell`)

- [ ] 3.1 Add `IPropertyControlFactory` + `PropertyControlContext`/`PropertyFieldView` (read-only projection:
      name, kind, metadata, value, enum options, `commit` callback).
- [ ] 3.2 Add default `sky::ui` controls: bool toggle; integer/float text commit, or **slider** when a range
      attribute is present; string text or multi-line (`MULTILINE`); enum cycling; asset-reference control;
      `Unknown` read-only label. All honors `readOnly`.
- [ ] 3.2b In the shell, register the default domain type handlers + control factories for `Color`
      (`PropertyEditorKind::Color`) and `Vector` (`Vector2/3/4`) so they render as single controls instead of
      nested fields (kind mapping and control in one UI-owned place).
- [ ] 3.3 Add `ReflectedFormView`: build a `sky::ui` tree from a `ReflectedForm` with collapsible section
      headers, label + control columns, per-depth indentation, struct expand/collapse, and scrolling.
- [ ] 3.4 `EditorShell`: add `SetPropertySource`/`SetConfigSource`/`SetControlFactory` (replacing
      `SetInspectorModel`) and subscribe to `SelectionService` + `CommandService` change callbacks.

## 4. Consumers (`engine/sandbox/shell`)

- [ ] 4.1 Reflection-driven inspector panel: selection source → `ReflectedForm` → `ReflectedFormView`, with an
      empty state when unresolved.
- [ ] 4.2 Global config panel: config source → one titled collapsible section per named config → view, with an
      empty state when none.

## 5. Module wiring and framework accessor (`engine/sandbox/module`, `engine/framework`)

- [ ] 5.1 Add an additive `World::GetMutableConfigByName` (or equivalent non-const accessor) for config
      write-back; runtime behavior unchanged.
- [ ] 5.2 Implement the registration-backed `RegisteredPropertySource` in `EditorCore` (a `Uuid` →
      `std::vector<PropertyObject>` map with an `Add`/`Remove` API) so extensions publish inspectable data and it
      is usable in headless tests.
- [ ] 5.3 Implement `IEditorConfigSource` over `World` named configurations in the module and inject the config
      source (plus the shell's default control factory) into the shell in `SandboxModule::Init`.
- [ ] 5.4 Register a config panel id + title and add it to the default layout.

## 6. Tests (`engine/sandbox/test`)

- [ ] 6.1 Extend `TestTypes` with bool, enum, string, nested struct, and sequence members.
- [ ] 6.2 Add `ReflectedFormTest`: section grouping, struct child fields, sequence child fields, rebuild.
- [ ] 6.3 Add `PropertyEditorRegistryTest`: kind resolution, enum options, unknown read-only, custom resolver,
      and attribute overrides (readonly, range → slider, hidden/visible, label/category, color kind override).
- [ ] 6.4 Add provider tests: selection source resolve + empty source; config source list + empty.
- [ ] 6.5 Add edit/undo tests: scalar, struct write-through, sequence add/remove.
- [ ] 6.6 Add a headless config write-back test (edit a `NamedPropertyObject`, read it back through the source).
- [ ] 6.7 Register every new test source in `engine/sandbox/test/CMakeLists.txt` (the target lists sources
      explicitly, not by glob).

## 7. Build and verification

- [ ] 7.1 Build `EditorCoreTest` (`SKY_BUILD_TEST`) headless and run all form/registry/provider/edit tests.
- [ ] 7.2 Build `SandboxEditor` (`SKY_BUILD_SANDBOX`) and confirm the inspector and config panels auto-lay-out,
      edit, and undo.
- [ ] 7.3 Extend the `editor-core` configure-time guard (`engine/sandbox/core/CMakeLists.txt`) to also reject
      `framework/world/` includes, then verify it passes (no `aurora/`, `render/`, `ui/`, Qt, or world header).
