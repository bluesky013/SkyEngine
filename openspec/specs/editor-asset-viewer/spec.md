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

The asset open path (`SandboxModule`'s asset-open handler) SHALL NOT open the asset viewer for a type whose `AssetViewFlags` is `HIDDEN`.

#### Scenario: Skin is not opened

- **WHEN** the user opens a skin asset
- **THEN** the asset viewer does not open

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
