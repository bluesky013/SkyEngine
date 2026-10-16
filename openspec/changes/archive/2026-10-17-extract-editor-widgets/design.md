## Context

`refactor-ui-foundations` separates the UI foundation's contracts and runtime implementation into independent targets within `engine/ui` and makes container arrangement explicit before paint. This change builds on that uncommitted/unarchived source state. It implements the next roadmap step: make shared editor widgets usable by a feature editor independently of the editor Shell.

Today `EditorCore` provides headless services and reflected property models; `EditorUi` provides low-level UI draw/theme primitives and `IAssetViewProvider`; `EditorShell` owns both reusable controls and host policy. Aurora's material view avoids Shell but therefore draws its own fields and uses fixed colors/sizes. Moving reusable controls exposes another existing boundary: the static EditorCore/EditorUi/Shell libraries can be linked separately into SandboxModule and a feature DLL. Function-local static services/themes and module globals are then duplicated. Conversely, the current field-widget registry stores mutable drag/popup state globally, so making that registry process-shared would create cross-form interaction bugs.

## Goals / Non-Goals

**Goals:**

- Provide a shared `EditorWidgets` framework that a feature editor can link without Shell.
- Preserve the UI-free EditorCore and low-level EditorUi dependency boundaries.
- Make default services/theme/catalog injection actually shared when called from separate DLLs.
- Make field-widget interaction state local to its form and clear stale interaction pointers before binding/rebuilding fields.
- Preserve the existing reflected edit/undo, metadata, reset, and external-notification behavior, with explicit context injection available for isolated consumers.
- Integrate the Aurora material view with shared skin/theme drawing and verify the boundary in builds and dynamic-module tests.

**Non-Goals:**

- Extracting the modal/window host, dock layout, selection-driven panels, or complete Shell into a new subsystem.
- Creating a process-global widget factory or retaining shared stateful widget instances.
- Completing document-scoped service ownership, surface/device lifetime redesign, a full Geometry system, constrained Measure, or game/editor theme-protocol unification.
- Replacing the material editor's shader-driven schema with fixed reflected configuration data or completing all dynamic-row/undo unification in this milestone.
- Adding new per-config or per-domain hand-built settings UIs.

## Decisions

### Add a shared editor framework layer above Core and Ui

`engine/sandbox/widgets` builds the static `EditorWidgets` target. Its public API uses flat `editor/widgets/...` paths. Its implementation may depend on `EditorCore`, `EditorUi`, runtime `UI`, and their required Core/Framework interfaces; it does not depend on EditorShell, EditorRender, SandboxModule, Qt, or concrete Aurora rendering. Configure it after EditorUi and before Shell, under the existing Sandbox/editor build gate.

| Layer | Owned responsibilities |
|---|---|
| `EditorCore` | CommandService, reflection/property models and metadata, property notifications, headless extension/catalog seams |
| `EditorUi` | UiDraw, UiTheme/default-theme storage, asset-view UI provider seam; Core + UI only |
| `EditorWidgets` | Reusable form/field interaction, UiSkin, controls and action-driven chrome |
| `EditorShell` | Main/window/modal composition, docking, selection-driven panels, dialog policy and asset-viewer hosting |
| `EditorRender` / feature editor | Render/platform hosting or domain-specific view integration; consumes lower framework layers |

Extract fourteen header/implementation groups: UiSkin; ReflectedWidget/Host/registry; GenericReflectedWidget; AssetReflectedWidget; ColorReflectedWidget; RotationReflectedWidget; EnumReflectedWidget; ColorPicker; ReflectedFormView; ToolItemButton; ToolBar; MenuBar; ActionBarBuilder; StatusBar.

ModalDialog, DockWidgets, ReflectedInspectorPanel, WorldConfigPanel, PresetEditorPanel, AssetViewerWidget, and other host/domain panels remain in Shell. StatusBar is reusable presentation receiving resolved text from its host; gathering project/FPS/selection state remains host policy. ActionBarBuilder consumes EditorActionRegistry/IconRegistry and receives an invoker callback rather than calling EditorShell directly. Toolbar/menu/shortcut behavior continues to use the existing registry and scope rules.

Alternative: extend EditorUi to own all forms and action widgets. This would force the low-level provider seam to depend on EditorCore and widen all extension dependencies. Alternative: leave controls in Shell and add convenience wrappers. That would retain the Shell link and duplicate policy boundary.

### Use an internal header migration without forwarding headers

Extracted APIs move from `editor/shell/...` or `editor/shell/widgets/...` to flat `editor/widgets/<Name>.h`. Update every repository include and CMake consumer together. Do not add old-path forwarding headers: these are internal framework APIs, and retaining shell-branded aliases would obscure which target owns them. Existing namespaces and class names remain where practical; the internal include-path and global-widget-registry access changes are explicitly breaking.

The shared module's headers and sources must not include Shell headers. Shell links Widgets, and Aurora.Render.Editor links Widgets without linking Shell. Framework documentation is updated to show the same ownership.

### Share default service data through Environment, not per-DLL statics

Store the objects returned by `EditorCore::GetCommandService`, `GetPropertyEditors`, and `GetPropertyChanges` in an Environment-backed `sky::Singleton<T>` bundle. Keep the service classes independently constructible for local/headless contexts. Preserve the existing getters as default access points, and ensure actual default form construction uses those same shared objects.

Move the asset-catalog override pointer behind Environment-backed shared state as well; the default EditorAssetCatalog singleton already has process scope. A host's `SetEditorAssetCatalog` must be visible to a widget linked into another DLL, and clearing it must restore the shared default lookup.

Put the default UiTheme in an Environment-backed singleton. Preserve the existing `GetDefaultUiTheme`/`SetDefaultUiTheme` API and stable address: setting a new value updates the same theme object, so an existing UiSkin that stores its address sees the new values, including from another module. An explicitly supplied custom theme remains owned by the caller and must outlive its form/skin.

Process-shared initialization must not use a function-local ordinary `static bool`. Initialize owned shared state in its singleton construction or use an Environment-backed once mechanism when needed. Register default property metadata idempotently without replacing an extension's existing metadata. Do not apply a process-once guard to per-form widget creation: each form needs its own defaults, and an injected metadata registry is an independent target.

Alternative: turn EditorCore/Ui into shared libraries to centralize globals. That changes the broader module/export/packaging model and still would not solve shared stateful field widgets. Environment-backed state follows the repository's established cross-DLL contract with smaller scope.

### Inject services and keep stateful widgets inside their form

Add `ReflectedFormContext` containing references to CommandService, PropertyEditorRegistry, PropertyChangeNotifier, and const UiTheme. Add a `(text, title, context)` constructor; keep the existing `(text, title)` constructor delegating to the shared defaults. Resolution, editing/undo, refresh polling, and skin creation all use the selected context, not a later global lookup. The context services/theme are borrowed and must outlive the form.

Each ReflectedFormView owns a ReflectedWidgetRegistry and its stateful widget instances. Expose `Widgets()` for form-local extension registration; remove `ReflectedWidgetRegistry::Get`. A custom widget registered for one form affects that form only. Sharing generic behavior across multiple kinds inside one form is acceptable; sharing mutable widget instances between different forms is not the default design. Widget ownership/creation remains local rather than introducing a process-wide widget factory.

This is necessary because ColorReflectedWidget/ColorPicker, GenericReflectedWidget, AssetReflectedWidget, EnumReflectedWidget, and RotationReflectedWidget hold interaction state or field pointers. Another form must not see their popup, capture, target, drag, or local mode. A shared registry containing these objects would allow one form to route events or repaint using another form's field.

### Clear interactions before replacing reflected fields

Bind and explicit Refresh/rebuild reset inline edit callbacks, drag/capture pointers, hovered fields, popups, and active field-widget state before rebuilding field storage. Widget extension registrations remain local and usable after binding; resetting interaction must not silently discard the form's extension policy. The reset path must not invoke a stale commit callback against the previous object. Review follow-up UI-03 in `docs/editor/ui-layout-and-chrome-plan.md` records a remaining gap: preview rollback still calls the previous descriptor, so model storage must currently survive cancellation. Post-replacement rebinding and owner/child destruction need an explicit abandon/detach protocol.

External refresh notifications continue to defer while editing or dragging and are applied after the interaction completes. Deferral is different from explicit Bind: binding a different object cancels old interaction and immediately switches to the new data. Rebuilt forms retain their configured context, defaults/baseline policy, and edited callback behavior.

Alternative: retain a global registry and check the active form on every callback. This adds mutable cross-form ownership and leaves popup/field-pointer lifetime coupled to a global object. Local ownership makes the lifetime explicit and testable.

### Cancel previews before history traversal and retain active widgets

The form-local Undo/Redo path first cancels current interaction, including restoring any uncommitted live color preview, and only then traverses command history and refreshes the form. The 2026-10-16 review found that Shell action dispatch bypasses this local ordering; UI-05 in `docs/editor/ui-layout-and-chrome-plan.md` tracks the missing shared history boundary. Reversing that order would let Refresh cancel the preview after Undo/Redo and overwrite the value just restored by the command. Color picker commit dispatch snapshots its callbacks/color before invoking host callbacks, since a successful edit can synchronously refresh the form and close the picker.

The form retains shared ownership of the active widget, and event/popup dispatch takes a local shared reference across callbacks. Replacing that kind's entry in `Widgets()` must not destroy a widget still serving capture or a popup. Reset includes the retained active instance even if it has been replaced in the registry, then releases its interaction ownership. This keeps ownership inside the form and does not reintroduce a process-global widget registry.

Inline edit callbacks also retain the widget that created them, independently of pointer capture. Pointer release and a subsequent registry replacement must not destroy a widget whose callback still captures its `this` pointer. Commit dispatch keeps a local callback copy across synchronous Refresh; commit, cancel, and rebinding release that ownership when the callback is no longer needed.

### Keep the immutable color wheel owned by its texture registry

Per-form ColorPicker instances share an immutable wheel resource through `ColorPicker::WHEEL_TEXTURE_ID` (`0x80000000`). This reserved builtin id is separate from glyph-atlas page ids starting at `0x40000000`. Each Open ensures registration with `RegisterTextureAs`, allowing an existing registry entry to be reused across picker/form instances without allocating another GPU wheel. Registration is idempotent and is not suppressed solely by a per-picker `wheelReady` flag that could survive a renderer reset.

Closing or destroying a picker does not release this shared builtin. The `sync-editor-surface-textures` follow-up owns CPU wheel pixels in UITextSystem's backend-free shared catalog, replays them to every attached surface, and re-ensures explicit registration after a backend texture reset. It also queues renderer updates and retires removed/replaced GPU generations until the host's completed-frame boundary; `fix-editor-surface-gpu-teardown` supplies safe surface shutdown. These follow-ups address the review's cross-surface and in-flight lifetime gaps; native GPU/display verification remains a separate gate. Per-form wheel reuse itself does not establish GPU safety or require a registry pointer/callback lifetime in the form.

### Make Aurora consume the shared skin and theme

Aurora.Render.Editor links EditorWidgets and MaterialEditorView uses the shared UiSkin for its fields/rows/panels and UiTheme for the corresponding colors, dimensions, and fonts. It continues to register through the EditorUi asset-view seam without Shell. This integration must call shared drawing behavior; adding an unused link/include is not sufficient.

The material rows are generated from shader/material reflection: their property names, types, counts, texture references, and technique/state entries vary with the shader schema and instance overrides. A fixed reflected C++ config structure cannot represent that complete dynamic schema without another descriptor/adapter layer. This milestone therefore preserves the existing dynamic row data and domain save behavior while adopting shared presentation. It does not introduce a new bespoke config form. A subsequent change can adapt dynamic row descriptors to the reflected widget model with a deliberate transaction/undo contract.

## Risks / Trade-offs

- [Internal header paths and static target ownership change] -> Migrate all repository consumers together, remove stale includes, and build both Shell and a feature editor.
- [Shared service state changes previously isolated histories/metadata] -> Preserve explicit local construction and context injection; prove default access identity and real form use across DLLs.
- [Sharing widget state would cross-contaminate forms] -> Keep registry/instances form-owned and test independent popup/capture/custom registration behavior.
- [Bind/Refresh invalidates field pointers] -> Reset interactions before data rebuild and test rebinding while edit/popup/drag state exists.
- [History refresh cancels a preview after the command has restored a value] -> Cancel previews before Undo/Redo and test the final value as well as the history cursor.
- [A registry replacement destroys an active widget] -> Keep shared ownership across active interaction and callback dispatch, including reset of replaced instances.
- [Per-form pickers allocate orphaned wheels or release resources used by old frames] -> Reuse the immutable reserved-id wheel per registry and let renderer shutdown own its GPU cleanup.
- [Theme pointer becomes stale or updates only one DLL] -> Keep stable singleton-owned storage and test a change from one module observed by already-created skin/widgets in another.
- [The material editor remains a dynamic custom view] -> Document the schema reason, adopt the shared presentation now, and defer dynamic-model transaction unification explicitly.

## Migration Plan

1. Preserve `refactor-ui-foundations` in the working tree; build this change on top of its `UI.Interface`/runtime target separation within the `engine/ui` foundation and its arrangement API.
2. Extract the fourteen reusable groups, introduce EditorWidgets/build ordering, and migrate internal headers and dependencies.
3. Centralize default services/theme/catalog state, then update forms for injected contexts and local widget/interaction lifetime.
4. Integrate MaterialEditorView with the shared skin/theme and update framework docs.
5. Reconfigure and run focused core/widgets/shell/feature builds, real dynamic-module regressions, and strict OpenSpec validation. Record unavailable platform checks rather than claiming them passed.

No persisted data migration. Rollback this change's widget moves and dependency/API changes together, leaving the preceding foundation change intact. Neither change is automatically committed or archived; those actions still require the user confirmations defined in AGENTS.md.

## Validation Strategy

- A dedicated EditorWidgets test target links Widgets without Shell and verifies form binding/edit/undo/context injection and action-driven chrome.
- Two distinct forms verify widget-instance/custom-registration/popup/capture isolation and safe Bind/Refresh interaction reset.
- Review regressions verify Undo/Redo during a live color preview, replacing an active widget before further dispatch/reset, and repeated picker creation/open/close reusing one reserved-id wheel without per-picker release.
- A real shared test module and a host executable each statically link the relevant editor framework code. Tests call across that dynamic-library boundary to verify shared default services, metadata visibility, undo/notification use, stable theme updates, and catalog override visibility. Calling getters repeatedly only within one executable is insufficient.
- Existing EditorCoreTest and EditorShellTest cover retained boundaries/host behavior; build Aurora.Render.Editor and SandboxModule to validate actual consumers.
- Configure the existing headless/editor-disabled path to confirm EditorCore stays UI-free and editor widget targets remain gated.

## Open Questions

- No question blocks this milestone. Document-scoped ownership and dynamic material form/undo integration remain follow-up designs.
