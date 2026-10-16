## Why

Feature editor extensions can supply custom UI through `EditorUi`, but shared skins, reflected forms, and action-driven chrome still live in `EditorShell`. Extracting them lets feature editors reuse the editor framework directly; doing so also requires fixing default service/theme state that is currently duplicated when static libraries are linked into different module DLLs.

## What Changes

- Add `EditorWidgets` in `engine/sandbox/widgets`, between `EditorCore`/`EditorUi` and `EditorShell`, exposing shared skins, reflected forms/field widgets, ColorPicker, ToolItemButton, action bar construction, ToolBar, MenuBar, and StatusBar.
- Keep `EditorCore` UI-free and keep `EditorUi` limited to low-level draw/theme primitives and the asset-view provider seam. Keep window/modal, docking, selection-driven panel, and asset-viewer composition in Shell.
- Make default command, property-metadata, property-change, theme, and asset-catalog override state process-shared across DLLs through Environment-backed singleton storage.
- Give each reflected form its own stateful field widgets and an extensible local registry; add explicit command/metadata/notifier/theme context injection while preserving the default constructor behavior.
- Reset form interaction state before binding/rebuilding field data so captured/popup/edit pointers do not outlive the old fields.
- Cancel live previews before Undo/Redo, retain shared ownership of actively dispatched widgets, and reuse the color wheel as an immutable registry-owned builtin texture.
- Make the Aurora material editor use shared UiSkin and UiTheme without linking Shell. Preserve its shader-driven dynamic row model for this milestone.
- **BREAKING**: Move the extracted internal headers to flat `editor/widgets/...` paths and migrate all repository consumers without compatibility forwarding headers. Remove the process-global `ReflectedWidgetRegistry::Get` access pattern in favor of the form's registry.
- Add isolated framework tests and real dynamic-module regressions for service/theme visibility, per-form interactions, and binding safety.

## Capabilities

### New Capabilities

- `editor-widgets`: the shared editor widget framework, its module boundary, header ownership, and use by feature editors.

### Modified Capabilities

- `editor-extension-ui`: feature editor dependencies can include EditorWidgets without linking Shell, while EditorUi remains a low-level seam.
- `editor-reflected-form`: shared view/control ownership, injected form context, form-local widgets, and safe rebinding.
- `editor-core`: process-shared default editor services and asset-catalog injection across DLLs.
- `editor-ui-styling`: one stable default theme shared by Shell and feature module widgets.
- `editor-command-registry`: shared action-to-toolbar/menu/accelerator construction outside Shell.

## Impact

- `engine/sandbox/{core,ui,widgets,shell,test}`, editor framework CMake ordering, and all repository includes of the extracted headers.
- `engine/aurora/editor` links `EditorWidgets` and uses its skin and theme; no Shell dependency is introduced.
- This change depends on the preceding `refactor-ui-foundations` source changes and preserves them in the working tree. Neither change is committed or archived as part of this work.
- No persisted document or layout format changes; dynamic material-row unification, full document/service ownership, and broader UI Geometry/theme-protocol redesign remain subsequent work.
