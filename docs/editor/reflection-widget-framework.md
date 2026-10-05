# SkyEngine Editor: Reflection Widget Framework

The sandbox editor renders and edits reflected data without knowing concrete C++
domain types. This document describes the framework's layers, the widget model,
and how to extend it.

## Layers

```
engine/sandbox/core            (no UI / no world / no math types; Core + Framework only)
  PropertyDescriptor           read/write one reflected member via its getter/setter
  ReflectedForm / PropertyField recursive form model (struct/sequence), undoable Edit, defaults
  PropertyEditor               PropertyEditorKind / EditorControl / PropertyEditorRegistry
  PropertyValidation           type parsing, input filtering, range/step constraints, formatting
  PropertyChangeNotifier       external-change revision + callbacks
  EditorAssetCatalog           asset lookup seam (default: framework AssetDataBase)
  EditorPropertySource         IEditorPropertySource / RegisteredPropertySource / IEditorConfigSource

engine/sandbox/shell           (sky::ui; styling + interaction)
  UiDraw / UiTheme / UiSkin    primitives (rounded/gradient/shadow/slider/color) + theme + component skin
  ReflectedWidget              per-kind widget base
  ReflectedWidgetHost          services a widget borrows from its host (the form view)
  ReflectedWidgetRegistry      kind -> widget (singletons)
  GenericReflectedWidget       Bool / Integer / Float / String / Vector (+ Enum popup)
  AssetReflectedWidget         asset references (type-validated picker)
  ColorReflectedWidget         colour (reflection channels + ColorPicker)
  RotationReflectedWidget      quaternion shown/edited as Euler (YZX, degrees) or raw
  ColorPicker                  standalone Blender-style colour popup (no engine colour type)
  ReflectedFormView            reusable form view; implements ReflectedWidgetHost; dispatches by kind
  ReflectionDemoPanel          thin demo consumer
```

## Data flow

- **Read**: `PropertyDescriptor::GetValue()` -> reflected getter.
- **Write**: `ReflectedForm::Edit(field, value, CommandService)` -> `PropertyEditCommand`
  (setter; undo restores the previous value). Nothing writes member memory directly.
- **Structure**: `TypeNode::members / enums / containerInfo` drive struct, sequence,
  and enum handling. Vector components are read/written by `isFloatingPoint` members.
- **Kinds**: resolved by attribute (`EDITOR_KIND`), compatibility attribute (`ASSET_TYPE`),
  registered type handler (`RegisterType`), or inference (`registeredId` for scalars).
- **External change**: a data owner calls `EditorCore::GetPropertyChanges().Notify()`;
  the view polls the revision and rebuilds (deferred while editing).
- **Reset to default**: the form snapshots defaults on first bind; `IsModified` /
  `ResetToDefault` (one undoable edit).

## Widget model

A widget draws and interacts with one property field's control rectangle.

```cpp
class ReflectedWidget {
    virtual PropertyEditorKind Kind() const = 0;
    virtual int RowCount() const;                       // multi-row widgets
    virtual void Paint(host, ctx, field, rect) = 0;
    virtual bool OnDown(host, field, event, rect);      // return true to capture
    virtual bool OnMove(host, event);
    virtual bool OnUp(host, event);
    virtual bool HasPopup() const;
    virtual void PaintPopup(host, ctx);
    virtual bool OnPopupPointer(host, event);
    virtual bool OnEscape(host);
};
```

`ReflectedFormView` lays out rows and dispatches:

- `DrawField` -> if a widget is registered for `field.kind`, `widget->Paint`; else the
  view's own container drawing (struct/sequence) or the enum fallback.
- Pointer `DOWN` -> a registered widget's `OnDown`; if it returns true the view captures
  it and forwards `MOVE`/`UP`. Otherwise the view handles container behaviour
  (struct/sequence expand, sequence add/remove).
- Pointer while a widget popup is open -> `widget->OnPopupPointer`; `Esc` -> `OnEscape`.

Widgets borrow host services via `ReflectedWidgetHost` (`Form/Commands/Skin/Text/
ViewBounds/MarkDirty/RefreshForm/IsHovered/BeginTextEdit`). `BeginTextEdit` gives any
widget the host's inline text editing (caret, input filter, Enter/Esc, invalid state),
with a commit callback that parses and applies the value.

## Reflection-driven, not type-driven

- The framework never does `GetAs<DomainType>()`; only the **specialized widget** for a
  domain type may (and even then, colour/rotation are handled through float members).
- Scalars (`bool`, integer, floating, string) are matched by `registeredId` — the
  reflection "scalar layer" every editor needs.
- Vector components, colour channels (r/g/b/a) and rotation (quaternion x/y/z/w) are
  located by member name among the struct's float members.

## Extending

- **New control / domain type**: implement `ReflectedWidget`, register it
  (`ReflectedWidgetRegistry::Get().Register(kind, ...)`), and resolve the kind via
  `PropertyEditorRegistry::RegisterType(typeId, kind)` or the `EDITOR_KIND` attribute.
- **New data flow**: add a `CommonPropertyKey` attribute; parse it in
  `ReadPropertyAttributes`; read it from a widget/`EditorControl`.
- **Restyle**: `SetDefaultUiTheme(theme)` or build a `UiSkin` from a custom `UiTheme`.
- **Data sources**: implement `IEditorPropertySource` / `IEditorConfigSource`.
- **Member appearance**: `PropertyEditorRegistry::RegisterMemberAppearance(typeId, member,
  {label, order})` relabels/reorders a struct's members in the form (used by `Transform`
  -> Position / Rotation / Scale).

## Transform

`Transform { Vector3 translation; Vector3 scale; Quaternion rotation; }` is already
reflected. It is edited through the generic struct recursion with:

- member appearances relabelling/ordering it as **Position / Rotation / Scale**;
- `Quaternion` resolved to `PropertyEditorKind::Rotation`, whose widget shows Euler
  **YZX** degrees (the engine convention) with a toggle to raw x/y/z/w.

## Notes / follow-ups

- Axis order is fixed to YZX (runtime convention); a multi-order dropdown is not planned.
- The generic leaf kind `Enum` still uses the view's inline popup; extracting it into an
  `EnumReflectedWidget` (or a shared `ListPopup`) is the remaining tidy-up.
- Widgets are per-kind singletons (only one popup/drag is active at a time).
