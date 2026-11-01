# editor-assets Specification

## Purpose

Define editor asset discovery, catalog, cooking, viewers and asset previews.

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

### Requirement: Cross-DLL singleton catalog service

The editor SHALL provide an asset catalog service in `EditorCore` that is a process-wide
`Singleton<EditorAssetCatalog>` (its instance stored in the environment), so that a catalog
constructed/registered by one module is the same instance read by shell panels in other DLLs. The
service SHALL be render- and toolkit-independent: its public headers SHALL NOT include `ui/`,
`aurora/`, or any Qt header, and it SHALL operate without a window, GPU, or UI toolkit. The existing
`IEditorAssetCatalog` type lookup SHALL be served by this catalog.

#### Scenario: One instance across modules
- **WHEN** a module obtains the catalog and a shell panel in another module obtains the catalog
- **THEN** both SHALL refer to the same singleton instance

#### Scenario: No function-local static instance
- **WHEN** the catalog's default instance is created
- **THEN** it SHALL be the environment-held singleton, not a function-local static

#### Scenario: Catalog runs headless
- **WHEN** a test constructs the catalog over a mounted source tree without initializing the platform
  or a renderer
- **THEN** it SHALL enumerate folders and assets and return metadata without a window or GPU

#### Scenario: Forbidden header rejected
- **WHEN** a public asset-catalog header includes `ui/`, `aurora/`, or a Qt header
- **THEN** configuration SHALL fail with an explicit error naming the offending header

### Requirement: Virtual-path tree from the asset database

The catalog SHALL derive a folder tree from the registered sources' logical paths (each source's
directory prefixes become folder nodes, its filename a leaf item), grouped by owning mount. It SHALL
NOT read the raw filesystem for structure and SHALL NOT depend on a `SourceAssetBundle` enum.

#### Scenario: List a folder
- **WHEN** the catalog lists the items and subfolders of a logical folder
- **THEN** it SHALL return that folder's registered assets and child folders derived from the source
  paths

#### Scenario: Empty folder absent
- **WHEN** a directory contains no registered assets and no subfolders with registered assets
- **THEN** the catalog SHALL NOT return it as a folder node

#### Scenario: Imported asset appears
- **WHEN** a file is imported (gaining identity) into a folder
- **THEN** the catalog SHALL list it in that folder after invalidation

#### Scenario: Newly created asset appears
- **WHEN** a new asset is created through a creator and registered
- **THEN** the catalog SHALL list it in its folder after invalidation

### Requirement: Registered assets only

The catalog SHALL list only assets that are registered sources with a known derived type. Manifest
and sidecar files (such as the per-directory `assets.jsonl`) and any other non-asset file SHALL NOT
appear as items or folders.

#### Scenario: Manifest file hidden
- **WHEN** a folder contains a registered asset and the per-directory manifest file
- **THEN** the catalog SHALL list the asset and SHALL NOT list the manifest file

#### Scenario: Unknown extension hidden
- **WHEN** a registered path has no known derived type
- **THEN** it SHALL NOT appear as an item

### Requirement: Asset item metadata

Each registered asset item SHALL expose its UUID, display name, logical path, and derived
`AssetTypeId`. The display name SHALL be the manifest name when present and the file name otherwise.
The type SHALL be derived from the source (builder registry), never from a persisted category.

#### Scenario: Item identity
- **WHEN** the catalog returns a registered asset item
- **THEN** the item SHALL carry its UUID, name, logical path, and derived type

#### Scenario: Type is derived
- **WHEN** an asset of a known extension is listed
- **THEN** its reported type SHALL equal the builder registry's type for that extension

### Requirement: Type, name, and lookup queries

The catalog SHALL provide lookups by UUID and by logical path, and SHALL list assets filtered by
type. The existing asset-typed property field lookup SHALL be preserved as a query over this service.

#### Scenario: Lookup by UUID
- **WHEN** the catalog is queried with a known UUID
- **THEN** it SHALL return the matching item

#### Scenario: Filter by type
- **WHEN** the catalog lists assets filtered to a type
- **THEN** it SHALL return only assets whose derived type matches

#### Scenario: Property field lookup preserved
- **WHEN** an asset-typed property field requests the assignable assets for its type
- **THEN** the result SHALL come from the same catalog service

### Requirement: Cook and product state

The catalog SHALL report, per asset UUID, its cook/product state (at least: not-cooked, cooking,
ready, failed) and the error text when failed. State transitions SHALL be driven by the asset
build-finished event and cook triggers, not by polling the filesystem.

#### Scenario: State before cook
- **WHEN** an asset has no product for the current target
- **THEN** the catalog SHALL report it as not-cooked

#### Scenario: State on completion
- **WHEN** the asset build-finished event reports success for an asset
- **THEN** the catalog SHALL report that asset as ready

#### Scenario: State on failure
- **WHEN** the asset build-finished event reports failure for an asset
- **THEN** the catalog SHALL report that asset as failed with the reported error text

### Requirement: Effective cook configuration inspection

The catalog SHALL expose the effective cook configuration for an asset, computed from the asset-level
override, the project-level preset, and the current platform, and SHALL expose the asset's declared
cook targets.

#### Scenario: Effective settings
- **WHEN** the catalog is asked for an asset's effective cook configuration
- **THEN** it SHALL return the merged asset × project × platform settings

#### Scenario: Declared targets
- **WHEN** the catalog is asked for an asset's targets
- **THEN** it SHALL return the targets the asset declares (or the default target when unconfigured)

### Requirement: Dependency queries

The catalog SHALL answer forward (`Dependencies`) and reverse (`Dependents`) dependency queries for an
asset through the framework dependency provider.

#### Scenario: Forward dependencies
- **WHEN** the catalog is asked for an asset's dependencies
- **THEN** it SHALL return the assets that asset references

#### Scenario: Reverse dependents
- **WHEN** the catalog is asked for an asset's dependents
- **THEN** it SHALL return the assets that reference it

### Requirement: Mount writability

The catalog SHALL report whether an asset's owning mount is writable, so that consumers can disable
mutating actions on read-only (engine) mounts.

#### Scenario: Workspace asset writable
- **WHEN** an asset lives in the writable workspace mount
- **THEN** the catalog SHALL report its mount as writable

#### Scenario: Engine asset read-only
- **WHEN** an asset lives in the read-only engine mount
- **THEN** the catalog SHALL report its mount as read-only

### Requirement: Safe snapshots

The catalog SHALL build folder/item snapshots by copying the registered sources under the asset
database's lock (through a read-only enumeration accessor) and SHALL NOT read the unfiltered source
map without synchronization.

#### Scenario: Concurrent build mutation
- **WHEN** a snapshot is taken while a cook/build mutates the source map
- **THEN** the snapshot SHALL be consistent (no torn/unsynchronized read)

#### Scenario: Enumeration accessor
- **WHEN** the catalog builds its tree
- **THEN** it SHALL obtain the sources through the database's lock-safe read-only accessor

### Requirement: Invalidation and change notification

The catalog SHALL invalidate affected entries on mutation performed through the framework APIs
(import, move, duplicate, delete), on asset build-finished events, and on an explicit refresh, and
SHALL notify registered observers so views can update. Entries SHALL be ordered deterministically.

#### Scenario: Notify on mutation
- **WHEN** an asset is imported, moved, duplicated, or deleted through the catalog
- **THEN** observers SHALL be notified and the affected folders SHALL refresh

#### Scenario: Explicit refresh
- **WHEN** a caller requests a refresh
- **THEN** the catalog SHALL re-read the mounted source metadata and notify observers

#### Scenario: Deterministic order
- **WHEN** a folder's items and subfolders are listed
- **THEN** they SHALL be returned in a stable, name-sorted order

#### Scenario: No polling
- **WHEN** the catalog has been built and no mutation, build event, or refresh occurs
- **THEN** it SHALL NOT read the filesystem on a timer

### Requirement: Per-asset cook override read and write

Building on the catalog's effective cook configuration inspection, `EditorAssetCatalog` SHALL expose, for an
asset and target, the **effective** reflected settings object plus the global-preset baseline object and the
builder-declared settings type (`GetCookSettings`), and SHALL persist an edited reflected object by
computing the sparse override against the bundle preset and writing the manifest `cook.settings[target]`
block (`ApplyCookSettings`). Persistence SHALL occur only for assets on a writable mount, SHALL be a no-op
otherwise, and SHALL notify observers. The catalog SHALL NOT auto-trigger a cook on mutation.

#### Scenario: Provide effective settings plus baseline

- **WHEN** the catalog is asked for an asset's cook settings for a target
- **THEN** it returns a reflected effective object, a reflected preset baseline, and the settings type

#### Scenario: Persist an edit as a sparse override

- **WHEN** an edited reflected object that differs from the preset only in `maxSize` is applied
- **THEN** the manifest stores a sparse `maxSize` override for that target and observers are notified

#### Scenario: Reset removes the override

- **WHEN** an object equal to the preset is applied
- **THEN** the target's override key is removed (empty override clears the target block)

#### Scenario: Read-only asset not editable

- **WHEN** cook settings are applied to an asset on a read-only mount
- **THEN** the manifest is unchanged

### Requirement: Editor cook requests are deferred and deduplicated

Editor-initiated cooking SHALL NOT run inside the pointer/key/text event dispatch. Triggers SHALL
only enqueue a request; the queue SHALL be drained at a safe point in the frame (the editor tick, on
the main thread). Repeated requests for the same asset SHALL be coalesced into a single build.

#### Scenario: Trigger from an event returns immediately
- **WHEN** the user Saves / Closes an asset or picks `Assets > Cook`, inside the event handler
- **THEN** the request SHALL only be enqueued and the handler SHALL return without running any cook

#### Scenario: Drain once per frame
- **WHEN** the editor ticks
- **THEN** the pending cook requests SHALL be submitted on the main thread, once per frame

#### Scenario: Duplicate requests coalesce
- **WHEN** the same asset is requested more than once before the next drain
- **THEN** exactly one build SHALL be submitted

### Requirement: Cooking never blocks the UI thread

The editor SHALL NOT block the main thread waiting for a cook. In-process cook builds SHALL run on a
dedicated cook pool, and the UI SHALL surface cook state without a blocking wait and refresh
locally on completion.

#### Scenario: No blocking wait
- **WHEN** a cook is in flight
- **THEN** the main thread SHALL continue ticking and drawing (no `WaitForAll` on the UI thread)

#### Scenario: Local refresh on completion
- **WHEN** a cook completes
- **THEN** the editor SHALL clear the cook state and refresh locally, without a full shell rebuild

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

### Requirement: Asset preview sessions

EditorCore SHALL extend its consumer-owned asset preview seam from a boolean query to explicit session creation, update/resize and close/status operations, with owned session identity and generation. Registry state SHALL be cross-DLL shared and provider registrations SHALL be revoked only after sessions drain. The shell SHALL host preview outputs through the existing shared renderer; animation semantics SHALL remain in Animation.Editor. Unsupported asset types or absent providers SHALL retain the placeholder. Sessions SHALL NOT own a second Device or producer.

#### Scenario: No provider draws a placeholder
- **WHEN** no provider supports an asset type
- **THEN** the preview region SHALL show its placeholder without claiming a live preview

#### Scenario: Multiple independent sessions
- **WHEN** the same asset is opened in two preview windows
- **THEN** each SHALL have a distinct session/time/output and closing one SHALL NOT destroy the other

#### Scenario: Provider unload with active output
- **WHEN** an extension is unloaded while a preview output is in flight
- **THEN** it SHALL stop session updates, drain CPU work and output retirement, and only then release provider code

### Requirement: Resizable viewer

The asset viewer SHALL be resizable via a bottom-right grip (drag), clamped to a minimum size and the window
bounds; the panel size SHALL persist across open/close within the session.

#### Scenario: Drag the grip resizes

- **WHEN** the user drags the bottom-right grip
- **THEN** the panel grows/shrinks within the clamp bounds and its layout follows

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

The asset-open handler SHALL NOT open a viewer for a type whose resolved AssetViewFlags is HIDDEN. AuroraSkin SHALL have a registered read-only reflected DATA summary in the Aurora editor extension, showing its owner source, slot/mapping mode and format/revision. Generated Skin editing SHALL occur by owner reimport, not by writing cooked matrices directly.

#### Scenario: Unregistered internal type is not opened
- **WHEN** a type has no settings or data provider and resolves to HIDDEN
- **THEN** the viewer SHALL not open

#### Scenario: Skin opens a reflected summary
- **WHEN** the user opens a generated Skin with its Aurora DATA provider registered
- **THEN** the viewer SHALL show its read-only summary and owner-source identity

### Requirement: Asset viewer notifies incremental composition changes

The viewer SHALL synchronize its owned reflected forms before calculating stacked content extent, scrollbar reservation, scroll clamp, and final child rectangles. Its audited owned composition SHALL support incremental arrangement with notified open/reload/rebuilt-content, panel-size, and scroll changes. Reopening at unchanged outer bounds SHALL still allocate the new content before paint. Unknown provider content SHALL retain its own runtime volatility policy and SHALL NOT be forced stable by the viewer. Custom content SHALL continue to be measured and arranged at its actual content-column width.

#### Scenario: New forms have correct first-frame extent
- **WHEN** an asset opens with reflected target/data forms and the viewer arranges before its first Paint
- **THEN** its content total, scrollbar, and allocated form heights SHALL use the current form extents

#### Scenario: Viewer repeats an unchanged controlled frame
- **WHEN** an open controlled viewer completes layout and no input or volatile descendant changes
- **THEN** its next same-rectangle incremental allocation SHALL be eligible to skip completed arrangement

#### Scenario: Same-bounds reopen or reload replaces content
- **WHEN** the viewer reopens or reloads content while its outer bounds stay unchanged
- **THEN** the notified layout work SHALL update its forms, extent, scroll clamp, and child allocations before paint

#### Scenario: Standalone caller forces arrangement after a notification
- **WHEN** an existing standalone caller uses forced Arrange followed by Paint after an owned form's dependency change
- **THEN** the executing viewer arrangement SHALL synchronize its forms before computing their content extent without requiring a preliminary Paint

#### Scenario: Resizing or scrolling updates geometry
- **WHEN** panel size or outer content scroll changes
- **THEN** the next incremental allocation SHALL update content widths/origins and scroll clamping while preserving reusable measurements for a scroll-only change where valid

#### Scenario: Provider remains externally dynamic
- **WHEN** custom provider content remains default-volatile and changes its desired extent externally
- **THEN** the viewer's incremental allocation SHALL continue to observe the provider's current extent and arrange it at the actual reserved content width

#### Scenario: Theme changes copied chrome metrics
- **WHEN** the default theme changes toolbar or menu bar height after the viewer has been constructed
- **THEN** viewer synchronization SHALL update those owned chrome inputs before allocating the toolbar and body rather than retaining constructor-copied heights

### Requirement: Viewer model storage outlives interaction cleanup

AssetViewerWidget SHALL cancel live reflected form interactions and detach bindings before replacing owned settings storage. Closing or opening another asset SHALL cancel outstanding live previews while their model remains valid. Viewer destruction SHALL release child form interactions/elements before its owned settings and registered view-data models are destroyed. This contract SHALL NOT imply that existing command-history targets remain valid after replacement.

#### Scenario: Reload replaces heap-backed cook settings
- **WHEN** Reload replaces a settings Any while its form has an active preview
- **THEN** cancellation and detachment SHALL complete before the old settings storage is released

#### Scenario: Close cancels a live preview
- **WHEN** the viewer closes with an active preview
- **THEN** the model SHALL be restored and the form SHALL retain no pending preview rollback

#### Scenario: Viewer destruction with an active preview
- **WHEN** a viewer owning heap-backed reflected data is destroyed during a preview
- **THEN** child forms SHALL clean up while their model/services remain alive and SHALL NOT access already-destroyed model members

### Requirement: Asset viewer owns an editing-session history

AssetViewerWidget SHALL own and inject a history for its reflected editing session. Opening, reopening, switching asset, closing, and catalog replacement SHALL clear the session history without reverting persisted commits. Viewer scoped actions SHALL traverse and report enabled state from that history rather than global editor history. The history and borrowed catalog service SHALL outlive all forms/commands that use them.

#### Scenario: Viewer edit leaves global history unchanged
- **WHEN** a reflected cook-settings form commits an asset edit
- **THEN** viewer history SHALL gain the command and global editor history SHALL remain unchanged

#### Scenario: Close and asset switch end history ownership
- **WHEN** the viewer closes or opens another asset
- **THEN** old commands SHALL be released and later viewer Undo SHALL NOT edit the previous asset

### Requirement: Cook-setting history restores persisted target state

Cook-setting commands SHALL own asset UUID, target identity, reflected schema identity, and before/after target sparse settings. They SHALL NOT retain viewer/field/settings storage targets. Do, Undo, and Redo SHALL use one current-identity persistence path that updates the manifest and catalog notifications while preserving other targets and unrelated cook-block data. Normal settings reload SHALL preserve valid session history. Save/Cook after Undo SHALL observe the restored persisted state.

#### Scenario: Undo after heap-backed settings reload
- **WHEN** a committed edit caused the previous heap-backed settings Any to be destroyed and the viewer invokes Undo
- **THEN** Undo SHALL restore persisted target settings and refreshed form values without accessing that Any

#### Scenario: Another target changes between edit and undo
- **WHEN** another target receives an independent persisted update before Undo of the original target
- **THEN** Undo SHALL restore the original target and preserve the other target's update

#### Scenario: External traversal uses the same persistence
- **WHEN** a caller traverses the injected viewer command service directly
- **THEN** persistence SHALL update synchronously and the viewer SHALL reload before its next layout

### Requirement: Stale asset history cannot write through obsolete targets

Command construction SHALL reject missing/read-only assets, missing properties/targets, incompatible values, and replaced schemas. Execution SHALL re-resolve current asset/target/schema identity. A missing asset, removed target, or replaced schema SHALL make the stale operation inert without accessing old model memory. The generic history cursor MAY advance for an inert command; the viewer SHALL clear its history when reload observes invalid targets/models/schema.

#### Scenario: Asset disappears with populated history
- **WHEN** the edited asset is removed before Undo or the next viewer synchronization
- **THEN** no stale model or missing source SHALL be written and viewer reload SHALL clear obsolete history

#### Scenario: Settings schema or target changes
- **WHEN** a command is traversed after its target is removed or reflected settings type identity changes
- **THEN** the command SHALL leave persistence unchanged and SHALL NOT use retained schema/model pointers

### Requirement: Viewer lifecycle work waits for a committing widget event

AssetViewerWidget SHALL defer Open, Close, catalog replacement and model/tree reload requested synchronously while any owned reflected form is editing. Nested synchronization SHALL NOT destroy or replace that form. Pending work SHALL execute at the next safe host layout boundary after the widget event returns; it SHALL NOT execute from edit-scope cleanup. A replacement session SHALL contain no command from a cleared previous session.

#### Scenario: Observer switches assets during inline commit
- **WHEN** a catalog observer requests Open for another asset and synchronizes while a widget is committing
- **THEN** the old widget/form SHALL remain alive through event return and the next layout SHALL open a fresh session for the requested asset

#### Scenario: Observer replaces targets or closes during commit
- **WHEN** a catalog observer changes the visible target set or requests Close and synchronizes during a widget commit
- **THEN** target-form destruction or close SHALL wait until the post-event layout boundary and invalidated history SHALL be cleared safely

#### Scenario: Observer replaces the session during Undo
- **WHEN** a synchronous undo observer opens another asset and clears the previous session
- **THEN** the completed old operation SHALL NOT populate Undo or Redo in the new session

### Requirement: Viewport owns its presentation

A viewport SHALL maintain its own `presentation`, choosing between `TEXTURE` (render to an offscreen content
target composited by the editor UI) and `WINDOW` (render to its own native window + swapchain). The content target
SHALL be independent of the presentation and SHALL be preserved across a presentation change.

#### Scenario: Texture presentation
- **WHEN** a viewport uses `TEXTURE` presentation
- **THEN** its content target SHALL be composited as a `sky::ui` image element in the editor frame, with no
  separate OS window

#### Scenario: Window presentation
- **WHEN** a viewport uses `WINDOW` presentation
- **THEN** a `NativeWindow` and a `ClientViewport`/swapchain SHALL be created for it and its content target SHALL
  be presented there

#### Scenario: Switch presentation at runtime
- **WHEN** a viewport switches presentation at runtime
- **THEN** the window + swapchain SHALL be created or destroyed while the same content target continues to be used

### Requirement: Multiple viewports

The editor SHALL support multiple viewports (the main viewport plus previews) at once, each with its own content
target and presentation.

#### Scenario: Several previews
- **WHEN** the editor creates more than one preview viewport
- **THEN** each SHALL have its own content target and independent presentation state

#### Scenario: Destroy a viewport
- **WHEN** a preview viewport is closed
- **THEN** its content target and any window/swapchain SHALL be released without affecting other viewports

#### Scenario: Per-viewport resize
- **WHEN** a `WINDOW` viewport is resized
- **THEN** only its own swapchain SHALL be rebuilt

### Requirement: Single device for all viewports

All viewport content targets and swapchains SHALL use the one Aurora device; the editor SHALL NOT create a second
device for viewports.

#### Scenario: No extra device
- **WHEN** any number of viewports (TEXTURE or WINDOW) is created
- **THEN** they SHALL all use the existing Aurora device

### Requirement: Viewport content until the scene renderer

Until the engine scene renderer is available, a viewport SHALL be able to present placeholder content; the
presentation mechanism SHALL work regardless of the content source.

#### Scenario: Placeholder content
- **WHEN** the engine scene renderer is not yet available
- **THEN** a viewport SHALL still present placeholder content (for example a clear color / test pattern)
