## Context

`EditorCore` (`engine/sandbox/core`, links only `Core` + `Framework`) ships `CommandService`, `SelectionService`,
and a flat reflection-backed `PropertyModel`/`PropertyDescriptor` derived from runtime reflection
(`serialize::TypeMemberNode`, `TypeNode`, `enums`, `properties`). The shell renders panels with `sky::ui`;
`InspectorPanel` currently prints member display names only (`EditorShell.cpp:168-171`).

Runtime data is reflection-rich but split from its container: `ComponentAdaptor<Data>` keeps metadata on `Data`
(`framework/world/Component.h:45`), and `World` stores named configs as `Any`
(`RegisterConfiguration` / `GetConfigByName`, `World.h:90-91`). Reflection exposes getter/setter pairs plus a
`valueChanged` hook per member — there is no reflected member offset — and `Any` provides value-semantics boxing
(`Any::Create` copies). Every data-driven panel therefore needs the same thing: turn a reflected object into an
editable, auto-laid-out form. This design builds that once.

## Goals / Non-Goals

**Goals:**

- A generic reflection-driven form framework: reflected object unit, recursive section/field model, editor-kind
  registry, control-factory and provider seams, auto-layout view, undoable editing.
- First consumers: selection inspector and global (named) config panel, built only on the framework.
- Headless-testable model/registry/edits; auto-layout lives in the `sky::ui` view.

**Non-Goals:**

- Rich editors (asset pickers, color/vector pickers, drag gizmos) beyond the basic control set.
- Auto-discovery of components from a world; hosts publish objects through the provider seams.
- Associative-map members, docking/page chrome, and document persistence.
- The legacy Qt editor (`engine/editor`).

## Decisions

### D1. The reflected unit is data

Everything binds to `PropertyObject {void *object, const TypeNode *type}` pointing at reflected data.
**Why:** metadata lives on `Data` (`ComponentAdaptor<Data>`), and world configs are `Any` values. **Alternative:**
bind components/world types — rejected, their members are not where reflection lives.

### D2. Core form model over the existing flat `PropertyModel`

Keep `PropertyModel` unchanged; add a form layer:

```
PropertyField {                 // one reflected member (recursive)
    PropertyDescriptor descriptor;   // member descriptor; object == the real object for a root field
    PropertyEditorKind  kind;
    EditorControl       control;      // kind + attributes (readOnly, range, options, hint, assetType)
    PropertyField      *parent = nullptr;   // owning struct field, null for a root field
    std::vector<PropertyField> children;
    Any                 ownedValue;   // struct/sequence backing for children (a copy from the getter)
    uint32_t            elementIndex = 0; // for a sequence element field
    bool                isSequenceElement = false;
    bool                expanded = false;
}
FormSection { std::string title; std::vector<PropertyField> fields; bool expanded; }
ReflectedForm {
    std::vector<FormSection> sections;
    void Build(const std::vector<PropertyObject> &objects);
    void Rebuild();
    bool Edit(PropertyField &field, Any value, CommandService &commands);
}
```

Field order defaults to the reflected member order (`MemberMap` is a `std::map`, i.e. name-sorted); the `ORDER`
attribute overrides it.

- Fields are built like the earlier recursive tree: scalars are leaves; struct fields iterate
  `TypeNode::members` and construct child `PropertyDescriptor(ownedValue.Data(), &member, name, category)` (with
  `parent` set to the owning struct field); sequence fields own the container `Any` and reuse
  `PropertyDescriptor::BuildSequenceChildren`, recording each element's `elementIndex`. `PropertyDescriptor`
  gains an additive `GetElementIndex()` accessor so a sequence field can target add/remove.
- Sections group top-level fields by `PropertyDescriptor::GetCategory()` (today the type name; a member
  `properties` override can refine it later). Multiple `PropertyObject`s become multiple sections, so a panel can
  show a component's data and, say, actor-level data together.

**Why:** keeps `editor-property-model` stable and gives both consumers one reusable, layout-neutral description.
**Alternative:** make `PropertyModel` recursive/grouped — rejected, it changes an existing capability and bloats
the flat API.

### D3. `PropertyEditorRegistry`: kind + metadata from reflection, extensible

Resolve `kind`/`EditorControl` from `PropertyDescriptor`:

- `containerInfo != nullptr` (element type not `char`) → `Sequence`.
- `staticInfo->isEnum` (enum `TypeInfoRT` has `underlyingTypeId` and `TypeNode::enums`) → `Enum` (options from
  `TypeNode::enums`).
- registered id matching `TypeInfo<bool>` / `TypeInfo<std::string>` → `Bool` / `String`; other
  `staticInfo->isInteger` → `Integer`; `staticInfo->isFloatingPoint` → `Float`. These are `Core` fundamentals,
  not math/render types.
- type with reflected members → `Struct` (unless overridden).
- otherwise `Unknown` (read-only).

An extension seam (`RegisterKindResolver` / `RegisterTypeHandler`) lets modules map custom kinds
(`Color`, `Vector`) without `EditorCore` referencing those types. The shell owns the default domain mappings:
it registers `TypeInfo<Color>`/`TypeInfo<Vector2|3|4>` → `Color`/`Vector` **and** the matching control factories
at init, so the kind mapping and its control stay in one UI-owned place; third parties can register additional
resolvers through the same seam.

**Why:** views choose a control by `kind`, never by C++ type. **Alternative:** hardcoded type table in the core —
rejected, couples the core to concrete value types.

### D3.1 Property UI attributes drive control selection

Reflection already attaches per-member attributes (`TypeMemberNode::properties`, a `serialize::PropertyMap`
written at registration via `TypeFactory::Property(...)`), and `CommonPropertyKey` already carries UI hints
(`VISIBLE`, `LABEL_COLOR`, `ASSET_TYPE`). Extend that vocabulary and read it in the core into a
toolkit-independent `PropertyAttributes`:

```
enum class CommonPropertyKey : uint32_t {   // extended
    VISIBLE, LABEL_VISIBLE, LABEL_COLOR, ASSET_TYPE, REPLICATED,
    LABEL, TOOLTIP, ORDER, CATEGORY, READONLY, MULTILINE,
    RANGE_MIN, RANGE_MAX, RANGE_STEP, EDITOR_HINT, EDITOR_KIND, ENUM_FLAGS, COLOR_SPACE
};
struct PropertyAttributes {
    bool visible = true; bool readOnly = false; bool multiline = false; bool enumFlags = false;
    std::string label; std::string tooltip; int order = 0; std::string category;
    bool hasRange = false; double rangeMin = 0, rangeMax = 0, rangeStep = 0;
    std::string editorHint;              // "slider", "drag", ...
    std::optional<PropertyEditorKind> kindOverride;
    std::string assetType;               // from ASSET_TYPE
    std::string colorSpace;              // from COLOR_SPACE
};
```

Control resolution precedence: **explicit attribute** (`EDITOR_KIND`/`EDITOR_HINT`) → **registered type
handler** → **type inference** (D3). Effects:

- `VISIBLE=false` → field omitted from the form.
- `READONLY=true` → `EditorControl.readOnly`; the factory builds a non-editable control.
- `RANGE_*` on `Integer`/`Float` → `EditorControl` carries the range; the factory selects a **slider** (with a
  numeric readout) instead of a text box.
- `EDITOR_HINT` → variant selection (slider vs drag text vs spin).
- `Color`/`Vector` (reflected structs, `CoreReflection.cpp`) → single control via a registered type handler, not
  four scalar children.
- `ASSET_TYPE` → asset-reference control (a UUID member).

Attribute value encodings (defined so registration and parsing agree): booleans (`VISIBLE`, `LABEL_VISIBLE`,
`READONLY`, `MULTILINE`, `ENUM_FLAGS`) are `Any(bool)`; numeric range (`RANGE_MIN`/`RANGE_MAX`/`RANGE_STEP`) is
`Any(double)`; text (`LABEL`, `TOOLTIP`, `CATEGORY`, `EDITOR_HINT`, `ASSET_TYPE`, `COLOR_SPACE`) is
`Any(std::string_view)`; `ORDER` is `Any(int32_t)`; `EDITOR_KIND` is `Any(int32_t)` holding a built-in
`PropertyEditorKind` (custom kinds are expressed through a registered type handler, not this attribute). An enum
whose `TypeNode::enums` is empty degrades to a numeric control.

**Why:** attributes are the natural place to express UI intent, they are already registered with the type, and
they let one `Color`/`Vector` type get the right editor everywhere. **Alternative:** infer everything from the
C++ type — rejected, it cannot express range/readonly/asset-type or distinguish a `Vector4` position from a
color. **Alternative:** a separate editor-only registry keyed by member name — rejected, it duplicates type
information and drifts.

Decision: the UI attribute keys are added directly to `CommonPropertyKey` (framework), next to the existing
UI-only keys (`VISIBLE`, `LABEL_*`, `ASSET_TYPE`); `EditorCore` only *reads* them. This keeps one attribute
vocabulary and lets any plugin declare editor UI hints at registration with the existing `.Property(...)` API
and convenience macros. No separate editor-only key namespace is introduced.

### D4. Control factory seam lives in the UI layer

`EditorCore` exposes only `kind` + metadata. The shell defines `IPropertyControlFactory`:

```
struct PropertyControlContext { PropertyFieldView field; std::function<void(Any)> commit; };
IPropertyControlFactory::Create(const PropertyControlContext &) -> std::unique_ptr<sky::ui::UIElement>;
```

`PropertyFieldView` is a read-only projection (name, kind, `PropertyAttributes`, current value, enum options) so
the factory needs no `EditorCore` internals. Default factories cover `Bool` (toggle), `Integer`/`Float`
(text box, or **slider** when the range attributes are present), `String` (text, or multi-line when `MULTILINE`),
`Enum` (cycle/menu), `Struct` (group), `Sequence` (list with add/remove). All default controls honor `readOnly`.
Extensions register custom factories — for example a `Color` picker for `PropertyEditorKind::Color` and a
multi-component control for `Vector`. Unknown kinds fall back to a read-only label.

**Why:** keeps `EditorCore` UI-free while still letting third parties add controls.

### D5. Provider seams: selection and config, host-injected

```
class IEditorPropertySource { virtual std::vector<PropertyObject> Resolve(const SelectionItem &) const = 0; };
class IEditorConfigSource   { virtual std::vector<NamedPropertyObject> GetConfigs() const = 0; };
struct NamedPropertyObject { std::string name; PropertyObject object; };
```

Split by dependency: the default `RegisteredPropertySource` (a `Uuid` → `std::vector<PropertyObject>` map, no
world types) lives in `EditorCore` and is headless-testable; the world-backed `IEditorConfigSource` lives in the
sandbox module (built on `World`). The shell holds non-owning pointers and degrades to an empty state when a
source is unset. Multi-object selections use the first non-empty resolution (aggregation out of scope).

**Why:** `EditorCore` stays free of world/component types and follows the repo rule that interfaces live in the
consumer module. **Alternative:** `EditorCore` depends on `framework/world` — rejected by the layering rules and
the "consider the data" direction.

### D6. `ReflectedFormView`: the auto-layout

The shell builds a `sky::ui` tree from a `ReflectedForm`: collapsible section headers, a fixed label column and a
control column per row, indentation per depth, expand/collapse for struct fields, and a scroll container for
overflow. Consumers only supply a form + control factory; they contain no per-type layout code.

**Why:** this is the "auto-layout page" the change is about — layout is a single generic implementation shared by
every panel. **Alternative:** each panel lays out its own fields — rejected, duplicates work and drifts.

### D7. Undoable edits

- Scalar leaf edit → set the new value on the leaf's descriptor (mutating its owner's `ownedValue`), or, for a
  root scalar, `descriptor.MakeEditCommand(Any)`; enum members commit as an enum-typed `Any`.
- Struct/leaf edit → `ReflectedForm::Edit` mutates the edited field's value (which writes into its owner's
  `ownedValue`), then **propagates up the `parent` chain**: for each ancestor from the edited field's owner up to
  but **excluding** the root, `ancestor.descriptor.SetValue(ancestor.ownedValue)` writes the mutated child copy
  into its parent copy. Finally **one** `root.descriptor.MakeEditCommand(root.ownedValue)` is executed through
  `CommandService` (one undo step); the root is never `SetValue`-mutated directly, so the command captures the
  true previous value.
- Sequence add/remove → the existing `MakeAdd` / `MakeRemoveSequenceElementCommand` on the owning sequence field.

Why one command: `PropertyEditCommand` captures the old value from the getter on the object it is bound to
(`PropertyEditCommand.cpp:12-18`), so binding the command to the **root field's real object** with the fully
mutated root copy yields a correct single-step undo; intermediate copies must not be committed individually or
their commands would capture a temporary object.

The shell subscribes to `SelectionService::AddChangedCallback` and `CommandService::AddChangeCallback` (both
exist) so the inspector/config views refresh on selection changes and on undo/redo.

### D8. Consumers built purely on the framework

- **Inspector**: selection → `IEditorPropertySource` → `PropertyObject`(s) → `ReflectedForm` → `ReflectedFormView`.
- **Global config**: `IEditorConfigSource` → named `PropertyObject`s → **one collapsible section per named
  config** stacked in a single scrollable view → `ReflectedFormView`. Writing back needs a mutable config
  pointer, so `framework` gains a small
  non-const world-config accessor (`GetMutableConfigByName`) used only by the editor's config source.

## Risks / Trade-offs

- [Nested struct edits rewrite the whole struct per leaf] → accept for the foundation; revisit only if reflected
  offsets are added.
- [Form model groups by `category` = type name today] → the `CATEGORY` attribute overrides it per member;
  default stays the type name.
- [Attributes are untyped `Any` keyed by `uint32_t`] → central `CommonPropertyKey` enum + a typed
  `PropertyAttributes` parser that ignores missing/mistyped entries and applies defaults.
- [`Color`/`Vector` need handler registration to avoid 4 scalar children] → the shell registers their type
  handlers + control factories at init; if a handler is missing they fall back to struct recursion (safe, just
  less pretty). A `Uuid` member without `ASSET_TYPE` likewise falls back to its reflected `id` member.
- [World config write-back mutates framework] → the accessor is additive and used only by the editor config
  source; runtime path unchanged.
- [UI/toolkit coupling creeping into `EditorCore`] → the registry is plain enums and control factories live in
  the shell; the `editor-core` configure-time guard stays green and is **extended to also reject
  `framework/world/`** so "no world/component header in the core" is enforced, not just documented.
- [Full form rebuild on every command/selection change] → accepted for editor scale; if it becomes hot, gate on
  a dirty flag or rebuild only the affected section (no requirement change).
- [A form binds raw data pointers from a provider] → the provider owns the storage; a form is rebuilt on every
  selection/command change and dropped before a provider invalidates its storage; re-registering a world config
  must happen behind a command so the refresh picks up the new pointer.
- [Re-entrant refresh] → the shell refreshes via the command/selection callbacks after the stack updates, and
  rebuilds the whole form rather than mutating it in place, so no iterator is invalidated mid-dispatch.
- [Section grouping of multi-object selections] → default: one section per provided object, titled by its type.

## Migration Plan

Additive. Add the core form layer, add the shell control factories + `ReflectedFormView`, replace
`EditorShell::SetInspectorModel` with source/control setters, and point the sandbox module at the selection and
config sources. Existing `EditorCoreTest` cases keep passing.

## Open Questions

- Whether `NamedPropertyObject` should carry an explicit commit callback instead of relying on a mutable pointer;
  default is the mutable-pointer accessor for simplicity.

## As-built additions (implemented)

The foundation was implemented and extended beyond the original sketch. Current shape:

- **Theme/skin layer (shell).** `UiDraw` (primitives: rounded rect, gradient, shadow, slider,
  scrollbar, HSV/color helpers), `UiTheme` (`UiColors`/`UiMetrics`/`UiFonts`, `MakeDarkTheme`,
  `Get/SetDefaultUiTheme`) and `UiSkin` (component painter: panel/section/row/field/checkbox/slider/
  swatch/tab/tool/popup). Any panel styles itself from one `UiSkin`; supply a custom `UiTheme` to restyle.
- **Reusable view (shell).** `ReflectedFormView : ui::UIElement` renders and edits a `ReflectedForm`
  (scalars, bool, enum dropdown, sliders, strings, color, vector, asset, nested struct, sequence) with
  attribute-driven controls, per-kind validation, reset-to-default, scroll, hover/selection. Subclass
  hooks (`OnViewTick`, `ExtraHeaderWidth`, `PaintExtraHeader`, `HandleExtraHeaderPointer`) let panels add
  chrome (the demo adds a "Live" toggle). Consumers no longer hand-write layout.
- **Color picker (shell).** `ColorPicker` is a standalone component: a CPU-generated hue/saturation disc
  rendered as a texture (continuous colors — no per-pixel rects), a value bar, an alpha bar and
  H/S/V + R/G/B/A readouts, decoupled via live/commit callbacks.
- **Value validation (core).** `PropertyValidation`: input filtering by kind, `ParseValueText` (parse per
  reflected type, integer-only, range clamp, step quantize) and `ConstrainValue`.
- **Reset to default (core).** `ReflectedForm` snapshots each field's default on first bind;
  `IsModified` / `ResetToDefault` (one undoable edit).
- **External change notification (core).** `PropertyChangeNotifier` + `EditorCore::GetPropertyChanges()`;
  views poll the revision and rebuild (deferred while editing/dragging).
- **Asset references (core + shell).** `PropertyEditorKind::Asset` resolved from the `ASSET_TYPE`
  attribute; `IEditorAssetCatalog`/`RegisteredPropertySource`-style seam with a default `AssetDataBase`
  implementation; the view shows the asset name, validates its type, and offers a picker popup.
- **Reflected-unit clear.** Members reflect via `PropertyDescriptor`'s getter/setter through
  `PropertyEditCommand` (undoable); nested structs write through copied parents plus a single root command.
- **Provider seams (core).** `editor/core/property/EditorPropertySource.h`: `IEditorPropertySource`,
  `RegisteredPropertySource`, `NamedPropertyObject`, `IEditorConfigSource`.

## As-built additions: widget architecture

The leaf controls were extracted into a `ReflectedWidget` system (shell):

- `ReflectedWidget` base (Paint / OnDown/OnMove/OnUp / popup + escape / RowCount) with a
  `ReflectedWidgetHost` service interface and a `ReflectedWidgetRegistry` (kind -> widget).
- `GenericReflectedWidget` (Bool / Integer / Float / String / Vector), `AssetReflectedWidget`,
  `ColorReflectedWidget`, `RotationReflectedWidget`. `ColorPicker` is a standalone component.
- `ReflectedFormView` implements the host and dispatches by kind; it keeps only form layout,
  scroll, default-reset and container (struct/sequence) behaviour. `Enum` still uses the
  view's inline popup. `BeginTextEdit` gives widgets host-managed inline text editing.
- Editor-side member appearance (`PropertyEditorRegistry::RegisterMemberAppearance`) relabels
  and reorders members (Transform -> Position / Rotation / Scale).
- `FormatPropertyValue` moved to core (`PropertyValidation`).
- Engine math types map to kinds by registration: Vector2/3/4 -> Vector, Quaternion -> Rotation.
- Axis order is fixed to YZX (runtime convention).

See `docs/editor/reflection-widget-framework.md`.
