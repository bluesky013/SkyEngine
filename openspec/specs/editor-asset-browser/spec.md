# editor-asset-browser Specification

## Purpose
TBD - created by archiving change editor-asset-browser. Update Purpose after archive.
## Requirements
### Requirement: Asset browser panel

The editor shell SHALL provide an asset browser panel registered as a built-in panel view (id
`assets`) that presents the source asset tree and its assets. The panel SHALL render through the
in-house `sky::ui` toolkit and SHALL read from the headless asset catalog service, holding no asset
metadata of its own.

#### Scenario: Panel available
- **WHEN** the editor runs with the sandbox built-in panels
- **THEN** the layout SHALL offer an `assets` panel that can be docked, floated, and tabbed like other
  panels

#### Scenario: Panel reads the catalog
- **WHEN** the panel displays a folder
- **THEN** its contents SHALL come from the asset catalog service

### Requirement: Asset item presentation

Each item SHALL render a thumbnail obtained from an optional asset-thumbnail provider keyed by type
or UUID, falling back to a type-derived icon when no provider is registered or it has no image yet.
The panel SHALL NOT load products or touch the renderer to obtain a thumbnail.

#### Scenario: Icon fallback
- **WHEN** no thumbnail provider is registered
- **THEN** each item SHALL render a type-derived icon

#### Scenario: Provider thumbnail
- **WHEN** a thumbnail provider returns an image for an asset
- **THEN** the item SHALL render that thumbnail instead of the icon

#### Scenario: Manifest and sidecar files absent
- **WHEN** a folder is listed
- **THEN** manifest/sidecar files SHALL NOT be shown as items

### Requirement: Two-pane folder tree and item view

The panel SHALL show a folder tree over the mount namespace beside an item list for the selected
folder, with a breadcrumb showing the current logical path. The item view SHALL be a list; a grid
view is not required.

#### Scenario: Select a folder
- **WHEN** a folder is selected in the tree
- **THEN** the item view SHALL show that folder's assets and subfolders and the breadcrumb SHALL show
  its logical path

#### Scenario: Navigate via breadcrumb
- **WHEN** an ancestor in the breadcrumb is activated
- **THEN** the item view SHALL navigate to that ancestor folder

### Requirement: Search, type filter, and sort

The panel SHALL provide a text search, a type filter, and a sort order that apply to the item view.
The type filter SHALL use the extension/asset-type filter model.

#### Scenario: Search filters items
- **WHEN** a search string is entered
- **THEN** the item view SHALL show only assets whose name matches

#### Scenario: Type filter
- **WHEN** a type filter is selected
- **THEN** the item view SHALL show only assets of the derived matching type

### Requirement: Multi-selection and selection service

The panel SHALL support single- and multi-selection of assets and SHALL publish the selection to the
`SelectionService` as asset selections keyed by UUID.

#### Scenario: Select an asset
- **WHEN** an asset is selected
- **THEN** the `SelectionService` SHALL report an asset selection with that asset's UUID

#### Scenario: Multi-select
- **WHEN** several assets are selected
- **THEN** the `SelectionService` SHALL report all of them

### Requirement: Context actions

The panel SHALL offer, through the editor action registry, the actions New Asset, Import, Rename,
Move, Duplicate, Delete, Cook/Build, Reimport, Copy Reference, Show in Explorer, Find References, and
Refresh. Mutating actions SHALL be enabled only for assets whose owning mount is writable.

#### Scenario: Context menu from actions
- **WHEN** the context menu is opened on the current selection
- **THEN** it SHALL present the applicable registered asset actions

#### Scenario: Mutating action disabled on read-only mount
- **WHEN** the selection contains only read-only (engine) assets
- **THEN** Import, Rename, Move, Duplicate, and Delete SHALL be disabled

#### Scenario: New asset in a folder
- **WHEN** a New Asset action is invoked with a creator in a writable folder
- **THEN** the asset SHALL be created and the folder SHALL refresh

### Requirement: Asset mutation preserves references

Import, Rename, Move, Duplicate, and Delete SHALL be performed through the framework source-asset
mutation APIs, preserving the UUID on move/rename and assigning a new UUID on duplicate, without
modifying existing references.

#### Scenario: Rename keeps identity
- **WHEN** an asset is renamed or moved
- **THEN** its UUID SHALL be unchanged and referenced assets SHALL still resolve

#### Scenario: Duplicate gets a new identity
- **WHEN** an asset is duplicated
- **THEN** the copy SHALL receive a new UUID while the original is unchanged

#### Scenario: Delete removes identity
- **WHEN** an asset is deleted
- **THEN** its file, manifest entry, and identity SHALL be removed and the folder SHALL refresh

### Requirement: Import

The panel SHALL provide an Import action that copies external files into the selected writable folder
through the framework import API (identity assigned, optionally cooked per configuration), and SHALL
refuse import into a read-only folder.

#### Scenario: Import files
- **WHEN** the Import action is invoked with files and a writable destination folder
- **THEN** each file SHALL be imported and the folder SHALL refresh

#### Scenario: Import onto read-only folder
- **WHEN** import is requested for a read-only engine folder
- **THEN** the import SHALL be refused

### Requirement: Open an asset

Double-clicking an asset SHALL open it through the registered asset-editor for its type. Opening
SHALL load the asset's data through the product-based loader (on-demand cook when the product is
missing), and SHALL NOT read source files. When no editor is registered the double-click SHALL be a
no-op with no error.

#### Scenario: Double-click with a registered editor
- **WHEN** an asset with a registered editor is double-clicked
- **THEN** its editor SHALL open using the product-based load path

#### Scenario: Double-click without an editor
- **WHEN** an asset with no registered editor is double-clicked
- **THEN** nothing SHALL open and no error SHALL be reported

#### Scenario: Missing product triggers cook
- **WHEN** an opened asset's product is missing but its source exists
- **THEN** the load SHALL enter LOADING, schedule a cook, and finish LOADED on success (never a
  source fallback)

### Requirement: Cook status display

The panel SHALL display each asset's cook/product state (not-cooked, cooking, ready, failed), update
it from cook triggers and the asset build-finished event, and surface the error for a failed asset.

#### Scenario: Cooking indicator
- **WHEN** a cook is triggered for an asset
- **THEN** the panel SHALL show that asset as cooking until completion

#### Scenario: Ready indicator
- **WHEN** the build-finished event reports success
- **THEN** the panel SHALL show that asset as ready

#### Scenario: Failed indicator
- **WHEN** the build-finished event reports failure
- **THEN** the panel SHALL show that asset as failed and make its error text available

### Requirement: Detail pane

The panel SHALL show a detail view for the selected asset with its metadata, the effective cook
configuration, and its dependencies/references.

#### Scenario: Show effective cook config
- **WHEN** an asset is selected
- **THEN** the detail view SHALL show its effective cook configuration and declared targets

#### Scenario: Show dependencies and dependents
- **WHEN** an asset is selected
- **THEN** the detail view SHALL show its dependencies and its dependents

### Requirement: Refresh

The panel SHALL refresh affected folders when the catalog notifies a change and SHALL provide an
explicit Refresh action.

#### Scenario: Refresh after change
- **WHEN** the catalog reports a change in the displayed folder
- **THEN** the panel SHALL update the folder's contents

#### Scenario: Manual refresh
- **WHEN** the Refresh action is invoked
- **THEN** the catalog SHALL re-read the mounted source metadata and the panel SHALL update

