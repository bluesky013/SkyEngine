# SkyEngine Editor: Reflection Widget Framework

The sandbox editor renders and edits reflected data without knowing concrete C++
domain types. This document describes the framework's layers, the widget model,
and how to extend it.

## Layers

```
engine/sandbox/core            (toolkit/render-independent; Core + Framework)
  PropertyDescriptor           read/write one reflected member via its getter/setter
  ReflectedForm / PropertyField recursive form model (struct/sequence), undoable Edit, defaults
  PropertyEditor               PropertyEditorKind / EditorControl / PropertyEditorRegistry
  PropertyValidation           type parsing, input filtering, range/step constraints, formatting
  PropertyChangeNotifier       external-change revision + callbacks
  EditorAssetCatalog           asset lookup seam (default: framework AssetDataBase)
  EditorPropertySource         IEditorPropertySource / RegisteredPropertySource

engine/sandbox/ui              (sky::ui primitives + extension provider seam; Core + UI only)
  UiDraw / UiTheme             painter primitives (rounded/gradient/shadow/slider/color) + theme

engine/sandbox/widgets         (EditorWidgets; EditorUi + EditorCore + UI + Core + Framework)
  UiSkin                       component skin (draws widgets from a UiTheme via UiDraw)
  ReflectedWidget              per-kind widget base
  ReflectedWidgetHost          services a widget borrows from its host (the form view)
  ReflectedWidgetRegistry      one form's kind -> widget instances
  GenericReflectedWidget       Bool / Integer / Float / String / Vector
  EnumReflectedWidget          enum dropdown
  AssetReflectedWidget         asset references (type-validated picker)
  ColorReflectedWidget         colour (reflection channels + ColorPicker)
  RotationReflectedWidget      quaternion shown/edited as Euler (YZX, degrees) or raw
  ColorPicker                  standalone Blender-style colour popup (no engine colour type)
  ReflectedFormView            reusable form view; implements ReflectedWidgetHost; dispatches by kind

engine/sandbox/shell           (host, docking, modal and concrete panel/viewer composition)
  ReflectionDemoPanel          thin demo consumer
```

Include shared controls through `editor/widgets/<Name>.h` and link `EditorWidgets`.
Feature editor modules can use the same forms, `UiSkin`, and action-driven chrome without
linking Shell or Aurora. The former `editor/shell/...` control headers have no forwarding aliases.

## Data flow

- **Read**: `PropertyDescriptor::GetValue()` -> reflected getter.
- **Write**: `ReflectedForm::Edit(field, value, CommandService)` constructs a command through
  the optional domain factory, or a generic `PropertyEditCommand` by default. Reset and sequence
  add/remove use the same path. Accepted edits notify a still-valid host once; asset persistence belongs
  to the domain command so Undo/Redo also update it. Live previews use reflected setters without
  recording a command and restore their starting value when cancelled on a valid model.
- **Structure**: `TypeNode::members / enums / containerInfo` drive struct, sequence,
  and enum handling. Vector components are read/written by `isFloatingPoint` members.
- **Kinds**: resolved by attribute (`EDITOR_KIND`), compatibility attribute (`ASSET_TYPE`),
  registered type handler (`RegisterType`), or inference (`registeredId` for scalars).
- **External change**: a data owner calls the form context's `PropertyChangeNotifier::Notify()`;
  the default context uses `EditorCore::GetPropertyChanges()`. The view subscribes to property and
  command changes; callbacks mark layout dirty and retain pending refresh rather than rebuild
  fields immediately. The host calls `SynchronizeLayout()` **before** its incremental skip and
  allocation decisions. Safe pending work is consumed there; active text/drag/popup interactions
  defer refresh without dropping it. Revision polling also preserves the legacy path. Destruction
  removes its subscriptions and invalidates their weak lifetime guards. Arbitrary unnotified
  object writes still require explicit Refresh or conservative volatility.
- **Preferred height**: Bind/Refresh and structural changes prepare row extents before Paint.
  Header/expansion/sequence changes and `Widgets().Register` invalidate layout on the owning form.
  A registered custom widget's unnotified internal RowCount changes require Refresh/volatility;
  registry notifications do not observe arbitrary custom state.
- **Hit areas**: a widget's `OnDown` owns only the control rect; the view dispatches
  the pointer to a widget only when it lands inside `controlRect`, so clicks on the row's
  label / **revert icon** fall through to `BeginInteraction` (previously a widget
  swallowed every click on its row, so the reset icon did nothing).
- **Reset to default**: the reset baseline is the type's **default-constructed value**
  (`MakeDefaultValue` via `TypeInfoRT::newFunc`), so reset means "restore the type
  default" (UE/Godot-style) and persists across load/save. `IsModified` / `ResetToDefault`
  (one undoable edit); `ResetToDefault` writes the baseline back through the normal
  undoable edit path. Types without a default constructor fall back to a snapshot of the
  bound values.

## Widget model

A widget draws and interacts with one property field's control rectangle.

```cpp
class ReflectedWidget {
    virtual PropertyEditorKind Kind() const = 0;
    virtual void ResetInteraction(host, ReflectedInteractionReset mode);
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
ViewBounds/BindingLifetime/MarkDirty/RefreshForm/IsHovered/BeginTextEdit`). `BeginTextEdit` gives any
widget the host's inline text editing (caret, input filter, Enter/Esc, invalid state),
with a commit callback that parses and applies the value.

Each `ReflectedFormView` owns its `ReflectedWidgetRegistry`, exposed through `Widgets()`.
Popup/drag state belongs to those instances, and text-edit state belongs to the form. Interaction
reset has two explicit modes. `CancelInteractions()` restores live previews and requires the model
to remain valid; `Refresh()` has the same requirement before rebuilding the current model.
`Bind()`, `Unbind()`, and form destruction use abandonment: they drop old callbacks/targets without
reading or writing the former model. Hosts requiring rollback must cancel **before** replacing
valid storage; after replacement they must abandon/rebind before further input or synchronization.
`Unbind()` also clears the form model so pending notifications cannot rebuild an obsolete pointer.

`ReflectedWidget::ResetInteraction(host, mode)` must drop target pointers in either mode and must
never access model data for `ReflectedInteractionReset::Abandon`. The form's weak `BindingLifetime()`
expires at interaction/field reset independently of notifier subscription lifetime; built-in color
and inline callbacks reject expired generations. Extensions retaining callbacks outside their
resettable state must use the same guard. This is a single-threaded validity signal, not model
ownership. Shared borrowed services must still outlive the form.

AssetViewer cancels/detaches before replacing settings and destroys child forms before owned models.
WorldConfigPanel refresh safely abandons storage that may already have been replaced at the same
document address. Its selected-subsystem switch cancels before saving a still-live config.
The repair is specified in
[`fix-editor-binding-lifetime`](../../openspec/changes/archive/2026-10-17-fix-editor-binding-lifetime/design.md),
covering **UI-03** in [the reviewed backlog](ui-layout-and-chrome-plan.md#9-reviewed-backlog-2026-10-16).

`CommandService::Undo/Redo` are now the shared before-history boundary (**UI-05**). Each form
subscribes to its injected service through `AddBeforeHistoryCallback` and cancels its live
interactions before that service changes the history cursor. This covers shell shortcuts, menus,
toolbars, and direct service calls using the same history. Form-local Ctrl+Z/Y still routes to
that service before inline text dispatch; preview cancellation no longer depends on that key path.
Preparation runs even for an empty stack, in registration order; removed subscribers are
skipped, recursive Undo/Redo is rejected, and form destruction removes the subscription. See
[`fix-editor-history-preview-order`](../../openspec/changes/archive/2026-10-17-fix-editor-history-preview-order/design.md).

`CommandService::Clear()` also advances a history generation. If a synchronous Do/Undo/Redo callback
clears or replaces the session, the in-flight old command is not pushed back into the new Undo/Redo
stacks or pending transaction. Clear does not undo work already applied by the command; the domain
operation and its persistence keep their own meaning.

Preferences now injects a dialog-owned CommandService and binds type-aware owned working copies.
Apply/OK writes eligible reflected members to still-matching live page registrations; Cancel/close
discards unapplied changes. Page reorder retains stable working addresses and cross-page history;
membership/re-registration changes clear history before old storage is released. The Shell routes
Preferences keys before global actions. See
[`fix-preferences-session-history`](../../openspec/changes/archive/2026-10-17-fix-preferences-session-history/design.md)
for callback/lifetime limits; generic plugin-page persistence and native application remain T11-C.

AssetViewer now injects session-owned history into its forms and routes scoped chrome to that
history. Its cook-setting command factory resolves a fresh effective settings copy and records
asset UUID, target, schema UUID, and before/after sparse target overrides. Do/Undo/Redo share the
same persistence/notification path; disposable settings reloads do not invalidate those commands.
Open/reopen/switch/close clear the session; missing targets/assets/schema invalidate its history.
See [`fix-asset-settings-history`](../../openspec/changes/archive/2026-10-17-fix-asset-settings-history/design.md)
for **UI-04**, including safe stale-command behavior and borrowed catalog-service lifetime.

`ReflectedForm::SetEditCommandFactory` is the UI-free seam for domain commands. Scalar, nested,
reset, and AddSequenceElement/RemoveSequenceElement edits all use it; factories may not retain
the transient descriptor or form fields. The default factory still creates generic raw-target
commands, so other document owners must keep those targets alive or scope/clear their history.
Context injection alone does not supply that ownership contract. Arbitrary custom asset source
writeback remains provider/domain work.

Field descriptors for nested structs point into the owning field's `Any`, which may use inline
storage. ReflectedForm sorts member metadata first, reserves final field/child capacity, and
constructs fields in place with their final parent links. Moving or sorting built fields would
invalidate these addresses. Borrowed field pointers/descriptors expire on Rebuild/Bind; generic
commands must resolve through their valid root target rather than retain a disposable child field.
See [`fix-reflected-field-storage-lifetime`](../../openspec/changes/archive/2026-10-17-fix-reflected-field-storage-lifetime/design.md).

Catalog notifications are synchronous. `ReflectedForm::Edit` copies its factory/changed callback
before dispatch and checks a weak core binding generation afterwards, so a destroyed/rebound form
does not receive an obsolete changed callback. Its active-edit scope retains only an independent
counter state. It does not keep the form or binding generation alive and does not access the form
when the scope ends.

AssetViewer queues Open/Close/catalog replacement/Reload while any owned form reports `IsEditing()`.
A nested `SynchronizeLayout()` during that edit only marks pending work; the next safe host layout
after the widget event returns applies it. Scope cleanup deliberately does not drain the queue,
because the widget/inline-event tail still uses the form. Host layout synchronization must run at
that safe boundary, rather than from an arbitrary dispatch tail. External code must keep views and
their command service alive throughout event/service dispatch; arbitrary deletion of a dispatching
view is not made safe by the core callback guard.

The two-argument constructor uses the shared default `EditorCore` command service, property editor
registry, change notifier, and default theme. These defaults use `sky::Singleton` storage and are
unique across module DLLs. For document-specific history or appearance, use the constructor taking
`ReflectedFormContext{commands, propertyEditors, propertyChanges, theme}`. It borrows those
host-owned objects, which must outlive the form; the context wrapper itself need not be retained.
The reusable controls do not move document ownership into process-wide storage.

Hosts must keep a borrowed model valid through normal input, cancellation, history preparation,
and `Refresh()`. When external code has already replaced model storage, notify with `Bind/Unbind`
or WorldConfigPanel's same-address `RefreshDocument()` before any further input/history/layout.
Weak binding signals become invalid when the host detaches or rebuilds; they do not automatically
observe arbitrary raw-pointer expiry. AssetViewer keeps child cleanup before owned-model destruction
and requires its borrowed catalog to outlive the session and its domain commands.

Shell-owned asset-viewer action predicates/handlers also carry weak owner validity, because the
process registry and copied action records can outlive a shell. Shell destruction invalidates them
before child teardown while preserving a newer shell's same-ID registrations. A live shell also
clears its viewer target before rebuilding the owning tree, so chrome predicates cannot read the
previous viewer between destruction and recreation; see
[`fix-editor-action-owner-lifetime`](../../openspec/changes/archive/2026-10-17-fix-editor-action-owner-lifetime/design.md).
This object guard does not implement scope-instance ownership or DLL-unload callback registration.

## Reflection-driven, not type-driven

- The framework never does `GetAs<DomainType>()`; only the **specialized widget** for a
  domain type may (and even then, colour/rotation are handled through float members).
- Scalars (`bool`, integer, floating, string) are matched by `registeredId` — the
  reflection "scalar layer" every editor needs.
- Vector components, colour channels (r/g/b/a) and rotation (quaternion x/y/z/w) are
  located by member name among the struct's float members.

## Extending

- **New control / domain type**: implement `ReflectedWidget`, register it
  on the consuming form (`view.Widgets().Register(kind, std::make_shared<MyWidget>())`), and resolve
  the kind via `PropertyEditorRegistry::RegisterType(typeId, kind)` or the `EDITOR_KIND` attribute.
- **New data flow**: add a `CommonPropertyKey` attribute; parse it in
  `ReadPropertyAttributes`; read it from a widget/`EditorControl`.
- **Restyle**: `SetDefaultUiTheme(theme)` or build a `UiSkin` from a custom `UiTheme`.
- **Data sources**: implement `IEditorPropertySource`.
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
- `EnumReflectedWidget` supplies the enum popup alongside the other per-form controls.
- Register fresh interaction widgets on each form; do not share a stateful widget instance between forms.
