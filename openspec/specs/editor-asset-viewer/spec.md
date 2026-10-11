# editor-asset-viewer Specification

## Purpose
TBD - created by archiving change asset-viewer-widget. Update Purpose after archive.
## Requirements
### Requirement: Asset viewer widget

The editor SHALL provide a reusable **asset viewer widget** that opens for a selected asset and shows its name, type and path; a **reserved preview region**; and a **content region** whose contents follow the asset's `AssetViewFlags` (see the view-flags requirement): the `BUNDLE` part renders **one reflected cook-settings form per cook target** (`ReflectedFormView` bound to `EditorAssetCatalog::GetCookSettings(uuid, target)` with the preset baseline, a target label over each), and the `DATA` part renders the provider's asset view. When both flags are set the widget SHALL render **both sections together** (scrollable). Editing SHALL be committed through the catalog (BUNDLE) / the view's write-back (DATA). The widget SHALL be dismissable (close button and Escape).

#### Scenario: Open shows header, preview region, and per-target settings

- **WHEN** the viewer is opened for an asset whose flags include `BUNDLE` and that resolves to several targets
- **THEN** it shows the asset header, a preview region, and one bound reflected form per target (each with a target label)

#### Scenario: Edit one target independently

- **WHEN** the user edits the `maxSize` of one target's form
- **THEN** only that target's sparse override is persisted; other targets are unchanged

#### Scenario: BUNDLE and DATA together

- **WHEN** the viewer is opened for an asset whose flags are `BUNDLE | DATA`
- **THEN** it renders the DATA view section and the per-target cook-settings forms together

#### Scenario: Escape closes

- **WHEN** the viewer is open and Escape is pressed
- **THEN** it closes without applying further edits

### Requirement: Reserved preview seam

The editor core SHALL expose an `IAssetPreviewProvider` seam (uuid + asset type) with a cross-DLL
`AssetPreviewProviderRegistry`, mirroring the thumbnail seam. The viewer SHALL query the provider for the
preview region and, when no provider is registered (or it has nothing), draw a placeholder. No preview
rendering SHALL be implemented by this change.

#### Scenario: No provider draws a placeholder

- **WHEN** the viewer wants the preview and no provider is registered
- **THEN** it draws an empty placeholder region (no error)

#### Scenario: Provider consulted

- **WHEN** a provider is registered and the viewer wants the preview
- **THEN** the provider is consulted with the asset uuid and type

### Requirement: Resizable viewer

The asset viewer SHALL be resizable via a bottom-right grip (drag), clamped to a minimum size and the window
bounds; the panel size SHALL persist across open/close within the session.

#### Scenario: Drag the grip resizes

- **WHEN** the user drags the bottom-right grip
- **THEN** the panel grows/shrinks within the clamp bounds and its layout follows

### Requirement: Per-asset-type view customization

The shell SHALL expose an `AssetViewProviderRegistry` (cross-DLL `Singleton`) mapping an asset **type** to an `IAssetViewProvider` that may supply a custom content widget (right region) and/or preview widget (left region). The registry SHALL also expose the asset's `AssetViewFlags` (see the view-flags requirement). Per-asset-type views SHALL be registered by the **owning module** (e.g. `engine/aurora/editor` for aurora assets, `engine/content/editor` for content assets), not centralised in the shell. The viewer SHALL use a provider's content for the `DATA` section; the `BUNDLE` section always uses the reflected cook-settings forms.

#### Scenario: Provider supplies custom content

- **WHEN** a provider is registered for the asset's type and returns a content widget
- **THEN** the viewer shows that widget in the `DATA` section of the right region

#### Scenario: No provider uses defaults

- **WHEN** no provider is registered for the asset's type and its flags include `BUNDLE`
- **THEN** the viewer shows only the reflected cook-settings form(s) and the reserved preview placeholder

### Requirement: Asset view flags derivation

The editor SHALL derive, per asset type, an `AssetViewFlags` bitmask:

- `AssetViewFlags::BUNDLE` SHALL be set when the asset's builder exposes a settings type (`AssetBuilderManager::GetBuilderSettingsType(type)` non-null; the asset has a special cook config).
- `AssetViewFlags::DATA` SHALL be set when the type has registered an asset-view (a reflected view struct and/or a custom content provider).
- When neither is set the flags SHALL be `AssetViewFlags::HIDDEN`.

`AssetViewFlags` SHALL support combination (e.g. `BUNDLE | DATA`). Types with no registration SHALL default to `BUNDLE`.

#### Scenario: Bundled asset

- **WHEN** the flags are derived for a texture (builder exposes settings; no asset-view registered)
- **THEN** the flags are `BUNDLE`

#### Scenario: Data asset

- **WHEN** the flags are derived for a material (no builder settings; asset-view registered)
- **THEN** the flags include `DATA`

#### Scenario: Internal asset

- **WHEN** the flags are derived for a skin (no builder settings; no asset-view registered)
- **THEN** the flags are `HIDDEN`

### Requirement: DATA view is reflection-driven and edits the source

The `DATA` section SHALL be a reflection-driven form (`ReflectedFormView`) bound to the type's registered **view data**; fields registered with editable metadata SHALL be editable (via `CommandService`), read-only metadata SHALL render read-only. Committing an edit SHALL **write back the source file and trigger a re-cook** (this change's MVP MAY leave fields read-only until the source write-back for that type exists). Types whose view data is a heterogeneous typed list (material / material-instance properties) SHALL use a **custom provider** instead of the generic form.

#### Scenario: Reflected view binds

- **WHEN** the viewer opens an asset whose flags include `DATA` and that has a registered view struct
- **THEN** the `DATA` section binds a reflected form to that view struct with the registered edit metadata

#### Scenario: Custom provider for material

- **WHEN** the viewer opens a material / material-instance
- **THEN** the `DATA` section uses the material provider (typed property list), not the generic reflected form

### Requirement: Hidden assets do not open the asset viewer

The asset open path (`SandboxModule`'s asset-open handler) SHALL NOT open the asset viewer for a type whose `AssetViewFlags` is `HIDDEN`.

#### Scenario: Skin is not opened

- **WHEN** the user opens a skin asset
- **THEN** the asset viewer does not open

