# editor-shell Specification

## Purpose

Define shared editor chrome, layout, widgets, preferences, resources and extension presentation.

## Scope notes

### editor-layout-dependencies

Refresh editor layout when theme, text, models or reflected forms change, using explicit dependency revisions and stable ownership.

### editor-preference-persistence

Provide portable checked Preferences capture, validation, storage and publication with explicit conflict, failure and partial-effect results.

### editor-widgets

Provide shared editor chrome and reflected controls behind the editor framework, with consistent actions, appearance and data-view separation.

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

### Requirement: Reusable file browser dialog

The editor SHALL provide a reusable, engine-drawn modal file browser dialog supporting three modes:
OpenFile, OpenProject, and SelectDirectory. The caller SHALL supply a title, an initial directory, filters,
and (for directory mode) a default name, and SHALL receive the chosen path on accept.

#### Scenario: Open file
- **WHEN** the dialog is opened in OpenFile mode and the user selects an existing file and confirms
- **THEN** the dialog SHALL report the selected file path to the caller and close

#### Scenario: Open project
- **WHEN** the dialog is opened in OpenProject mode
- **THEN** the listing SHALL be filtered to `*.skyproj` and confirming SHALL return the chosen project file

#### Scenario: Select directory
- **WHEN** the dialog is opened in SelectDirectory mode and the user navigates to a directory and confirms
- **THEN** the dialog SHALL report the current directory combined with the entered name

#### Scenario: Cancel
- **WHEN** the user presses Cancel or Esc
- **THEN** the dialog SHALL close and report no selection

### Requirement: Directory navigation and listing

The dialog SHALL display the entries of the current directory and allow navigation into subdirectories and
to a parent directory; it SHALL be UI-toolkit-free in its model and testable headlessly.

#### Scenario: Navigate
- **WHEN** the user activates a subdirectory entry
- **THEN** the listing SHALL show that directory's entries and the location SHALL update

#### Scenario: Parent
- **WHEN** the user activates the parent entry
- **THEN** the listing SHALL show the parent directory's entries

### Requirement: Rename and context menu

The browser SHALL provide a context menu on list entries. In write mode the menu SHALL offer New Folder; with an
entry targeted it SHALL offer Rename. Renaming SHALL edit the entry's name through the name field, commit on
Enter/Open (or blur), and cancel on Esc, reselecting the renamed entry. Creating a folder SHALL enter rename mode
on the new folder so its name can be changed immediately.

#### Scenario: Rename via context menu
- **WHEN** the user right-clicks an entry and chooses Rename
- **THEN** the name field SHALL be prefilled with the entry name and selected for editing

#### Scenario: Commit rename
- **WHEN** the user confirms the edited name
- **THEN** the entry SHALL be renamed in the current location and reselected

#### Scenario: Cancel rename
- **WHEN** the user presses Esc during rename
- **THEN** the original name SHALL be kept

#### Scenario: Create then rename
- **WHEN** New Folder is activated
- **THEN** a folder SHALL be created and the name field SHALL enter rename mode for it

### Requirement: Create folder

The browser SHALL provide a New Folder action that creates a directory in the current location (using a
unique name when none is given), refreshes the listing, selects it, and puts its name in the name field.
Sources that are read-only MAY decline.

#### Scenario: Create
- **WHEN** the user activates New Folder
- **THEN** a new directory SHALL be created in the current location, appear in the listing, and be selected

#### Scenario: Unsupported source
- **WHEN** the active source does not support creation
- **THEN** the action SHALL report the error and leave the listing unchanged

### Requirement: Extension filters

The dialog SHALL restrict OpenFile/OpenProject listings and acceptance to the configured extensions, while
still allowing directory navigation.

#### Scenario: Filtered listing
- **WHEN** OpenProject is configured with the `*.skyproj` filter
- **THEN** the listing SHALL show directories and only files matching the filter

### Requirement: Modal input

While the dialog is open it SHALL capture pointer and keyboard input, and the editor content behind it
SHALL NOT receive input.

#### Scenario: Captured
- **WHEN** the dialog is open and the user clicks or types
- **THEN** the input SHALL be handled by the dialog only

### Requirement: Pluggable location source

The browser SHALL be backed by a pluggable location source (`IFileBrowserSource`) rather than being
filesystem-specific, so the same dialog also serves asset selection. A filesystem source SHALL be the
default; the source SHALL provide navigation, listing, and optional sidebar places, and MAY annotate
entries with an asset type id.

#### Scenario: Filesystem default
- **WHEN** no source is supplied
- **THEN** the browser SHALL browse the filesystem using a built-in filesystem source

#### Scenario: Asset source reuse
- **WHEN** an asset source is supplied
- **THEN** the browser SHALL list its entries and expose each entry's asset type id to filtering

### Requirement: Extension and asset-type filters

Filters SHALL match entries by file extension and/or by asset type id. The dialog SHALL expose a filter
selector that switches the active filter, and directory entries SHALL always remain visible.

#### Scenario: Filter by extension
- **WHEN** the `*.skyproj` filter is active in OpenProject mode
- **THEN** only directories and `*.skyproj` files SHALL be listed

#### Scenario: Filter by asset type
- **WHEN** a filter listing asset types is active
- **THEN** only directories and entries whose asset type id matches SHALL be listed

#### Scenario: Switch to all files
- **WHEN** the user selects the "All Files" filter
- **THEN** entries of every extension SHALL be listed

### Requirement: Navigation affordances

The dialog SHALL present a places sidebar of shortcuts, an editable location/toolbar with an up action, a
columned listing (name and type), a name field, and confirm/cancel actions.

#### Scenario: Places shortcut
- **WHEN** the user activates a sidebar place
- **THEN** the listing SHALL show that place's location

#### Scenario: Type column
- **WHEN** the listing is shown
- **THEN** directories SHALL read as "Folder" and files SHALL show their type (extension or asset type)

### Requirement: Floating panel surface

A floating panel SHALL be presented in its own operating-system window with its own UI context, swapchain, and
UI renderer, and SHALL render the same panel view that it renders when docked (including shared-device content
such as the viewport placeholder). Floating windows SHALL use the one editor Aurora device, not a second device,
and any shared content target SHALL be registered in every window that samples it.

#### Scenario: Floated panel draws in its own window
- **WHEN** a panel is floated
- **THEN** its view SHALL be rendered into a dedicated native window's swapchain, not inside the main window

#### Scenario: No extra device
- **WHEN** any number of floating windows exists
- **THEN** they SHALL all use the existing Aurora device

#### Scenario: Shared content is visible in a floating window
- **WHEN** a floating panel samples a content target owned by the editor (for example the viewport placeholder)
- **THEN** that target SHALL have been registered with the floating window's UI renderer and SHALL render

### Requirement: Native window creation order

Floating native windows SHALL respect the backend's graphics-instance creation ordering: on backends that can
only create graphics-capable windows before the device is initialized, floating windows SHALL be pre-created (for
example a hidden pool) at initialization and reused; on-demand creation SHALL be used only where the backend
supports creating a graphics-capable window after device initialization.

#### Scenario: Backend with a creation-order constraint
- **WHEN** a panel is floated on a backend that requires windows to be created before device init
- **THEN** a pre-created hidden window SHALL be shown and bound to the panel instead of creating a new window after
  device init

#### Scenario: On-demand backend
- **WHEN** a panel is floated on a backend that can create a graphics-capable window after device init
- **THEN** a new native window MAY be created on demand

### Requirement: Cross-window drag

Tear-out and re-dock SHALL be driven by pointer capture and screen coordinates, so a drag that leaves the source
window continues to be tracked and can be resolved against the main window or any floating window.

#### Scenario: Drag continues outside the source window
- **WHEN** a tab drag moves the pointer outside the window that started the drag
- **THEN** the drag SHALL continue (pointer capture / screen coordinates) and the drop SHALL be resolved against
  the window under the pointer

#### Scenario: Resolve the drop target
- **WHEN** a dragging panel is released over a floating window
- **THEN** the panel SHALL dock into that window's area, and when released over empty space away from any window
  it SHALL remain or become floating

### Requirement: Tear-out

Dragging a docked panel's tab outside the main window SHALL tear the panel out into a floating window, removing it
from the dock tree and adding it to the floating set.

#### Scenario: Tear out a tab
- **WHEN** the user drags a tab out of the main window and releases outside it
- **THEN** the panel SHALL become floating with its own window and SHALL no longer appear in the main window's dock
  tree

### Requirement: Re-dock

Dragging a floating panel's window (or its header) back over the main window SHALL dock the panel into the
indicated area and release its floating window (destroy it, or return a pre-created window to the pool), restoring
the panel in the dock tree.

#### Scenario: Re-dock a floating panel
- **WHEN** the user drags a floating panel over a dock area of the main window and releases
- **THEN** the panel SHALL be removed from the floating set and inserted into the dock tree at the indicated
  position, and its floating window SHALL be released

### Requirement: Floating window close

Closing a floating window SHALL re-dock its panel into the main window by default rather than losing the panel.
A close request SHALL be broadcast to owners on every supported backend (the SDL-backed surfaces SHALL broadcast
the same close notification as Win32), and closing a floating window SHALL NOT request application exit.

#### Scenario: Close a floating window
- **WHEN** the user closes a floating panel's window
- **THEN** the panel SHALL be re-docked into the main window and SHALL remain visible

#### Scenario: Close broadcast on every backend
- **WHEN** a floating window receives a close request on Windows, macOS, or Linux
- **THEN** the owner SHALL receive the close notification and re-dock the panel

#### Scenario: Closing a floating window does not exit
- **WHEN** a floating window is closed
- **THEN** the application SHALL keep running and only the main window close SHALL request exit

### Requirement: Window geometry notification

The framework SHALL notify owners when a floating window is moved, in addition to resize, so the editor can record
the window's position and size. The layout model SHALL remain the single serialized source of floating geometry,
updated from these notifications.

#### Scenario: Record moved geometry
- **WHEN** a floating window is moved or resized
- **THEN** the editor SHALL update that panel's floating geometry in the layout model and persist it

#### Scenario: Restore uses latest geometry
- **WHEN** the editor restarts after a floating window was moved
- **THEN** the panel SHALL reopen at the last recorded position

### Requirement: Multiple floating windows

The editor SHALL support multiple concurrent floating windows, each with independent content, geometry, and
presentation, and destroying one SHALL not affect the others.

#### Scenario: Several floating panels
- **WHEN** more than one panel is floated
- **THEN** each SHALL have its own window and geometry, and closing one SHALL leave the others intact

#### Scenario: Per-window resize
- **WHEN** a floating window is resized
- **THEN** only its own swapchain SHALL be rebuilt

### Requirement: Named configuration source

The editor SHALL resolve global configuration objects through a config source that returns a name and a
reflected data object for each registered configuration, without `EditorCore` depending on the concrete config
type or its storage.

#### Scenario: Registered configs are exposed

- **WHEN** the host has registered one or more named reflected configurations
- **THEN** the config source SHALL return each configuration's name and a reflected data object

#### Scenario: No configs

- **WHEN** no configuration is registered
- **THEN** the config source SHALL return an empty list and the panel SHALL show an empty state

### Requirement: Configuration form panel

The global config panel SHALL render each named configuration as a form using the `editor-reflected-form`
framework, stacked as one collapsible section per named configuration in a single scrollable view.

#### Scenario: Each config becomes a form

- **WHEN** the panel shows multiple named configurations
- **THEN** each configuration SHALL be rendered as its own titled form section

#### Scenario: Reuses the form framework

- **WHEN** a configuration object is rendered
- **THEN** its members, nesting, and controls SHALL be produced by the reflected-form framework, not by
  config-specific layout code

### Requirement: Configuration edits are undoable and persisted

Edits made through the config panel SHALL be routed through the `CommandService` so they are undoable, and SHALL
be written back to the underlying configuration storage so the change is observable through the config source.

#### Scenario: Edit is undoable

- **WHEN** the user edits a configuration value
- **THEN** the underlying configuration SHALL change and a subsequent undo SHALL restore the previous value

#### Scenario: Edit is written back

- **WHEN** the user edits a configuration value
- **THEN** reading that configuration through the config source SHALL return the new value

### Requirement: Global config panel uses the framework and a config source

The global config panel SHALL obtain named configurations through an `IEditorConfigSource` and render
each through the `editor-reflected-form` framework, and edits SHALL be undoable and written back to the
configuration storage.

#### Scenario: Named configs rendered

- **WHEN** the config source returns named reflected configurations
- **THEN** the panel SHALL render one section per configuration through the reflected-form framework

#### Scenario: Edit is undoable and persisted

- **WHEN** the user edits a configuration value
- **THEN** the change SHALL be applied through the command service and SHALL be observable when the
  configuration is read again

### Requirement: Toolkit-independent layout model

The editor layout SHALL be represented as a toolkit-independent data model: a tree of split, tab, and panel
nodes, where a panel node references a panel by id. The model SHALL contain no UI-toolkit or render types and
SHALL be usable without a window.

#### Scenario: Build and query a layout
- **WHEN** a layout is assembled from a split with two tab groups and queried
- **THEN** the model SHALL report the tree structure and the panel ids without requiring a window or GPU

### Requirement: Panel registry

The editor SHALL provide a panel registry that maps a panel id to its title, minimum size, and factory. A layout
SHALL reference panels only by id.

#### Scenario: Layout references registered panels
- **WHEN** a layout references a panel id
- **THEN** the registry SHALL resolve it to a panel definition, and unknown ids SHALL be reported rather than
  crashing

### Requirement: Layout operations

The layout model SHALL support split, move-to-area, tabify, dock-at-position, float, dock-floating, close,
set-ratio, and reset-to-default operations.

#### Scenario: Split and resize
- **WHEN** a tab node is split and the splitter ratio is changed
- **THEN** the model SHALL contain the new split with the requested ratio

#### Scenario: Move and tabify
- **WHEN** a panel is moved to another area or tabified with another panel
- **THEN** the panel node SHALL be re-parented into the target tab node

#### Scenario: Dock a panel at a drop position
- **WHEN** a panel is docked onto a target panel with a drop position
- **THEN** the model SHALL either tabify it (center) or insert it into a new tab split on the requested side of
  the target and normalize the parent split

#### Scenario: Float a docked panel
- **WHEN** a docked panel is floated
- **THEN** it SHALL be removed from the dock tree and added to the floating set

#### Scenario: Dock a floating panel
- **WHEN** a floating panel is docked
- **THEN** it SHALL be removed from the floating set and re-inserted into the dock tree

#### Scenario: Close and reset
- **WHEN** a panel is closed or the layout is reset to default
- **THEN** the layout SHALL reflect the closure or return to the default arrangement

### Requirement: Layout persistence

The editor SHALL serialize the layout to JSON with a version field and restore it on startup, and SHALL tolerate
layouts that reference unknown or missing panel ids.

#### Scenario: Save/restore round-trip
- **WHEN** a layout is saved and then restored
- **THEN** the restored layout SHALL be equivalent to the saved layout

#### Scenario: Stale layout
- **WHEN** a stored layout references a panel id that no longer exists
- **THEN** the editor SHALL skip that panel and still start with a valid layout

#### Scenario: Floating geometry persists
- **WHEN** a layout with floating panels and their window geometry is saved and restored
- **THEN** the restored layout SHALL reproduce the floating panels and their geometry

#### Scenario: Auto-save and restore per user
- **WHEN** the layout is changed
- **THEN** the editor SHALL auto-save it under the user-config path and SHALL load it on the next startup

#### Scenario: Load an older layout version
- **WHEN** a stored layout from an older version without floating data is loaded
- **THEN** the editor SHALL load it and start with a valid layout

### Requirement: Layout interaction in the shell

The editor shell SHALL render the layout with draggable splitters and a tab bar, and SHALL place panels inside the
single main window by default.

#### Scenario: Splitter and tab interaction
- **WHEN** the user drags a splitter or activates/reorders a tab
- **THEN** the shell SHALL update the layout model and re-layout the affected areas

#### Scenario: Drag-to-dock
- **WHEN** the user drags a tab over another panel area
- **THEN** the shell SHALL show a drop-zone highlight and, on drop, dock the panel at the indicated position
  (center = tabify, edge = split)

#### Scenario: Close a tab
- **WHEN** the user activates the close affordance on a tab
- **THEN** the shell SHALL close that panel through the layout model and collapse the emptied area

#### Scenario: No child OS window by default
- **WHEN** panels are docked in the default (non-floating) configuration
- **THEN** they SHALL be placed within the main window without creating child OS windows

### Requirement: Panel visibility through the layout model

The editor SHALL express panel visibility through the layout model: hiding a panel uses the model's close
operation, and showing a hidden panel re-adds it to the layout; the shell SHALL NOT track hidden panels itself.

#### Scenario: Hide a panel
- **WHEN** a panel is hidden (e.g. from the View menu)
- **THEN** it SHALL be removed from the layout model and the shell SHALL rebuild without it

#### Scenario: Show a hidden panel
- **WHEN** a previously hidden panel is shown
- **THEN** it SHALL be re-added to the layout model (split/tab) and the shell SHALL rebuild with it

### Requirement: Floating panel state in the layout model

The layout model SHALL represent floating panels as a set of entries (panel id plus window geometry) separate from
the dock tree, and a panel SHALL be either docked in the tree or floating, never both. The model SHALL contain no
UI-toolkit or render types.

#### Scenario: A panel is docked or floating
- **WHEN** a panel is floated or docked
- **THEN** the model SHALL report it in exactly one of the dock tree or the floating set

#### Scenario: Update floating geometry
- **WHEN** a floating panel is moved or resized
- **THEN** the model SHALL record its new geometry

#### Scenario: Geometry write-back from the window
- **WHEN** the renderer reports a floating window's move or resize
- **THEN** the model SHALL update that panel's geometry and remain the single serialized source

### Requirement: Dock target and drop zones

The layout model SHALL express docking as a target panel plus a drop position of center or one of the four edges,
and SHALL reject a drop that would dock a panel onto itself.

#### Scenario: Edge drop splits on the requested side
- **WHEN** a panel is dropped on the left, right, top, or bottom edge of a target
- **THEN** the model SHALL place the panel in a new tab split on that side of the target with the requested
  orientation

#### Scenario: Invalid drop
- **WHEN** a panel is dropped onto itself
- **THEN** the model SHALL make no change

### Requirement: Reset layout including floating panels

Resetting the layout SHALL restore the default dock arrangement and SHALL clear the floating set.

#### Scenario: Reset from a modified layout
- **WHEN** a layout has been rearranged and contains floating panels and it is reset
- **THEN** the model SHALL return to the default arrangement with no floating panels

### Requirement: Controlled editor hosts synchronize dependencies before allocation

Known migrated editor hosts SHALL explicitly synchronize pending layout dependencies before querying child preferred size or requesting incremental arrangement. Synchronization SHALL occur before paint and SHALL make newly observed data available to the same allocation pass. Controlled host-owned forms and composition nodes MAY opt out of volatility only after their layout inputs have notification paths. Unknown custom/derived hosts SHALL retain conservative layout behavior unless they explicitly satisfy the incremental notification contract.

#### Scenario: External change precedes a clean frame
- **WHEN** an injected dependency changes after a controlled editor host completes layout and the host's allocated rectangle remains unchanged
- **THEN** the next host allocation SHALL synchronize that dependency and consume its notified layout work before paint

#### Scenario: Unknown provider retains polling compatibility
- **WHEN** a custom provider element changes externally without a setter notification and retains default volatility
- **THEN** its containing incremental host SHALL execute the branch and observe the new layout rather than declare the provider stable

#### Scenario: Stable host repeats unchanged input
- **WHEN** a controlled editor host completes its work and no dependency, theme, rectangle, or volatile descendant changes
- **THEN** another incremental allocation SHALL be eligible to skip its completed arrangement work

### Requirement: World config supports explicit document and refresh notifications

WorldConfigPanel SHALL support explicit document binding and an explicit refresh notification for config data/storage changes at the same document address. Binding/refresh SHALL rebind the reflected config as necessary and invalidate layout before reuse. EditorShell SHALL propagate its document changes to applicable main and floating world-config views. The legacy DocumentProvider path SHALL remain compatible and volatile so unnotified provider changes are still observed during layout.

#### Scenario: Document changes through the shell
- **WHEN** the shell opens, closes, or replaces its world document and the panel rectangle is unchanged
- **THEN** applicable world-config views SHALL bind the new document or empty state before the next paint

#### Scenario: Same document replaces config storage
- **WHEN** a document owner rebuilds or relocates config storage and explicitly requests a world-config refresh
- **THEN** the panel SHALL rebind to the current config storage instead of retaining a stale reflected data pointer

#### Scenario: Shell repeats the same document pointer
- **WHEN** a document owner replaces config storage and calls EditorShell::SetWorldDocument with its current document pointer
- **THEN** applicable attached, hidden, and floating world-config views SHALL receive a refresh and rebind to the current storage

#### Scenario: Legacy provider returns another document
- **WHEN** a legacy provider returns a different document without a setter notification between incremental frames
- **THEN** the panel SHALL continue to detect the change and update its bound config through the conservative path

### Requirement: Preference operations return explicit results and conservative effect flags

Portable preference operations SHALL return a PreferenceResult with a status code, failing page/path/message context where available, and storageWritten/liveWritten/applicationApplied flags for that operation. storageWritten SHALL become true only after explicit Write success. liveWritten SHALL become true before entering the first live setter and applicationApplied before entering the first page application callback, including rejection/exception paths. These two flags SHALL indicate entry into potentially effectful stages rather than prove a value changed or application completed. Complete success SHALL be distinct from absent storage, storage failure, invalid/unsupported data, validation/application rejection, identity conflict and busy/reentrant rejection. A failed result SHALL NOT imply that potential effects were rolled back.

#### Scenario: Application fails after a durable write
- **WHEN** persistence succeeds but a later live setter or page application rejects
- **THEN** the result SHALL report failure with storageWritten and conservative live/application stage flags rather than report complete success or claim rollback

#### Scenario: Invalid data identifies its location
- **WHEN** a known page/member cannot be decoded or validated
- **THEN** the failure SHALL identify its page/path where available without depending on native UI state

### Requirement: Owned candidate copy exceptions unwind safely

If a reflected working-value copy throws during candidate capture, Apply SHALL return InvalidData before storage/live/application stages and retain the current working value/history for retry. Any::Create SHALL publish its type/storage state only after successful construction and SHALL use RAII to release raw storage if placement construction fails, without invoking the complete-object destructor on an incompletely constructed value. The construction exception SHALL propagate naturally to the existing capture boundary. Its public signature, failure contract and the generic JSON loader SHALL remain unchanged by this local correction.

#### Scenario: Nontrivial working settings reject copying
- **WHEN** an edited settings value containing a heap-backed string throws from its reflected copy constructor before candidate capture completes
- **THEN** the operation SHALL report InvalidData with no storage/live/application stage flags, preserve current working/history/live/storage state, and safely release incomplete candidate storage
- **AND** a later corrected candidate SHALL remain available for retry

### Requirement: Storage is injected and failed writes preserve old bytes

PreferencesPersistence SHALL use an injected IPreferenceStorage for document Read/Write. Read SHALL distinguish NotFound from a read error. A failed or throwing Write SHALL preserve that adapter's previous document bytes; adapters with partial replacement on failure/throw SHALL be unsupported. Operations on one store SHALL run under its owner's serialized-call contract. This SHALL NOT promise a transaction spanning storage, live setters and application callbacks.

#### Scenario: Storage is absent
- **WHEN** the adapter reports NotFound during Load
- **THEN** Load SHALL report absence and preserve provider-supplied live values

#### Scenario: Storage write rejects
- **WHEN** Write returns failure for a prepared document
- **THEN** previous bytes SHALL remain and session Apply SHALL publish no live values through that failed storage gate

### Requirement: Reflected preference decoding is checked before publication

The portable codec SHALL read the existing root-page-id to classId/elements object envelope through checked JSON/reflection operations over owned candidates. It SHALL validate known data shape, type/class identity, numeric validity/range and supported schemas without invoking assertion-based JsonInputArchive decoding or blindly executing arbitrary custom codecs. Invalid or unsupported known schemas SHALL return explicit failures before live publication. Runtime registration numbers and object addresses SHALL NOT be serialized as persistent identity.

#### Scenario: Field metadata retains current boundaries
- **WHEN** known reflected fields are encoded, decoded and published
- **THEN** encoding SHALL include readable fields, decoding SHALL change only writable candidate fields, and publication SHALL retain the existing top-level const/no-setter/READONLY/invisible filtering
- **AND** Load/Save SHALL NOT filter fields solely by the unreliable existing isStatic metadata

#### Scenario: Malformed or incompatible known data
- **WHEN** stored JSON has a malformed known value, mismatching classId or unsupported reflected schema
- **THEN** the operation SHALL fail with context and SHALL publish no partially decoded live page

#### Scenario: Missing fields retain provider defaults
- **WHEN** a valid document omits a registered page or one of its known fields
- **THEN** its owned candidate SHALL retain that provider-supplied value rather than reset unspecified data to zero

### Requirement: Save preserves unknown object data and replaces known arrays

Save SHALL merge encoded known fields into a fresh storage read while retaining unknown root/page/object keys and pages for currently absent plugins. Known arrays SHALL replace prior known arrays, including replacement by an empty array. Invalid existing documents SHALL be reported rather than silently overwritten as empty storage. Save SHALL NOT itself write live settings.

#### Scenario: Plugin is absent from this registry
- **WHEN** Save updates known registered pages while an existing document contains an unregistered plugin page and unknown object keys
- **THEN** those unknown entries SHALL remain in the saved document

#### Scenario: Sequence is replaced or cleared
- **WHEN** a known candidate sequence is shorter than its stored version or is empty
- **THEN** the saved/loaded known sequence SHALL match the candidate sequence without appending stale elements

#### Scenario: Read fails before Save
- **WHEN** the fresh storage read returns a read failure rather than NotFound
- **THEN** Save SHALL return failure without a document write or live publication

#### Scenario: Save owns input before callbacks
- **WHEN** Save prepares candidates before a storage/getter callback can release its caller
- **THEN** it SHALL retain explicit Any::Create-owned snapshots for the operation and convert a snapshot-copy exception through the safe InvalidData path

#### Scenario: Reflected type has a nonconst getter
- **WHEN** a readable reflected member supplies getterFn with no usable getterConstFn
- **THEN** the codec SHALL support reading that field from its owned candidate, preferring getterFn when present and rechecking validity afterward

### Requirement: Load prepares all candidates at a controlled host boundary

Load SHALL be called at a host-controlled boundary with no open editing session. It SHALL copy current provider values, decode all known data and validate the full candidate set before guarded live publication and page application. Decode/validation/read failures SHALL publish nothing. Publication failures SHALL report conservative partial live/application stage flags. The service SHALL NOT silently replace an open session's working objects.

#### Scenario: A later page fails validation
- **WHEN** an earlier page decodes successfully but another candidate fails decoding or validation
- **THEN** Load SHALL leave all live pages unmodified by that preparation attempt

#### Scenario: Publication callback rejects
- **WHEN** all loaded candidates validate but a later setter/application callback fails during publication
- **THEN** Load SHALL stop further publication and report conservative partial-effect flags without claiming arbitrary external rollback

### Requirement: Page callbacks retain provider ownership and identity guards

PreferencePage MAY supply pure candidate validation and retry-safe application callbacks. Validation SHALL run on an owned candidate before persistence/live writes; application SHALL run after guarded live writes. Callback/value state SHALL be copied where necessary and registrations/session/owner validity SHALL be rechecked after callback boundaries. Re-registration at the same address SHALL remain a new identity. Providers SHALL preserve metadata/object/callback lifetime and tolerate retry after potentially effectful rejection/exception; concurrent mutation and DLL unload are outside this contract.

#### Scenario: Validator mutates registration identity
- **WHEN** a callback removes or re-registers a candidate page
- **THEN** the old operation SHALL report conflict and SHALL NOT continue accessing the obsolete page registration

#### Scenario: Application is retried after partial failure
- **WHEN** a failed operation with partial effects is retried
- **THEN** the provider callback SHALL tolerate repeated application and the new result SHALL describe that new operation's effects

### Requirement: File storage honors the portable replacement contract

FilePreferenceStorage SHALL implement the existing IPreferenceStorage over an explicit path, distinguish NotFound from ReadFailed, and complete/close an exclusive same-directory temporary file before replacing the destination. A failed write SHALL preserve previous destination bytes and clean its temporary file. The owner SHALL serialize calls. The adapter SHALL NOT claim cross-writer compare-and-swap or power-loss durability; JSON/schema handling remains in PreferencesPersistence.

#### Scenario: Replace a real saved document
- **WHEN** a complete file Write succeeds
- **THEN** a subsequent Read SHALL return the new bytes and no operation-owned temporary file SHALL remain

#### Scenario: File write cannot complete
- **WHEN** directory, temporary-write or replacement work fails
- **THEN** Write SHALL report failure, keep any previous destination bytes and clean temporary artifacts created by that attempt

#### Scenario: Missing file versus invalid location
- **WHEN** the path is absent or cannot be read as a valid preference file
- **THEN** Read SHALL distinguish absence from read failure under the existing storage result protocol

### Requirement: Exception translation is limited to provider boundaries

Preferences SHALL translate failures only at injected guard, reflection/provider and storage-adapter calls covered by its structured result protocol. Ordinary JSON/collection/allocation work SHALL NOT acquire whole-operation catches. Session SHALL reuse shared candidate capture. Any and file resources SHALL use RAII or equivalent ownership that releases incomplete resources when construction or allocation exits exceptionally.

#### Scenario: Reflected copy throws before completing construction
- **WHEN** a reflected value constructor throws
- **THEN** unconstructed allocation SHALL be released without running the complete-object destructor, and capture SHALL retain its existing structured failure result

#### Scenario: Ordinary file or memory failure occurs
- **WHEN** file APIs report an IO error
- **THEN** storage SHALL report the existing IO result and preserve failed-write destination bytes; unrelated allocation exceptions SHALL propagate through RAII cleanup without a new blanket catch

### Requirement: Preferences dialog

The editor SHALL provide a modal Preferences dialog opened from the File menu, with a category list on the
left and the selected page on the right, and SHALL close on OK or Cancel.

#### Scenario: Open from the menu
- **WHEN** the user selects `File > Preferences…`
- **THEN** the Preferences dialog SHALL open, focused, over the editor content

#### Scenario: Modal input
- **WHEN** the dialog is open
- **THEN** pointer and keyboard input SHALL be captured by the dialog and the dock content SHALL NOT receive it

#### Scenario: Close
- **WHEN** the user presses OK, Cancel, or Esc
- **THEN** the dialog SHALL close and return input to the editor

### Requirement: Category pages contributed by modules

Preferences SHALL be organized into named categories/pages that modules contribute; the shell SHALL NOT
hard-code subsystem settings.

#### Scenario: Registered pages appear
- **WHEN** a module registers a preference page
- **THEN** that page SHALL appear as a category in the dialog and its settings SHALL render in the page host

#### Scenario: Built-in categories
- **WHEN** the editor starts
- **THEN** at least General, Editor, and Rendering pages SHALL be present, contributed through the
  registration model rather than the shell

### Requirement: Apply, OK, Cancel and Reset

The dialog SHALL edit owned working copies of reflected settings through a session-local command service. OK and Apply SHALL write reflected writable members to still-matching live page registrations and notify the host to persist/apply them. Cancel, Escape, Close and destruction SHALL discard unapplied working values and local history without changing the editor-wide history. Reset SHALL restore the page's reflected type defaults within the same local session. Committed changes SHALL take effect where the owning subsystem supports them.

#### Scenario: Cancel discards
- **WHEN** the user changes settings and presses Cancel, Escape or Close
- **THEN** unapplied working changes and local history SHALL be discarded
- **AND** the live settings and editor-wide document history SHALL remain unchanged

#### Scenario: Apply commits
- **WHEN** the user changes settings and presses Apply
- **THEN** reflected writable members SHALL be written to matching live pages and the host SHALL be notified
- **AND** local history SHALL be cleared before that notification
- **AND** discarding subsequent edits SHALL preserve the applied live values

#### Scenario: OK closes an applied session
- **WHEN** the user presses OK
- **THEN** the current working values SHALL be applied and that session SHALL close
- **AND** a new session opened by the applied callback SHALL NOT be closed by the previous OK operation

#### Scenario: Reset
- **WHEN** the user chooses Reset on a page
- **THEN** that page's fields SHALL return to their type defaults through local undoable edits
- **AND** those values SHALL remain unapplied until Apply or OK

#### Scenario: Undoable cross-page edit
- **WHEN** the user edits fields on multiple pages and switches pages
- **THEN** Undo and Redo SHALL traverse that session's changes across those pages
- **AND** those shortcuts SHALL NOT traverse editor-wide document history

#### Scenario: Reopen
- **WHEN** Preferences opens again after Apply, Cancel or Close
- **THEN** working values SHALL be copied from the current live settings and local history SHALL start empty

### Requirement: Per-user persistence

Committed preferences SHALL persist per user and SHALL be restored on the next startup; unknown keys in a
stored file SHALL be skipped without failing to load.

#### Scenario: Save and restore
- **WHEN** the user commits settings and restarts the editor
- **THEN** the dialog SHALL show the previously committed values

#### Scenario: Forward-compatible load
- **WHEN** the stored file contains keys not known to the current build
- **THEN** loading SHALL succeed and those keys SHALL be ignored

### Requirement: Reflection-driven settings model

Settings SHALL be expressed as reflected data types (plain structs) registered with the reflection
context and decorated with UI metadata (label, order, range, enum names); the model SHALL be
UI-toolkit-free, SHALL provide each setting with a default, and SHALL be testable headlessly. The set
of preference pages SHALL be dynamic (modules register page types), while each page's fields are a
fixed reflected type; the shell SHALL NOT hand-draw per-type controls.

#### Scenario: Defaults
- **WHEN** a setting has no stored value
- **THEN** the model SHALL report its declared (default-constructed) value

#### Scenario: Metadata drives the control
- **WHEN** a page renders a reflected field
- **THEN** the control SHALL be chosen from the field's reflected kind + metadata (bool / integer /
  float slider / enum dropdown), not from a per-type switch

#### Scenario: Registered pages
- **WHEN** a module registers a preference page type
- **THEN** that page SHALL appear as a category and its fields SHALL render through the generic
  reflected form

### Requirement: Owned values and stable page registrations

A Preferences session SHALL own value copies with safe reflected copy semantics, including owning string and sequence values. Editable pages SHALL have unique nonempty ids, copyable reflected types and live registration identities. Registration identity SHALL distinguish re-registration at an unchanged object address. Session commands SHALL target stable owned working objects and SHALL be discarded before those objects are released.

#### Scenario: Deep value cancellation
- **WHEN** a working string or sequence is edited and the session is discarded
- **THEN** the live value SHALL remain valid and unchanged without byte-copy restoration

#### Scenario: Reorder
- **WHEN** existing page registrations are reordered
- **THEN** their working-object addresses, selected page id and local cross-page history SHALL be preserved

#### Scenario: Remove or replace
- **WHEN** a page is removed, replaced with a different type or re-registered with the same id, type and address
- **THEN** reconciliation SHALL clear old history before releasing old working values
- **AND** the replacement SHALL start from its current live values

#### Scenario: Changed registration during Apply
- **WHEN** a setter notification closes/replaces the session or invalidates a page registration
- **THEN** Apply SHALL stop subsequent writes and SHALL NOT emit the success callback

### Requirement: Session callback lifetime

Preferences SHALL detach forms before replacing owned working storage and SHALL destroy child forms before their local services. Notification-capable dialog transitions and event dispatch SHALL retain local services until that dispatch returns. Weak owner/binding checks SHALL reject further access after destruction. Model transitions requested during a committed form edit SHALL wait until that widget event has returned.

#### Scenario: Callback replaces or destroys the dialog
- **WHEN** an applied, closed or local-history notification reopens, replaces or destroys the dialog
- **THEN** the previous operation SHALL NOT access released dialog/form storage or clear the new session's history

#### Scenario: Transition requested during an edit
- **WHEN** a synchronous edit observer requests Close, Open or model replacement
- **THEN** the current field binding SHALL remain valid until the committed widget dispatch returns
- **AND** the requested transition SHALL run at a subsequent safe event/layout boundary

### Requirement: Preferences uses shared container composition

Preferences SHALL compose category navigation, reflected page content, and footer actions through shared UI containers using UiTheme dimensions and explicit logical allocation sizes. The selected page SHALL remain a generic ReflectedFormView bound to the existing session-owned working page and local history. Layout SHALL finish before paint/input. Compatibility rectangle queries, paint, clipping, and hit-testing SHALL consume container-arranged geometry rather than independent per-control placement formulas. Painting SHALL NOT create or repair the layout tree.

#### Scenario: First layout precedes paint
- **WHEN** Preferences opens and is arranged before its first Paint
- **THEN** category rows, page viewport and footer actions SHALL already have their final bounds and logical allocations for paint and input

#### Scenario: Query and input share the arranged tree
- **WHEN** a caller queries a category or action rectangle after layout and sends pointer input within its visible bounds
- **THEN** hit-testing SHALL target the same arranged element that painting uses
- **AND** invalid indices and points outside the relevant clipped region SHALL NOT resolve to an active category/action

#### Scenario: Fractional origin retains logical allocation
- **WHEN** the dialog is arranged at a fractional origin or scaled size
- **THEN** nested measurement and arrangement SHALL use their explicit logical sizes without deriving a contradictory budget from rounded endpoint subtraction

#### Scenario: Paint consumes a prepared composition
- **WHEN** page registration changes after layout and Paint runs before the next explicit preparation boundary
- **THEN** Paint SHALL consume the prepared tree without synchronizing registrations or creating/rearranging category/action elements
- **AND** the next layout, input entry, explicit SynchronizeLayout or compatibility geometry query SHALL prepare the changed composition

### Requirement: Category and reflected page scroll independently

The category list SHALL have a bounded vertical scrolling viewport independent of the selected reflected page's scroll state. Scrolling SHALL route by the visible hovered region, while active reflected popup/capture interactions retain their established routing priority. Category/content scrolling SHALL NOT move footer actions. Offscreen or clipped category rows SHALL NOT be selectable through another region, and scroll offsets SHALL remain clamped after page/extent/viewport changes.

#### Scenario: Select a category beyond the initial viewport
- **WHEN** contributed pages overflow the category viewport and the user scrolls to a later visible row
- **THEN** that row SHALL be selectable and bind its session page through the existing generic form
- **AND** rows outside the viewport SHALL NOT receive selection input

#### Scenario: Wheel stays in the hovered region
- **WHEN** the category list and selected form both overflow and wheel input occurs over one of them
- **THEN** only that region's scroll offset SHALL change and the footer SHALL retain its arranged position

#### Scenario: Form popup keeps input priority
- **WHEN** a reflected page owns an active popup or captured interaction extending beyond the page viewport
- **THEN** category/footer routing SHALL preserve its existing delivery and cancellation semantics

#### Scenario: Foreign pointer cannot release form capture
- **WHEN** a form interaction is captured by one pointer and another pointer sends MOVE, UP or cancellation
- **THEN** the unrelated pointer SHALL NOT modify or end the captured form interaction

#### Scenario: Matching cancellation permits fresh capture
- **WHEN** the owning pointer is cancelled during a form interaction
- **THEN** the dialog SHALL clear its capture and forward cancellation safely to the form before a later pointer starts a new interaction

### Requirement: Category scrollbar stays within its reserved track

The category scrollbar SHALL paint through UiSkin using its actual reserved track rectangle with the viewport height. Its theme-sized thumb SHALL remain inside that rectangle at fractional, ordinary and increased scale and for small valid tracks. Content that fits the viewport SHALL draw no thumb. This painter SHALL NOT create a second layout or scroll owner.

#### Scenario: Reserved track changes with scale
- **WHEN** a reserved category scrollbar track is painted at 0.5x, 1x or 2x theme scale
- **THEN** emitted thumb geometry SHALL remain within the supplied track and represent the clamped scroll offset

#### Scenario: Track is tiny or content fits
- **WHEN** the reserved track is smaller than the normal themed thumb or the content does not overflow
- **THEN** thumb geometry SHALL be bounded to the valid track or omitted when content fits, without negative/inverted draw bounds

### Requirement: Preferences responds to available bounds and theme

Preferences SHALL derive panel, category, row, footer, action and spacing dimensions from UiTheme and constrain composition to the available surface. Smaller windows SHALL use bounded category/page allocations and an adapted shared-container footer configuration rather than overlapping fixed targets. Long labels SHALL stay within their allocated painted/hit regions. Theme revisions, same-scale metric changes, resize, page reconciliation and reopen SHALL prepare the affected tree before subsequent paint/input. Degenerate bounds MAY yield clipped or empty regions but SHALL NOT produce negative/NaN allocations or active targets outside their visible region.

#### Scenario: Smaller window adapts the layout
- **WHEN** available width cannot fit the normal footer row and nominal category width
- **THEN** container allocation SHALL adapt to the constrained bounds with finite nonnegative regions and nonoverlapping visible footer action targets

#### Scenario: Long category label stays contained
- **WHEN** a contributed category title exceeds its allocated row width
- **THEN** its text SHALL be clipped or ellipsized within that row and SHALL NOT extend the category's hit target or overlap the reflected page

#### Scenario: Scale changes before the next paint
- **WHEN** theme scale or same-scale layout metrics change after the dialog has been constructed
- **THEN** copied container inputs and affected layout state SHALL update before paint using the new theme without requiring dialog reconstruction

### Requirement: Composition preserves Preferences session behavior

Container and row rebuilding SHALL preserve the existing PreferencesSession working objects, local history, page registration reconciliation, Apply/OK/Cancel/Reset behavior, and guarded transition/callback lifetime. The form SHALL remain bound to session-local services and SHALL be unbound/destroyed before those services or working objects are released. Recomposition alone SHALL NOT clear valid session history or apply settings. Generic persistence/application and native module wiring SHALL remain unchanged by this layout migration.

#### Scenario: Cross-page undo survives navigation composition
- **WHEN** the user edits multiple working pages, scrolls/selects categories, or the dialog resizes
- **THEN** Undo/Redo SHALL retain the session's existing cross-page behavior and SHALL NOT consume editor-wide document history

#### Scenario: Apply and Cancel retain their baselines
- **WHEN** the user applies working changes, edits again and then cancels after a layout/page change
- **THEN** the applied live values SHALL remain and the subsequent unapplied working changes SHALL be discarded according to the existing session contract

#### Scenario: Layout transition notifies an external callback
- **WHEN** a safe pending transition or session operation triggers an observer that closes, reopens, replaces or destroys Preferences
- **THEN** the composition code SHALL preserve existing service retention and lifetime/generation checks without accessing released form/row/dialog storage or cancelling a replacement session

### Requirement: Session Apply orders persistence before guarded publication

PreferencesSession SHALL provide a detailed Apply result while retaining the bool Apply compatibility entry. Detailed Apply SHALL verify/capture owned candidates, validate all candidates, optionally persist through the injected service, perform guarded live writes and page application, and only then reach one guarded history-clear boundary. Complete success SHALL require that boundary's callbacks/current guard add no conflicting work. Read/serialization/validation/failed-Write results SHALL leave live settings unchanged and retain working values/history. Post-save failures before Clear SHALL retain the current working values/history where the owner remains current and report accumulated effects without rollback. A Clear-observer conflict SHALL preserve the observer's new work/history even though the published old snapshot's history has already been cleared.

#### Scenario: Validation or storage fails
- **WHEN** validation, serialization, read or Write fails before publication
- **THEN** working values/history SHALL remain available for correction/retry, live settings SHALL remain unchanged, and no success notification SHALL be emitted

#### Scenario: Save succeeds but publication fails
- **WHEN** durable Save succeeds and guarded live/application publication later fails
- **THEN** the result SHALL report partial effects and local history SHALL NOT be cleared merely because storage succeeded

#### Scenario: Apply completes successfully
- **WHEN** every required validation, storage and publication/application stage succeeds
- **THEN** the guarded history-clear boundary SHALL complete before success notification and subsequent unapplied working edits MAY be discarded without undoing that completed Apply

#### Scenario: No persistence service is configured
- **WHEN** an existing caller uses bool Apply or detailed Apply without an injected persistence service
- **THEN** validation and guarded publication SHALL remain available without storage effects and bool Apply SHALL report complete success only

### Requirement: Cancel does not roll back previously reported partial effects

Cancel/Close SHALL discard the current session's working values and history. Following a failed Apply with partial effects, they SHALL NOT promise to restore already-written storage, live values or provider application effects. Reopen SHALL capture current live values, and retry MAY repeat prior publication/application work through retry-safe providers.

#### Scenario: Cancel follows a post-save failure
- **WHEN** Apply reports storage/live/application effects and the user cancels the remaining working session
- **THEN** working values/history SHALL be discarded while those already-performed effects remain outside Cancel's rollback contract

### Requirement: Dialog exposes optional portable persistence and failure results

PreferencesDialog SHALL support an optional shared portable persistence service, expose its structured operation result, and present generic failure information through existing shared UI. Failed Apply/OK SHALL leave the current dialog/session open where its owner remains current, preserve retryable working/history state, suppress the legacy success callback and avoid an OK close. The existing void onApplied callback SHALL remain a success-only compatibility notification rather than the checked storage protocol. Generic fields, container composition and local/global history isolation SHALL remain unchanged.

#### Scenario: Failed OK remains retryable
- **WHEN** OK encounters a validation/storage/publication/application failure while the dialog remains current
- **THEN** it SHALL remain open, expose the failure/effect result and allow correction or retry without emitting a successful applied/closed notification

#### Scenario: Retry then success
- **WHEN** the failure is corrected and a later Apply succeeds
- **THEN** the detailed result SHALL describe the successful operation, local history SHALL clear through the existing guarded boundary, and the compatibility success notification SHALL run

### Requirement: Old operations do not consume replacement session history

Notification-capable validation/storage/publication/history operations SHALL retain required service/value lifetime and recheck session/owner generation after callbacks. A destroyed/reopened/replaced dialog or session SHALL stop the previous operation from accessing released storage or clearing new history. Busy/reentrant operations SHALL report their result rather than unsafely nesting the same publication operation.

#### Scenario: Current guard invalidates itself
- **WHEN** an isCurrent callback returns false, throws, or closes/reopens/destroys/edits its owner during the check
- **THEN** the old operation SHALL return Conflict and SHALL NOT treat a true return from a state-mutating guard as permission to access stale borrowed state

#### Scenario: Callback replaces the session
- **WHEN** a page/storage/history callback opens a replacement session during Apply
- **THEN** the previous operation SHALL stop under its generation/owner guards and SHALL NOT clear replacement working values or history

#### Scenario: Callback destroys the dialog
- **WHEN** a callback destroys the dialog while its operation is running
- **THEN** retained services MAY finish their guarded call but no later access SHALL use released dialog/form storage

### Requirement: Callback edits invalidate an Apply snapshot by history revision

CommandService SHALL expose a mutation revision that changes for edits inside an open transaction as well as ordinary history operations. Apply SHALL compare that revision with its captured revision around callback-capable stages, including its current guard. New edits from storage/application/guard callbacks SHALL invalidate the old snapshot, return Conflict and preserve the newer working/history state. After the old history's successful-publication Clear, only its own single revision increment SHALL be accepted; edits/history created by Clear observers or a final guard SHALL remain and SHALL NOT be cleared again by the old operation.

#### Scenario: Callback edits inside a transaction
- **WHEN** storage, application or a current guard edits the working page inside an open transaction without changing stack counts
- **THEN** the changed revision SHALL stop the stale Apply snapshot before further publication/history clearing and preserve that pending work

#### Scenario: Clear observer creates new history
- **WHEN** a history Clear observer or its subsequent guard creates another edit/history entry
- **THEN** the old operation SHALL return Conflict after observing more than its expected one revision increment and SHALL leave the new work intact

### Requirement: Host preferences use generic persistence and preserve contributors

The host SHALL register its owned built-in pages and contributed pages before its controlled generic preference Load, then inject the shared file persistence service into Shell/Dialog Apply rather than retain a three-built-in-page Save-only gate. The hub SHALL load available preferences before applying its settings. Built-in registration lifecycle SHALL remove only its exact owned registration ids and reject unrelated reserved-id collisions without clearing contributed pages. Existing saved page envelopes and unknown/temporarily absent plugin values SHALL retain the prior checked codec contract.

#### Scenario: Plugin pages survive host setup
- **WHEN** a contributor registers a nonreserved page and the host binds/rebinds its built-ins
- **THEN** that contributor SHALL remain registered and participate in generic Load/Save at the controlled registration boundary

#### Scenario: Built-in owner is released
- **WHEN** a built-in binding resets or is destroyed after a registration was replaced
- **THEN** it SHALL remove only its own still-matching registration ids and SHALL NOT remove a newer or plugin-owned page

#### Scenario: Existing saved file starts a new host
- **WHEN** the host loads an existing pageId/classId/elements document after registration
- **THEN** registered built-ins/contributors SHALL use the generic checked Load and later Apply SHALL be gated by the injected persistence service

### Requirement: Unsupported built-in setting effects are presented as stored only

Only editor.uiScale SHALL claim a connected immediate Shell/Theme application in this batch. The other eight current built-in values—language, theme, autosave, gridSize, snap, undoDepth, vsync and resolutionScale—SHALL remain editable/validated/stored but SHALL be identified as stored-only where no consumer exists. The UI/docs SHALL NOT claim they apply immediately or on restart. Rendering consumers SHALL remain outside the current RHI/RDG/backend exclusion.

#### Scenario: User edits a stored-only value
- **WHEN** the user changes one of those eight built-in values and Apply succeeds
- **THEN** its value SHALL be stored through the generic protocol and its presentation SHALL distinguish storage from an unimplemented feature effect

#### Scenario: User changes UI scale
- **WHEN** editor.uiScale applies successfully
- **THEN** its callback SHALL update the shared Shell/Theme user multiplier while preserving the previous result-aware history/partial-effect contract

### Requirement: Reflected copy reentry preserves source and replacement state

Preference capture SHALL own page descriptors before invoking reflected copying and SHALL recheck its current guard after each copy. Session Apply SHALL retain working source objects across End/Begin reentry. Synchronize SHALL build candidates without releasing existing working objects and SHALL publish them only while its generation, registry identities and history revision remain current.

#### Scenario: Copy mutates the registry
- **WHEN** a reflected copy clears or replaces the registry
- **THEN** capture SHALL stop before another borrowed page access and SHALL leave its output unchanged on Conflict

#### Scenario: Copy ends the source session
- **WHEN** a reflected constructor ends or replaces the session and continues reading its source
- **THEN** that source SHALL remain alive until copying completes and the old operation SHALL not publish into the replacement session

#### Scenario: Synchronization copy creates newer history
- **WHEN** a reflected constructor edits an existing working object or opens another session
- **THEN** the stale synchronization SHALL retain that newer state/history and SHALL NOT release its command targets

### Requirement: Opening Preferences stops after owner destruction

OpenPreferences SHALL verify shell lifetime, dialog identity and chrome generation after callback-capable setup/open operations. History observers SHALL be allowed to destroy or replace the owner without a later stale continuation.

#### Scenario: Reopening destroys the shell
- **WHEN** a SetModel or Open history notification destroys the shell during Preferences reopening
- **THEN** OpenPreferences SHALL return without accessing released shell or dialog storage

### Requirement: Editor builtin resource root

The sandbox editor SHALL own a builtin resource tree for editor-only resources (icons, editor fonts) at
`engine/sandbox/resources/`, separate from the project-package `assets/` tree.

#### Scenario: Resource root contents
- **WHEN** the editor resource tree is inspected
- **THEN** it SHALL contain the editor's builtin resources (for example `icons/` and `fonts/`) under a single
  root that is not the project package `assets/`

### Requirement: Bundle-relative resource resolution

The editor SHALL resolve builtin resource paths relative to the executable (platform bundle path), not the current
working directory, so the editor can be launched from any working directory.

#### Scenario: Resolve from a non-repo working directory
- **WHEN** the editor is launched from the executable's output directory (no `assets/` present)
- **THEN** `SandboxResources::Root()` SHALL resolve to `<executable_dir>/resources/` and `Resolve(relative)` SHALL
  return an absolute path under it

#### Scenario: No working-directory dependency
- **WHEN** the same resource is requested under different working directories
- **THEN** the resolved path SHALL be the same

### Requirement: Resource deployment to the executable

The build SHALL deploy the editor builtin resources next to the executable, preferring a symbolic link and falling
back to a recursive copy when symlinks are unavailable, without deleting the source tree.

#### Scenario: Symlink when supported
- **WHEN** the platform allows creating a directory symlink
- **THEN** the deployed `<executable_dir>/resources` SHALL be a symbolic link to the source resource tree

#### Scenario: Copy fallback
- **WHEN** creating the symlink fails (for example Windows without developer mode)
- **THEN** the deployment SHALL fall back to copying the resource tree into `<executable_dir>/resources`

#### Scenario: Source tree preserved
- **WHEN** a previous deployment (symlink or copied tree) is replaced
- **THEN** the source resource tree SHALL remain intact

### Requirement: Editor font loads from the builtin resources

The editor text system SHALL load its font through the builtin resource resolution, so the font is found when the
editor runs from the deployed output directory.

#### Scenario: Font found after deployment
- **WHEN** the editor starts from the output directory after a build
- **THEN** the font SHALL load from `<executable_dir>/resources/fonts/` and the text system SHALL NOT fall back to
  the block-glyph provider

### Requirement: SVG icon rasterization through the DDC

The editor SHALL provide a UI icon builder that converts SVG source bytes into RGBA8 pixels at a
requested pixel size, registered with the derived data cache under a stable id so that results are
cached and reused across runs.

#### Scenario: Icon rasterized and cached

- **WHEN** the icon builder builds an SVG at a requested pixel size
- **THEN** it SHALL emit RGBA8 pixels at that size and the derived data cache SHALL store the result

#### Scenario: Requested size is part of the cache key

- **WHEN** the same SVG is built at two different requested sizes
- **THEN** each size SHALL be stored as its own cache entry

#### Scenario: Invalid source fails gracefully

- **WHEN** the source bytes are empty or cannot be parsed as SVG
- **THEN** the builder SHALL report failure without emitting pixels

### Requirement: Resource-backed icon bake in the editor

The editor SHALL bake UI icons from SVG sources stored under the sandbox resources tree and SHALL
store the derived output under the sandbox resources directory, so a control bound to a resource
icon renders from the cache; when the source cannot be read it SHALL fall back to a directly
generated glyph.

#### Scenario: Icon baked from a resource SVG

- **WHEN** the editor displays a control bound to `resources/icons/<name>.svg`
- **THEN** the icon SHALL be rasterized through the derived data cache and the derived bytes SHALL be stored under the sandbox resources directory

#### Scenario: Missing source still draws an icon

- **WHEN** the source SVG cannot be read
- **THEN** the control SHALL draw a directly generated icon without failing

#### Scenario: Icon control is interactive

- **WHEN** the pointer moves over or presses the icon control
- **THEN** the control SHALL show hover and press feedback in response

### Requirement: Icon dependency isolation

The SVG rasterization dependency SHALL be referenced only by the sandbox editor module and SHALL NOT
be exposed to engine or other plugin targets.

#### Scenario: Engine targets do not link the SVG rasterizer

- **WHEN** the engine and plugin target link dependencies are inspected
- **THEN** no engine target SHALL link the SVG rasterizer third-party

### Requirement: Single UI style source

The editor UI dimensions and typography SHALL come from a single theme (`UiTheme` with `UiMetrics` / `UiFonts`);
views SHALL derive sizes from the theme rather than hard-coding pixel layouts.

#### Scenario: Derived size
- **WHEN** a view needs a row height / padding / field height
- **THEN** it SHALL read it from the theme metrics (or an expression of them), not a literal

### Requirement: Uniform scaling

The theme SHALL provide a scale; applying it SHALL multiply every metric and font size together, so widget
sizes and text scale uniformly.

#### Scenario: Scale doubles
- **WHEN** the theme is built at 2× scale
- **THEN** every metric (e.g. row height) and font size SHALL be doubled

### Requirement: Chrome scales with DPI

The menu bar, toolbar, and status bar heights SHALL be theme metrics so the editor chrome scales with DPI like
the rest of the UI.

#### Scenario: High-DPI chrome
- **WHEN** the effective scale is above 1 (e.g. a 150% display)
- **THEN** the menu bar, toolbar, and status bar SHALL grow with the theme scale, not stay at fixed pixels

### Requirement: Effective scale from DPI and preference

The effective scale SHALL be the system DPI scale times a user preference (`editor.uiScale`), with an
environment override for development, and SHALL apply to the editor and the Project Manager hub.

#### Scenario: User preference multiplies DPI
- **WHEN** the display DPI scale is 1.5 and the user sets UI Scale to 1.25
- **THEN** the effective scale SHALL be 1.875

### Requirement: Reset Layout restores the built-out default

Resetting the layout SHALL restore the full default arrangement that was built (splits + panels), not a single
initial panel.

#### Scenario: Restore arrangement
- **WHEN** the default layout has been built and captured, the user rearranges/closes panels, and chooses
  Reset Layout
- **THEN** the full default arrangement SHALL be restored

### Requirement: Default theme revisions invalidate complete surface layout

The shared default UiTheme SHALL retain its stable address and expose a process-shared revision that advances whenever SetDefaultUiTheme is called, including same-scale updates. Editor surfaces SHALL select their effective DPI theme before layout synchronization and SHALL apply a newly observed revision by invalidating layout/measurement state for every live element in that surface before incremental reuse. Invalidating only the root SHALL NOT be considered sufficient. Main/floating surfaces SHALL independently track the revision applied to their layout. Independently mutable custom themes require owner notification or conservative volatility.

#### Scenario: Another DLL updates the theme
- **WHEN** an editor module sets the default theme through its linked EditorUi copy
- **THEN** other editor modules SHALL observe the same theme value/address and advanced revision

#### Scenario: Metrics change at the same scale
- **WHEN** the default theme's metrics or fonts change at the same scale after a clean layout
- **THEN** the next surface layout SHALL invalidate nested cached sizes/arrangements and update affected geometry even if outer rectangles remain unchanged

#### Scenario: Main and floating surfaces use different DPI
- **WHEN** main and floating surfaces select different effective DPI themes during successive paints
- **THEN** each surface SHALL apply the active theme revision before its own layout and SHALL NOT reuse nested geometry prepared under an incompatible theme

#### Scenario: Theme is unchanged between ordinary frames
- **WHEN** a surface receives the same theme revision and allocation inputs without a dependency mutation
- **THEN** theme handling SHALL NOT itself invalidate otherwise reusable layout

#### Scenario: Hidden view missed a live theme update
- **WHEN** a preserved view was detached during a theme change and is adopted back into a surface whose revision is already current
- **THEN** its entire subtree SHALL be synchronized before reuse so unchanged nested allocations cannot retain old-theme measurement results

#### Scenario: Main menu bar metrics change without rebuilding chrome
- **WHEN** the active theme changes menu bar height after main-window chrome construction
- **THEN** main layout SHALL update the menu bar's stored height before allocation so its paint, input, and popup origin use the active metric

### Requirement: Every editor surface multiplies device DPI by user scale once

Main and floating editor windows SHALL retain separate device-DPI and user-preference inputs. SetUiScale SHALL set the main device input, SetSurfaceScale the floating device input, and SetUserUiScale the user multiplier; effective scale queries SHALL use the same normalized product clamped to 0.5–4. The supported user preference range SHALL remain 0.5–2. A floating device override SHALL NOT bypass or duplicate the user multiplier. Startup/hub preference load and later Apply SHALL share this policy, retaining the developer environment override at the device-input boundary.

#### Scenario: Floating surface has a different DPI
- **WHEN** main/floating device scales differ and the same user multiplier is applied
- **THEN** each surface SHALL compute its own device×user result once rather than reuse an already-multiplied main value

#### Scenario: User multiplier changes after construction
- **WHEN** UI scale is applied from Preferences
- **THEN** the next main/floating layout SHALL use the updated effective scale and existing theme-revision invalidation

#### Scenario: Scale input is invalid or out of range
- **WHEN** a supplied scale is nonfinite or outside its supported range
- **THEN** normalization/clamping SHALL keep effective theme geometry finite and within the declared effective range

### Requirement: Surface presentation and input establish the target theme

The existing host GUI dispatch SHALL preserve its supplied surface identity through Shell presentation. Shell pointer, key and text entry points SHALL select that target surface's effective theme and prepare its layout before routing input, with existing owner/attachment lifetime guards. A geometry query without a supplied window context SHALL NOT be assumed to infer the target surface; the presentation/input host establishes that context.

#### Scenario: Main modal receives input after a floating paint
- **WHEN** a different-DPI floating surface paints and the main window next receives pointer, key or text input
- **THEN** input preparation SHALL select the main window's theme/layout before routing, including modal Cancel and field hit targets

#### Scenario: Host presents a floating surface
- **WHEN** the GUI callback is invoked for a floating surface
- **THEN** the supplied identity SHALL reach EditorShell::PaintSurface rather than presenting the main surface in that window

### Requirement: Panels built on the in-house UI framework

Editor panels SHALL be authored as `engine/ui` (`sky::ui`) element trees and SHALL NOT use OS-native widgets or
an external UI toolkit.

#### Scenario: Panel is a sky::ui tree
- **WHEN** the editor creates a panel
- **THEN** the panel SHALL be composed of `sky::ui` elements and emit draw data through the UI render pass

### Requirement: Docking provided by editor-layout

The shell SHALL arrange panels according to the `editor-layout` model. Panel docking, tabs, splits, and layout
persistence are specified by the `editor-layout` capability.

#### Scenario: Shell renders the layout model
- **WHEN** the layout model defines split and tab areas
- **THEN** the shell SHALL place the panels accordingly and route input to the active panel

### Requirement: Engine-drawn menus and theming

The editor menu bar and toolbars SHALL be drawn by the engine with `sky::ui` (not the OS), so they are consistent
across Windows and macOS, and SHALL resolve appearance from the `sky::ui` style/theme rather than hard-coded
values.

#### Scenario: Consistent menu bar
- **WHEN** the editor starts on Windows or macOS
- **THEN** the menu bar SHALL be rendered by the engine as `sky::ui` elements, without relying on an OS-native
  menu bar

#### Scenario: Theme change affects panels
- **WHEN** the active theme is changed
- **THEN** subsequent paints of shell and panel elements SHALL use the new theme values

### Requirement: Focus and input routing

The shell SHALL route pointer and keyboard input to the focused panel, and SHALL report whether a UI surface
wants input so game/viewport input is gated while UI is active.

#### Scenario: UI gating
- **WHEN** a focused or modal UI surface is active
- **THEN** the shell SHALL report that it wants input and the viewport SHALL not receive that input

### Requirement: Shell composed from the core layout and panel registry

The editor shell SHALL build its `sky::ui` element tree from the `editor-layout` model and the `PanelRegistry`
owned by `EditorCore`, and SHALL NOT hardcode panel content or placement. Panel ids that have no registered view
SHALL be skipped without failing.

#### Scenario: Panels follow the default layout
- **WHEN** the editor starts with the default layout and the registered core panel ids
- **THEN** the shell SHALL create a `sky::ui` element subtree for each panel id present in the layout

#### Scenario: Unknown panel id is skipped
- **WHEN** the layout references a panel id that has no registered view
- **THEN** the shell SHALL skip it and continue building the rest of the shell

### Requirement: Input routed through the UI event router

Platform window pointer and keyboard events SHALL be forwarded to the shell, which SHALL dispatch them through the
`sky::ui` `UIEventRouter`, and SHALL expose `UIContext::WantsInput()` so the host can gate non-UI input.

#### Scenario: Pointer routes to the topmost element
- **WHEN** a pointer event occurs over overlapping UI elements
- **THEN** the router SHALL dispatch it to the topmost visible element under the point

#### Scenario: Wants input reported
- **WHEN** a focused or modal UI surface is active
- **THEN** `UIContext::WantsInput()` SHALL be true

### Requirement: Viewport input gating

While the shell reports that it wants input, the editor viewport SHALL NOT receive the pointer/keyboard input.

#### Scenario: UI captures input
- **WHEN** the shell reports `WantsInput()` and a pointer event arrives over the UI
- **THEN** the viewport SHALL NOT receive that event

### Requirement: Theme-driven panel styling

The editor shell SHALL style its surfaces through the UI theme (`UITheme`/`UIStyle`), so panel, title, menu and
button colors come from a single theme rather than hardcoded per-panel values.

#### Scenario: Panels resolve colors from the theme
- **WHEN** a panel is painted
- **THEN** its background and title colors SHALL come from the theme's resolved style for the panel's classes

#### Scenario: Changing the theme changes the shell
- **WHEN** a theme style value is changed
- **THEN** the affected shell surfaces SHALL render with the new value, with no per-panel constant to change

### Requirement: Tab headers switch the active panel

A tab that contains more than one panel SHALL render a header row of its panel titles, and selecting a header SHALL
make that panel the tab's active panel and show its view.

#### Scenario: Switch tabs
- **WHEN** the user selects a panel title in a multi-panel tab's header
- **THEN** that panel SHALL become active and its view SHALL be shown

#### Scenario: Only the active panel body is shown
- **WHEN** a tab has multiple panels
- **THEN** only the active panel's body SHALL be laid out and painted

### Requirement: Engine-drawn menu bar

The shell SHALL present an engine-drawn menu bar with labeled top-level menus (at least File, Edit, View, Window,
Tools, Help) drawn with `sky::ui` (not the OS), and selecting a menu item SHALL run its associated editor action.
The View menu SHALL expose panel visibility toggles, and the Window menu SHALL expose a reset-layout action.

#### Scenario: Open a menu and run an item
- **WHEN** the user opens a top-level menu and selects an item
- **THEN** the associated editor action SHALL run and the menu SHALL close

#### Scenario: View menu toggles a panel
- **WHEN** the user selects a panel entry in the View menu
- **THEN** that panel SHALL be hidden if shown, or shown if hidden, through the layout model

#### Scenario: Consistent menu bar across platforms
- **WHEN** the editor starts on Windows or macOS
- **THEN** the menu bar SHALL be rendered by the engine as `sky::ui` elements, without relying on an OS-native
  menu bar

### Requirement: Status bar

The shell SHALL present an engine-drawn status bar showing at least the project name, engine version, active RHI
backend, frame rate, edit mode, and selection count, resolved from existing editor services and not from
hard-coded constants.

#### Scenario: Status bar reflects state
- **WHEN** the selection or the frame rate changes
- **THEN** the status bar SHALL display the updated value on the next paint

### Requirement: Drag-to-dock interaction

The shell SHALL provide a drag interaction for panel tabs: while a tab is dragged the shell SHALL highlight the
candidate drop zone, and on release it SHALL dock the panel at the indicated position or float it when dropped
outside the main window. Gesture hit areas SHALL be discrete elements (splitter handles and tab headers) so panel
bodies remain directly hittable; the drop-zone highlight SHALL exist only while a drag is active. All gesture
hit-testing SHALL use the same coordinate space as the layout (device pixels), independent of the paint scale.

#### Scenario: Drop zone highlight
- **WHEN** the user drags a tab over a panel area
- **THEN** the shell SHALL show a drop-zone highlight indicating center or a side

#### Scenario: Float on drop outside
- **WHEN** the user drops a dragged tab outside the main window
- **THEN** the shell SHALL float the panel into its own native window

#### Scenario: Panel input is not swallowed
- **WHEN** the pointer is over a panel body and not over a splitter handle or tab header
- **THEN** the panel SHALL receive the event directly (no full-area overlay intercepts it)

### Requirement: Per-window input routing

The shell SHALL route keyboard and text input to the `UIContext` of the window identified by the event's
`winID`, and SHALL route non-drag pointer input to that window's own event router so each window keeps its own
focus and hover state. For a drag that may cross windows (tab reorder, drag-to-dock, tear-out), the shell SHALL
use OS-level pointer capture and screen coordinates so the drag continues when the pointer leaves the source
window.

#### Scenario: Keyboard goes to the focused window
- **WHEN** a key event arrives for a floating window's `winID`
- **THEN** the shell SHALL dispatch it to that window's `UIContext` router and not to the main window

#### Scenario: Drag leaves the window
- **WHEN** the user drags a tab and the pointer moves outside the source window while the button is held
- **THEN** the shell SHALL keep receiving the drag (via pointer capture / screen coordinates) and SHALL be able
  to drop outside the window

### Requirement: Preferences entry in the File menu

The File menu SHALL include a `Preferences…` command that opens the Preferences dialog, and the docked
`Config` panel SHALL no longer be part of the default layout or the View menu.

#### Scenario: Preferences command
- **WHEN** the user opens the File menu and selects `Preferences…`
- **THEN** the Preferences dialog SHALL open (see `editor-preferences`)

#### Scenario: Config panel retired
- **WHEN** the editor starts with the default layout
- **THEN** there SHALL be no `Config` panel/tab, and no `Config` entry in the View menu

### Requirement: Asset browser is a renderer

The asset browser panel SHALL render the catalog-built tree and item set and perform layout and hit
testing only; it SHALL NOT assemble the folder tree or query per-folder contents.

#### Scenario: Render core-built tree
- **WHEN** the panel displays the folder tree
- **THEN** it SHALL render the tree provided by the catalog without recursing per folder

#### Scenario: View holds no structure
- **WHEN** the catalog changes
- **THEN** the panel SHALL re-render from the catalog rather than maintaining its own folder model

### Requirement: Detail view shows targets and settings

The asset browser detail view SHALL show the active platform, the resolved cook targets, and, for
each target, the effective per-kind settings reported by the asset's builder (or the targets alone
when the kind reports no settings).

#### Scenario: Texture settings shown
- **WHEN** a texture is selected
- **THEN** the detail view SHALL show its targets and each target's effective settings (e.g. encode,
  srgb, max size, mip generation)

#### Scenario: No builder settings
- **WHEN** the asset's kind reports no settings
- **THEN** the detail view SHALL show the targets without settings and SHALL NOT error

### Requirement: Usable default bottom dock

The default layout SHALL give the bottom dock (asset browser + output log) a usable height, set
explicitly on the correct split rather than left to an accidental ratio.

#### Scenario: Bottom dock visible by default
- **WHEN** the editor starts with the default layout
- **THEN** the bottom dock SHALL show the asset browser with enough height to display items

### Requirement: Built-in asset browser panel

The shell SHALL register a built-in view for the `assets` panel id and SHALL include it in the default
layout, composed from the core layout and panel registry like the other built-in panels. The panel view
SHALL render the asset browser with the in-house `sky::ui` toolkit.

#### Scenario: Registered built-in view
- **WHEN** the shell registers its built-in panel views
- **THEN** an `assets` panel id SHALL have a view that renders the asset browser

#### Scenario: Present in the default layout
- **WHEN** the editor starts with the default layout
- **THEN** the `assets` panel SHALL be part of it and SHALL dock, float, and tab like the other panels

### Requirement: Editable per-asset cook settings in the asset browser

The asset browser details pane SHALL edit the selected asset's per-target cook settings through the generic
reflected form view (`ReflectedFormView`) bound to the catalog's effective reflected settings object with the
global-preset baseline, so that enums/bools/ints render as their standard typed controls and "reset"
restores the preset. Edits SHALL be committed through the asset catalog and persisted to the manifest; for
assets on a read-only mount the settings SHALL be shown non-editable.

#### Scenario: Bind the reflected form

- **WHEN** an asset with a builder-declared settings type is selected
- **THEN** the details pane binds a `ReflectedFormView` to the effective settings with the preset baseline

#### Scenario: Edit persists

- **WHEN** the user changes a value through the form
- **THEN** the catalog persists the resulting sparse override and the pane reflects the new effective value

#### Scenario: Reset restores the preset

- **WHEN** the user resets an overridden value through the form
- **THEN** the override key is removed and the shown value returns to the preset baseline

#### Scenario: Read-only mount

- **WHEN** the selected asset belongs to a read-only mount
- **THEN** the settings are displayed without editable controls

### Requirement: Shell hosts the asset viewer

`EditorShell` SHALL host the asset viewer widget as an overlay dialog (like the file browser), expose
`OpenAssetViewer(uuid)`, include it in modal input routing and layout, and SHALL route asset double-click /
`asset.open` to it when no type-specific `IEditorAssetEditor` is registered. The type-specific editor SHALL
take precedence when registered.

#### Scenario: Double-click opens the viewer

- **WHEN** an asset is double-clicked and no type-specific editor is registered
- **THEN** the shell opens the asset viewer for that uuid and it becomes the active modal

#### Scenario: Type-specific editor wins

- **WHEN** an `IEditorAssetEditor` is registered for the asset type
- **THEN** opening routes to that editor instead of the generic viewer

#### Scenario: Viewer participates in input routing

- **WHEN** the viewer is open
- **THEN** pointer/key/text input is routed to it (Escape closes) and normal panels do not receive it

### Requirement: T11 remaining-screen composition has a frozen four-view scope

FileBrowserDialog, NewWorldDialog, ProjectManagerView and WorldConfigPanel SHALL allocate their frozen outer regions/control slots through shared UI containers with theme inputs and explicit logical sizes. Compatibility geometry queries and pointer hit-testing SHALL consume arranged slots/visible viewports rather than independent dialog-level placement formulas. Paint SHALL consume prepared geometry. Small/degenerate allocations SHALL remain finite/nonnegative and long labels SHALL stay within their allotted clips. Specialized list-row/glyph painting and existing models/business operations MAY remain within their owning regions. This delivery SHALL NOT claim every editor screen is migrated.

#### Scenario: File browser regions are composed
- **WHEN** the browser first lays out or resizes
- **THEN** sidebar/navigation/list/footer/control queries SHALL follow shared-container allocation while navigation/filter/create/rename/accept/cancel and specialized popup behavior remain available

#### Scenario: New world fields and actions are composed
- **WHEN** New World lays out before first paint or after a theme/size change
- **THEN** name/location/Create/Cancel targets SHALL follow the arranged field/footer slots while existing text/path/create semantics remain intact

#### Scenario: Project Manager controls and recent viewport are composed
- **WHEN** the hub lays out or its geometry is queried before paint
- **THEN** banner/top actions/selection actions/header/list/status SHALL use arranged container slots, clipped rows SHALL not select through other regions, and existing recent/open/add/new/remove/confirmed-delete behavior SHALL remain

#### Scenario: World config keeps generic form behavior
- **WHEN** its composed list/form allocation changes or its document is rebound
- **THEN** the subsystem navigation and stable generic form SHALL use arranged regions while explicit document refresh/provider compatibility and subsystem enable/save behavior remain intact

### Requirement: T11 implementation and platform acceptance remain distinct

The closeout SHALL record the frozen inventory, actual settings-application matrix, narrow settings-only SandboxModule exception and executed portable/file/ASan checks. Windows/native module execution and real display/cross-DPI/GPU evidence SHALL remain explicit open gates when not run. Renderer initialization/tick/surface/refactor code, RHI/RDG/backends and Core Event SHALL remain outside the settings exception.

#### Scenario: Portable closeout succeeds without native GPU
- **WHEN** implementation and targeted portable checks complete on a host without a usable native device
- **THEN** T11 implementation closeout MAY be recorded with actual evidence while T11 platform acceptance remains unfinished

### Requirement: Native floating input reaches its existing surface dispatcher

SandboxModule SHALL route key-down, key-up and filtered text events from a known floating native window to the existing EditorShell surface APIs. A known floating window SHALL NOT be learned as the main window when its event arrives first. Control-character filtering and existing main-window routes SHALL remain in effect.

#### Scenario: Restored floating window receives the first event
- **WHEN** a known floating native window receives keyboard/text input before the main window
- **THEN** the host SHALL resolve its current surface identity and dispatch there without replacing the main-window identity

### Requirement: Themeable editor styling

The editor UI SHALL be drawn through a themeable style layer where colors, metrics and fonts come from a
swappable theme, and panels/components SHALL draw through shared skin primitives rather than panel-local
layout code.

#### Scenario: One theme drives all panels

- **WHEN** a theme is supplied
- **THEN** panels SHALL render with that theme's colors, metrics and fonts

### Requirement: Anti-aliased rounded shapes without MSAA

The style layer SHALL render rounded rectangles, fields and borders with anti-aliased edges without
relying on multi-sample anti-aliasing.

#### Scenario: Smooth rounded corners

- **WHEN** a rounded rectangle or field is drawn
- **THEN** its corners and border SHALL be anti-aliased rather than showing hard stair-steps

#### Scenario: Graceful fallback

- **WHEN** no texture registry is available for the anti-aliased path
- **THEN** the style layer SHALL fall back to an equivalent non-anti-aliased path without failing

### Requirement: Default editor theme is shared across DLLs

The default UiTheme accessed through GetDefaultUiTheme and SetDefaultUiTheme SHALL use Environment-backed process-shared storage with a stable object address. Replacing default theme values SHALL update already-created shared skins/widgets that reference that default, including consumers in another DLL. Explicitly injected custom themes SHALL remain usable independently of the default.

#### Scenario: Default theme changes cross the module boundary
- **WHEN** the host changes the default theme and an already-created feature-module skin reads it
- **THEN** the skin SHALL observe the changed colors, metrics, and fonts from the same stable theme object

#### Scenario: Feature and host share default theme identity
- **WHEN** a real dynamic module and its host each access the default theme
- **THEN** both SHALL receive the same object address rather than independent function-static copies

#### Scenario: Explicit custom theme stays independent
- **WHEN** a form uses an explicitly supplied theme and the default theme changes
- **THEN** the form SHALL continue to use its supplied theme

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

### Requirement: Persist window geometry

The editor SHALL persist the main window's size and position per user and restore them on the next launch.

#### Scenario: Save on graceful exit
- **WHEN** the user closes the editor window normally
- **THEN** the current window client size and screen position SHALL be written to the user's editor state

#### Scenario: Restore on launch
- **WHEN** the editor starts and a valid saved window state exists
- **THEN** the main window SHALL open at the saved size and position

#### Scenario: No saved state
- **WHEN** no saved window state exists or it is malformed
- **THEN** the window SHALL open at the default size

#### Scenario: Bounded run does not overwrite
- **WHEN** the editor runs bounded (for example `--frames N`)
- **THEN** it SHALL NOT overwrite the saved window state

### Requirement: Live window geometry

The native window SHALL report its live client size and screen position so the state can be captured.

#### Scenario: Live size after resize
- **WHEN** the user resizes the window
- **THEN** the window size accessors SHALL report the new client size

#### Scenario: Position accessor
- **WHEN** the window has been moved
- **THEN** the window SHALL report its current screen position
