## ADDED Requirements

### Requirement: Asset viewer widget

The editor SHALL provide a reusable **asset viewer widget** that opens for a selected asset and shows its
name, type and path; a **reserved preview region**; and the asset's **cook settings** rendered by the generic
reflected form (`ReflectedFormView` bound to `EditorAssetCatalog::GetCookSettings(uuid, target)` with the
preset baseline). When an asset resolves to multiple cook **targets**, the widget SHALL show **one reflected
form per target simultaneously** (a target label over each), scrollable when they exceed the region, so each
target can be configured independently. Editing SHALL be committed per target through the catalog. The widget
SHALL be dismissable (close button and Escape).

#### Scenario: Open shows header, preview region, and per-target settings

- **WHEN** the viewer is opened for an asset that resolves to several targets
- **THEN** it shows the asset header, a preview region, and one bound reflected form per target (each with a target label)

#### Scenario: Edit one target independently

- **WHEN** the user edits the `maxSize` of one target's form
- **THEN** only that target's sparse override is persisted; other targets are unchanged

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

The shell SHALL expose an `AssetViewProviderRegistry` (cross-DLL `Singleton`) mapping an asset **type** to an
`IAssetViewProvider` that may supply a custom content widget (right region) and/or preview widget (left
region). When a provider supplies one, the viewer SHALL use it in place of the default reflected
cook-settings form / reserved preview placeholder.

#### Scenario: Provider supplies custom content

- **WHEN** a provider is registered for the asset's type and returns a content widget
- **THEN** the viewer shows that widget in the right region instead of the default reflected form

#### Scenario: No provider uses defaults

- **WHEN** no provider is registered for the asset's type
- **THEN** the viewer shows the reflected cook-settings form and the reserved preview placeholder

