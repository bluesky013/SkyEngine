## MODIFIED Requirements

### Requirement: Per-asset-type view customization

The **UI seam module** (`EditorUi`, `editor/ui/AssetViewProvider.h`) SHALL expose an `AssetViewProviderRegistry` (cross-DLL `Singleton`) mapping an asset **type** to an `IAssetViewProvider` that may supply a custom content widget (right region); the reserved preview region remains served by the separate `IAssetPreviewProvider` seam. Per-asset-type views SHALL be registered by the **owning module** (e.g. `engine/aurora/editor` for aurora assets, `engine/content/editor` for content assets) by linking `EditorUi`, not centralized in the shell. The viewer (shell) SHALL use a provider's content for the `DATA` section; the `BUNDLE` section always uses the reflected cook-settings forms. The viewer's `AssetViewFlags` derivation SHALL be shell-owned (see `editor-extension-ui`).

#### Scenario: Provider supplies custom content

- **WHEN** a provider is registered for the asset's type and returns a content widget
- **THEN** the viewer shows that widget in the `DATA` section of the right region

#### Scenario: No provider uses defaults

- **WHEN** no provider is registered for the asset's type and its flags include `BUNDLE`
- **THEN** the viewer shows only the reflected cook-settings form(s) and the reserved preview placeholder

#### Scenario: Registration does not pull the shell

- **WHEN** an owning editor extension registers an `IAssetViewProvider`
- **THEN** it links `EditorUi` (not `EditorShell`)
