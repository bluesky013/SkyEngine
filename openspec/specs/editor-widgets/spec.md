# editor-widgets Specification

## Purpose
Provide shared editor chrome and reflected controls behind the editor framework, with consistent actions, appearance and data-view separation.

## Requirements
### Requirement: Shared editor widget framework
The editor SHALL provide an `EditorWidgets` target in `engine/sandbox/widgets` for reusable UiSkin, ReflectedFormView, ReflectedWidget/Host/registry and built-in field widgets, ColorPicker, ToolItemButton, ActionBarBuilder, ToolBar, MenuBar, and StatusBar. It SHALL depend toward EditorCore and EditorUi and their runtime UI/Core dependencies, and SHALL NOT depend on EditorShell, EditorRender, SandboxModule, Qt, or concrete Aurora rendering. EditorCore SHALL remain UI-free; EditorUi SHALL remain a low-level Core + UI module without a dependency on EditorWidgets or EditorCore.

#### Scenario: Widget consumer does not link Shell
- **WHEN** an editor extension or a test links EditorWidgets to create a reflected form or shared chrome
- **THEN** it SHALL obtain the shared widget APIs without linking EditorShell or a render/platform host

#### Scenario: Dependencies point toward the lower layers
- **WHEN** editor framework target dependencies and shared widget includes are inspected
- **THEN** EditorShell SHALL consume EditorWidgets, EditorWidgets SHALL NOT include/link Shell, and neither EditorCore nor EditorUi SHALL depend on EditorWidgets

#### Scenario: Editor target gating
- **WHEN** the Sandbox/editor build is disabled
- **THEN** EditorWidgets SHALL NOT be required by the headless EditorCore build

### Requirement: Widget API header ownership
The extracted widget APIs SHALL be exposed under flat `editor/widgets/<Name>.h` paths. All repository consumers SHALL migrate from the previous `editor/shell/...` or `editor/shell/widgets/...` paths, and the migration SHALL NOT provide old-path forwarding headers. Existing internal users SHALL link the target that owns the APIs.

#### Scenario: Shared widget includes resolve
- **WHEN** a caller links EditorWidgets and includes `editor/widgets/UiSkin.h`, `editor/widgets/ReflectedFormView.h`, or shared chrome headers
- **THEN** those headers SHALL resolve without Shell include directories

#### Scenario: Repository migration is complete
- **WHEN** extracted widget include paths are searched in repository sources
- **THEN** consumers SHALL use the new `editor/widgets/...` paths and SHALL NOT reference removed shell-owned widget headers

### Requirement: Host policy stays outside shared widgets
Window/modal composition, dock layout widgets, selection-driven inspector panels, and asset-viewer host policy SHALL remain owned by EditorShell. Shared StatusBar SHALL accept host-resolved display state rather than owning project/window/service polling policy. Shared action chrome SHALL accept host invocation without depending on an EditorShell instance.

#### Scenario: Status text is supplied by its host
- **WHEN** a host updates its status text
- **THEN** the shared StatusBar SHALL display that text without querying a Shell or render host

#### Scenario: Shell composes shared controls
- **WHEN** Shell creates a dialog, viewer, or reflected settings panel
- **THEN** it SHALL reuse the shared controls while retaining its host/modal/selection policy

### Requirement: Feature editor uses the shared presentation
Aurora.Render.Editor SHALL link EditorWidgets without linking EditorShell. MaterialEditorView SHALL use the shared UiSkin and UiTheme for its adopted panel/row/field drawing, dimensions, and typography. Its shader-driven dynamic row schema and save behavior SHALL remain available during this extraction.

#### Scenario: Aurora material editor shares the skin
- **WHEN** the material editor paints its adopted controls
- **THEN** it SHALL invoke the shared skin with theme-derived presentation values rather than merely linking an unused widget target

#### Scenario: Dynamic shader rows remain usable
- **WHEN** shader/material reflection yields different property names, types, or counts
- **THEN** the material editor SHALL retain its dynamic rows and existing source-save path without requiring a fixed reflected config structure

### Requirement: Immutable picker wheel is registry-owned
ColorPicker SHALL reuse its immutable wheel resource through reserved `WHEEL_TEXTURE_ID` (`0x80000000`) and `RegisterTextureAs`. This id SHALL be separate from glyph-atlas page ids starting at `0x40000000`. Each Open SHALL ensure registration against the current texture registry without relying only on a per-picker ready flag. The texture registry SHALL own the shared wheel until renderer shutdown; closing or destroying one picker SHALL NOT immediately release a resource that another picker or an in-flight frame can still use. This requirement SHALL NOT require a broader surface/registry lifetime redesign.

#### Scenario: Multiple picker instances reuse a wheel
- **WHEN** separate forms open color pickers using the same texture registry
- **THEN** they SHALL reference the same reserved wheel id and SHALL NOT allocate one wheel texture per form

#### Scenario: Picker destruction preserves registry ownership
- **WHEN** a picker closes or its form is destroyed
- **THEN** it SHALL NOT call per-picker ReleaseTexture for the shared wheel, and renderer shutdown SHALL remain responsible for registry resource cleanup

#### Scenario: Opening ensures the builtin remains registered
- **WHEN** a picker is opened against a valid registry after its previous registration state has been cleared
- **THEN** it SHALL ensure the reserved wheel resource through RegisterTextureAs rather than skipping registration due to an old picker-local ready flag
