## Why

The sandbox editor has a reflection-backed `PropertyModel`/`PropertyDescriptor` (`EditorCore`) and runtime data
whose metadata is reflected (`ComponentAdaptor<Data>`, `World::RegisterConfiguration(name, Any)`), but no shared
way to turn reflected data into an editable UI. Today the shell `InspectorPanel` only prints member display names
(`engine/sandbox/shell/src/EditorShell.cpp:168-171`), and every data-driven panel (component inspector, world /
global config, future terrain/vegetation tool settings) would otherwise hand-write its own form. This change
builds one **reflection-driven auto-layout form framework** and ships the first two consumers on top of it, so
any reflected object gets an editable page for free.

## What Changes

- Add a toolkit-independent **reflected-form framework** in `EditorCore`: a `PropertyObject {void *data,
  const TypeNode *type}` unit, a recursive field/form model grouped into sections, and a `PropertyEditorRegistry`
  that maps a descriptor to a `PropertyEditorKind` (`Bool`, `Integer`, `Float`, `String`, `Enum`, `Color`,
  `Vector`, `Struct`, `Sequence`, `Unknown`) plus metadata, extensible by modules.
- Add a **property UI attribute layer** read from the existing reflection attribute map
  (`TypeMemberNode::properties`): visibility, label/tooltip/order/category, `readonly`, `multiline`, numeric
  **range → slider**, enum-as-flags, asset type, and an explicit editor-kind override. Attributes take precedence
  over type inference, so a `Color` struct becomes a color editor, a ranged float becomes a slider, and a
  `readonly` member renders read-only — declared once at reflection registration, honored by every panel.
- Add an **auto-layout view** in the shell (`sky::ui`): a control-factory seam (`kind` + metadata → control) and
  a `ReflectedFormView` that lays out sections (headers), label/control rows, indentation, expand/collapse and
  scrolling from the form model — no per-type layout code in consumers. Unknown kinds render read-only.
- Add **provider seams** so a panel supplies reflected objects without `EditorCore` depending on any world type:
  a selection-driven source and a named-config source, both host-injected.
- Route every accepted edit through `CommandService` so changes are undoable and the view refreshes on command
  and selection changes.
- Ship two consumers: a **component/selection inspector** and a **global config panel** (named world/project
  configs), both built purely on the framework.
- Add headless tests for the form model, kind registry, control resolution, and undoable edits.

## Capabilities

### New Capabilities

- `editor-reflected-form`: the generic reflection-driven form framework — reflected data unit, recursive
  section/field model, editor-kind registry, control-factory and object-provider seams, auto-layout form view,
  and undoable editing.
- `editor-inspector`: the selection-driven inspector consumer that renders the selected reflected data through
  the form framework.
- `editor-global-config`: the global config consumer that renders named reflected configuration objects through
  the form framework.

### Modified Capabilities

<!-- None: editor-property-model already provides descriptors, struct type exposure, sequence children, and
     undo-routed edits; this change composes them without changing their requirements. -->

## Impact

- `engine/sandbox/core` (EditorCore): new `property/` framework — `IEditorPropertySource`,
  `IEditorConfigSource`, `PropertyEditorRegistry`, `PropertyAttributes`, `ReflectedForm`/`PropertyField`;
  `EditorCore` stays `Core`+`Framework`-only (no world/render/UI-toolkit type).
- `engine/framework` `serialization/PropertyCommon.h`: extend `CommonPropertyKey` with the shared UI attribute
  keys (label/tooltip/order/category, readonly, multiline, range/step, editor hint/kind, enum flags, color
  space), alongside the existing `VISIBLE`/`LABEL_*`/`ASSET_TYPE`.
- `engine/sandbox/shell`: `IPropertyControlFactory` + default `sky::ui` controls and a `ReflectedFormView`
  auto-layout element; `EditorShell` gains `SetPropertySource`/`SetConfigSource`/`SetControlFactory` (replacing
  `SetInspectorModel`) and subscribes to `SelectionService` + `CommandService` changes.
- `engine/sandbox/module`: provides the world-backed global-config source (over `World` configurations
  `World::GetConfigByName`/`RegisterConfiguration`, `World.h:90-91`) and injects it; the generic
  registration-backed selection source lives in `EditorCore`.
- `engine/framework`: possibly a non-const world-config accessor so the config panel can write back in place
  (small, additive).
- `engine/sandbox/test`: extend `TestTypes` (bool/enum/string/struct/nested) and add form/registry/provider and
  undo tests.
- No new third-party dependencies; the legacy Qt editor (`engine/editor`) is not modified.
