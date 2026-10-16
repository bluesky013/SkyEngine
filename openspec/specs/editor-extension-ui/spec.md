# editor-extension-ui Specification

## Purpose
TBD - created by archiving change extract-editor-ui-seam. Update Purpose after archive.
## Requirements
### Requirement: Extension UI seam module

The editor SHALL provide a UI-dependent editor module **`EditorUi`** (target `EditorUi`, source
`engine/sandbox/ui/`) that exposes the UI-facing extension seam and the shared UI primitives used by custom
asset views. `EditorUi` SHALL depend only on `Core` and `UI` and SHALL NOT depend on `EditorShell` or
`EditorCore`.

#### Scenario: Module dependencies
- **WHEN** the `EditorUi` target is configured
- **THEN** its direct dependencies are `Core` and `UI` only (no `EditorCore`, no `EditorShell`)

#### Scenario: Seam and primitives are available
- **WHEN** a feature editor extension includes the seam
- **THEN** it resolves `editor/ui/AssetViewProvider.h` (`IAssetViewProvider` + `AssetViewProviderRegistry`)
  and the UI primitives `editor/ui/UiDraw.h` / `editor/ui/UiTheme.h` from `EditorUi`

### Requirement: Editor extensions do not link the shell
Feature editor-extension modules that supply custom asset views SHALL depend on EditorUi and EditorCore as needed, and SHALL be able to link EditorWidgets for reusable editor presentation without linking EditorShell. EditorUi SHALL retain the UI-facing provider seam; reusable skin/form/chrome implementations SHALL be consumed from EditorWidgets rather than Shell.

#### Scenario: Aurora editor extension
- **WHEN** `engine/aurora/editor` (`Aurora.Render.Editor`) is configured
- **THEN** it SHALL link EditorWidgets and the required EditorCore/EditorUi/UI dependencies and SHALL NOT link EditorShell

#### Scenario: Custom view still works
- **WHEN** the aurora material editor provides a custom `UIElement` content widget
- **THEN** it SHALL register through EditorUi's AssetViewProviderRegistry without any EditorShell dependency

#### Scenario: Low-level extension remains small
- **WHEN** an extension only needs the asset-view provider seam and low-level primitives
- **THEN** it SHALL be able to use EditorUi without acquiring an EditorWidgets or EditorCore dependency through EditorUi

### Requirement: EditorCore stays UI-free

`EditorCore` SHALL remain free of `UI`/render dependencies; the UI-facing seam SHALL NOT be placed in
`EditorCore`.

#### Scenario: UI headers not in core
- **WHEN** an `editor/core/*.h` header includes `ui/`
- **THEN** the configure-time guard rejects it

### Requirement: Viewer flag policy is shell-owned

The asset-viewer flag derivation (`AssetViewFlags` + `DeriveAssetViewFlags`) SHALL live in the editor shell,
not in the seam module, because it composes the builder manager (`Framework`) and the view-data registry
(`EditorCore`).

#### Scenario: Seam module free of Framework
- **WHEN** the `EditorUi` target is configured
- **THEN** `DeriveAssetViewFlags` is not part of it and its dependency list does not contain `Framework` or
  `EditorCore`

### Requirement: Editor extensions build in the editor phase

The feature editor-extension modules SHALL be configured after the editor framework (`add_subdirectory(sandbox)`)
and gated by the editor build option.

#### Scenario: Editor disabled
- **WHEN** the editor build option is off
- **THEN** the feature editor-extension modules are not configured (no reference to a framework that is not
  built)

#### Scenario: Editor enabled
- **WHEN** the editor build option is on
- **THEN** the feature editor-extension modules are configured after the editor framework and link `EditorCore`
  / `EditorUi` successfully
