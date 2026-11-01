# editor-core Specification

## Purpose

Define editor application/document ownership, project management, selection, commands and undo.

## Requirements

### Requirement: Native application shell without a UI toolkit

The editor application SHALL create its main window through the existing native window framework
(`Framework` `Platform` + `NativeWindow`, SDL-backed Win32/Cocoa) and SHALL own the event loop, without using a
UI toolkit for the application layer. The main window SHALL be created with the persisted size and position
when a valid saved window state exists, and its geometry SHALL be saved on graceful exit.

#### Scenario: Non-Qt window and event loop
- **WHEN** the editor application starts on Windows or macOS
- **THEN** it SHALL create a native window, expose its native handle, and pump events until exit, without Qt

#### Scenario: Window opens with restored geometry
- **WHEN** the editor starts and a saved window state exists
- **THEN** the created window SHALL use the saved size and position

#### Scenario: Geometry saved on exit
- **WHEN** the editor window is closed normally
- **THEN** the window geometry SHALL be written to the user's editor state before exit

### Requirement: Toolkit-independent module loading and frame driver

The application SHALL load modules from configuration and drive a per-frame tick that is independent of any UI
toolkit. When a quit has been requested (for example the main window closed), the loop SHALL stop before ticking
modules so no frame is rendered against a window that is being destroyed.

#### Scenario: Frame tick driven each loop
- **WHEN** the application main loop runs
- **THEN** it SHALL tick loaded modules each frame regardless of the UI framework

#### Scenario: Quit stops the frame before ticking
- **WHEN** a quit is requested during event pumping (for example the main window closed)
- **THEN** the loop SHALL skip the module tick/rendering for that frame and exit

### Requirement: Shell services through the platform layer

File dialogs, clipboard, and text input/IME SHALL be provided by the native platform layer. The editor menu bar
is engine-drawn and is specified by `editor-ui-shell`.

#### Scenario: File dialog returns a selection
- **WHEN** a panel requests a file through the platform layer
- **THEN** the platform SHALL present the OS file dialog and return the chosen path to the caller

#### Scenario: Text input enabled
- **WHEN** the shell starts and a text field is focused
- **THEN** text input/IME SHALL be provided by the platform layer

### Requirement: Editor host target

The editor host SHALL be started by the dedicated `SandboxEditor` executable (`engine/sandbox/app`). There is no
`Launcher --app editor` mode (the engine no longer ships a launcher). The editor host SHALL load its modules from
`configs/modules_editor.json` (falling back to `SandboxModule` when the config is absent). The `SandboxEditor` target
SHALL build with `SKY_BUILD_SANDBOX`; the core tests SHALL build with `SKY_BUILD_TEST` and run headless.

#### Scenario: Editor host builds and runs the module

- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `SandboxEditor` target SHALL build, open a non-Qt native window, and run the editor module

#### Scenario: Headless tests run

- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU

### Requirement: Editor action registry

The editor SHALL provide a toolkit-agnostic `EditorActionRegistry` where modules and plugins register
commands as `EditorAction` records (id, label, icon, toolbar group, menu / submenu, order, enabled predicate,
invoke). Registering an id again SHALL override the previous record. No UI or render type SHALL appear in the
action contract.

#### Scenario: Register and resolve
- **WHEN** a module registers an action by id and the editor invokes that id
- **THEN** the registry SHALL resolve it and run its invoke callback

#### Scenario: Override
- **WHEN** the same id is registered again
- **THEN** the later registration SHALL replace the previous one

#### Scenario: Disabled action
- **WHEN** an action declares an `enabled` predicate that is false
- **THEN** invoking it SHALL be a no-op (consumed) and any toolbar/menu view SHALL reflect it as disabled

### Requirement: Toolbar built from actions

The toolbar SHALL be built from the registry's actions that declare a toolbar group: ordered by (group, order),
grouped with visible separators, drawn as icon buttons, and disabled/dimmed per the action's `enabled`.

#### Scenario: Groups
- **WHEN** actions declare different toolbar groups
- **THEN** the toolbar SHALL show them in group order separated by divider lines

#### Scenario: Icon and tooltip
- **WHEN** an action declares an icon
- **THEN** the toolbar SHALL draw the icon (icon-only) and reveal the label as a hover tooltip

### Requirement: Menus built from actions

The editor menus SHALL be built from the registry's actions that declare a menu: top-level menus ordered by
`menuOrder`, items ordered by `order`, and items with a `submenu` nested one level deep. Host-only entries the
shell owns (e.g. Preferences) and panels (dynamic) MAY be injected by the shell.

#### Scenario: Nesting
- **WHEN** an action declares a `submenu`
- **THEN** it SHALL appear under that second-level submenu within its top-level menu

### Requirement: Shortcuts drive actions by id

Keyboard shortcuts SHALL resolve to registry actions by id (not to per-command handlers); a shortcut for a
missing/disabled action SHALL be a no-op.

#### Scenario: Save shortcut
- **WHEN** the user presses Ctrl+S and a `file.save` action is registered
- **THEN** that action's invoke SHALL run

### Requirement: Plugin extensibility

A plugin SHALL contribute toolbar/menu actions through its editor extension registration without depending on
the shell or any concrete editor type.

#### Scenario: Plugin action
- **WHEN** a plugin registers an action in its `Register()`
- **THEN** it SHALL appear in the toolbar and/or menus alongside the built-in actions

### Requirement: Cook targets from platform preset bundles

The editor SHALL resolve an asset's cook targets to the active platform's preset product bundles when
the asset declares no cook-target override and the project declares no named targets, and the Cook
action SHALL cook each of them.

#### Scenario: Target-less project
- **WHEN** an asset with no cook override is cooked and the project has no named targets
- **THEN** the cook SHALL run for each product bundle in the active platform preset (e.g. common,
  tex_pc) instead of doing nothing

#### Scenario: Asset override wins
- **WHEN** an asset declares cook targets
- **THEN** the cook SHALL run for the declared targets

### Requirement: Asset browser actions

The editor SHALL register the asset browser actions in the `EditorActionRegistry` as `EditorAction`
records with ids, labels, and enabled predicates: New Asset, Import, Rename, Move, Duplicate, Delete,
Cook/Build, Reimport, Copy Reference, Show in Explorer, and Find References, and Refresh. Mutating
actions SHALL declare an enabled predicate that is false when the selection is empty or read-only.

#### Scenario: Actions resolve by id
- **WHEN** the editor invokes an asset browser action id
- **THEN** the registry SHALL resolve it and run its invoke callback

#### Scenario: Enabled predicate drives the UI
- **WHEN** the selection is empty or only read-only assets
- **THEN** the mutating actions SHALL report disabled and invoking them SHALL be a no-op

#### Scenario: Shared implementation
- **WHEN** an asset action is triggered from the toolbar, a menu, a shortcut, or the panel context menu
- **THEN** the same registered action SHALL run

### Requirement: Shared action chrome is independent of the shell

ActionBarBuilder, ToolBar, and MenuBar SHALL be provided by EditorWidgets and SHALL be usable without linking EditorShell. They SHALL continue to consume EditorActionRegistry, scope, icon, and accelerator data and SHALL invoke actions through a host-supplied callback rather than a concrete Shell instance. Host-only menu entries and policy SHALL remain with their host.

#### Scenario: Feature consumer builds scoped chrome
- **WHEN** a consumer linked only to the shared editor framework builds toolbar/menu items for its active scope
- **THEN** the builder SHALL use that scope's registered actions without requiring Shell

#### Scenario: Action invocation reaches the supplied host
- **WHEN** a shared toolbar/menu item is activated
- **THEN** it SHALL pass the registered action id to the supplied invoker so the host can apply its normal policy

#### Scenario: Accelerator resolution stays shared
- **WHEN** a registered accelerator is resolved outside the Shell
- **THEN** the shared builder SHALL return the same scoped action id used to build its toolbar/menu entries

### Requirement: Host-bound actions reject expired owners

EditorShell's asset-viewer action callbacks SHALL remain safe when process registry entries or copied EditorAction records outlive their creating shell. The shell SHALL invalidate its owner-lifetime signal before child/member teardown. Expired-owner enabled predicates SHALL return false and invoke handlers SHALL perform no shell/viewer access. Callback invocation SHALL NOT retain the owner signal in a way that makes an already-destroyed shell appear live during reentrant dispatch.

#### Scenario: Independent viewer consumes actions after shell destruction
- **WHEN** a shell is destroyed and another viewer builds toolbar/menu state from the retained registry entries
- **THEN** enabled predicates SHALL not dereference the expired shell and its actions SHALL be inert

#### Scenario: Copied action outlives its owner
- **WHEN** a consumer retains an EditorAction copy after the creating shell is destroyed
- **THEN** evaluating its enabled predicate or invoking it directly SHALL perform no access to the expired owner

#### Scenario: Older owner dies after action replacement
- **WHEN** another live shell replaces the same scoped action IDs before the former owner is destroyed
- **THEN** former-owner teardown SHALL preserve the newer registrations and old callback copies SHALL become inert independently

#### Scenario: Live shell rebuilds its viewer tree
- **WHEN** a live shell rebuilds its owned viewer/chrome while its actions remain registered
- **THEN** action predicates SHALL observe an absent target during destruction/reconstruction and SHALL NOT dereference the previous viewer

### Requirement: Unified dynamic action state

The editor action contract SHALL expose enabled, visible, checkable and checked state independently of UI and render implementations. Toolbar, menu and accelerator dispatch SHALL consume that same state. A hidden action SHALL occupy no chrome geometry and SHALL NOT invoke. A disabled action SHALL use disabled visual styling and SHALL NOT invoke; its registered accelerator SHALL retain consumed-input semantics.

#### Scenario: Live state changes while a menu is open

- **WHEN** an action's enabled, checked or visible predicate changes and the host refreshes its chrome
- **THEN** toolbar and menu state update without reconstructing the widget tree
- **AND** a still-visible open menu remains open and hidden rows are excluded from layout and hit testing

#### Scenario: Checked and disabled drawing

- **WHEN** a checkable action is checked or an action is disabled
- **THEN** menu and toolbar CPU draw data represent the corresponding theme state
- **AND** neither pointer nor accelerator dispatch runs a disabled handler

### Requirement: Registration-safe state and dispatch

Action state resolution and invocation SHALL use copied registration snapshots with registration generation and active scope. After each predicate, the registry SHALL verify the exact registration and scope remain current before evaluating another callback or invoking a handler. A retained chrome entry SHALL NOT invoke a removed, replaced or differently scoped registration.

#### Scenario: Predicate replaces its own action

- **WHEN** an enabled, visible or checked predicate removes or replaces its registration
- **THEN** the current dispatch is rejected safely
- **AND** neither the stale handler nor the replacement handler executes through that snapshot

#### Scenario: Chrome callback outlives its registration

- **WHEN** a retained toolbar or menu callback is triggered after its action is removed, replaced or its active scope changes
- **THEN** the callback has no action effect

#### Scenario: Reentrant chrome refresh

- **WHEN** a state predicate replaces the chrome entries, recursively requests refresh or destroys its chrome owner
- **THEN** the current refresh stops or skips the recursive request without reading invalid item storage
- **AND** a surviving chrome remains refreshable on the next host boundary

### Requirement: Render- and toolkit-independent core module

The editor core SHALL live in `engine/sandbox/core` and build the `EditorCore` target as a static library that
links only `Core` and `Framework`. It SHALL build independently of the Sandbox shell and the removed legacy Qt editor. No public header under `include/editor/core/` SHALL include an `aurora/`, `render/`, or `ui/`
header, or any Qt header.

#### Scenario: Core builds without Qt
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX` or `SKY_BUILD_TEST` on and Qt absent
- **THEN** the `EditorCore` target SHALL still be configured and SHALL build

#### Scenario: Forbidden dependency rejected
- **WHEN** a public header under `include/editor/core/` includes `aurora/`, `render/`, `ui/`, or a Qt header
- **THEN** configuration SHALL fail with an explicit error naming the offending header

### Requirement: Headless core services

The core services (undo/redo, property model, documents, selection) SHALL be usable without a window, a render
device, or any UI toolkit.

#### Scenario: Services run headless
- **WHEN** a test constructs and exercises a core service without initializing the platform or a renderer
- **THEN** the service SHALL operate and return results without requiring a window or GPU

### Requirement: Toolkit-independent extension registration

Editor extensions SHALL register asset creators, actor creators, gizmo factories, and inspectors through an API
whose contracts reference no UI toolkit type.

#### Scenario: Extension registers without Qt
- **WHEN** an extension module registers a creator
- **THEN** the registration SHALL compile and link without Qt and the registered factory SHALL be retrievable by
  the editor core

### Requirement: Asset catalog consumes framework provenance

The headless asset catalog SHALL obtain each item's mount (id, display name, writable) from the
framework's recorded provenance and SHALL NOT determine it by probing filesystems or by hardcoding
mount names. The catalog's public surface SHALL NOT include filesystem or absolute-path logic.

#### Scenario: Mount from provenance
- **WHEN** the catalog lists an item
- **THEN** its mount display name and writability SHALL come from the framework mount record

#### Scenario: No probing or hardcoded names
- **WHEN** the framework gains a new mount
- **THEN** the catalog SHALL surface it without any code change or name hardcoding in core

#### Scenario: No filesystem access in core
- **WHEN** the catalog exposes structure/metadata
- **THEN** it SHALL NOT return or compute OS filesystem paths

### Requirement: Single-pass tree

The catalog SHALL build the virtual-path tree in a single pass over the source set and expose it to
views, rather than requiring views to recurse per folder.

#### Scenario: One pass
- **WHEN** the tree is requested
- **THEN** it SHALL be assembled from one traversal of the sources, in deterministic order

#### Scenario: View does not recurse
- **WHEN** a view renders the tree
- **THEN** it SHALL consume the catalog-built tree instead of calling per-folder queries

### Requirement: Structure cached, state live

The catalog SHALL cache structure and metadata and SHALL query cook/product state live, keyed by the
asset UUID together with the product bundle, so a cook/build event does not force a rebuild of the
cached structure.

#### Scenario: Build event does not rebuild structure
- **WHEN** an asset's cook state changes
- **THEN** the cached structure SHALL NOT be rebuilt and the state query SHALL return the new state

#### Scenario: Dependents on demand
- **WHEN** dependents are requested without a prior explicit refresh
- **THEN** the catalog SHALL return them (built on demand), not an empty set by default

### Requirement: Per-target cook state

The catalog SHALL track cook/product state per asset UUID and product bundle (not per UUID alone) and
SHALL expose an aggregate state for a UUID plus the per-bundle entries, so a multi-bundle cook reports
each product.

#### Scenario: Multi-bundle cook
- **WHEN** a texture is cooked into `common` and `tex_pc`
- **THEN** both targets' results SHALL be retained and the UUID's aggregate SHALL reflect failure if
  any target failed

#### Scenario: Product presence via framework
- **WHEN** the detail view needs to know which targets have a product
- **THEN** the catalog SHALL report it from the framework bundle lookup, without constructing storage
  paths itself

### Requirement: Private build listener

The catalog's public header SHALL NOT expose the framework asset-event interface; build-event
handling SHALL be an internal implementation detail.

#### Scenario: Public header is editor-only
- **WHEN** the catalog header is included by a consumer
- **THEN** it SHALL NOT require or expose the framework asset-event types

### Requirement: Default editor services are shared across DLLs

The default CommandService, PropertyEditorRegistry, and PropertyChangeNotifier returned by EditorCore SHALL have process-wide Environment-backed singleton storage even when static EditorCore code is linked into multiple DLLs. Default consumers SHALL actually use these shared instances. The service classes SHALL remain independently constructible for explicit local/headless contexts, and EditorCore SHALL remain UI-free.

#### Scenario: Modules resolve the same services
- **WHEN** the host and a separately linked dynamic module call the default service getters
- **THEN** they SHALL receive the same command service, metadata registry, and notifier objects

#### Scenario: Metadata and notifications cross the module boundary
- **WHEN** one module registers property metadata or advances the default property notifier
- **THEN** a default form/consumer in another module SHALL observe that metadata or notification

#### Scenario: Commands cross the module boundary
- **WHEN** a default form in a feature module performs an undoable property edit
- **THEN** the host's default CommandService SHALL be able to undo that edit

#### Scenario: Local services remain possible
- **WHEN** a headless test or explicit context constructs independent services
- **THEN** it SHALL be able to use them without altering the process-default services or requiring UI initialization

### Requirement: Asset catalog injection is shared across DLLs

The override set through SetEditorAssetCatalog SHALL have Environment-backed process-shared storage. A host's injected catalog SHALL be returned to widgets and feature consumers in other DLLs; clearing the override SHALL restore the existing default EditorAssetCatalog singleton lookup.

#### Scenario: Feature widget sees host catalog
- **WHEN** the host installs a catalog override and a feature module resolves the editor catalog
- **THEN** the feature module SHALL receive the installed override

#### Scenario: Clearing override restores the default
- **WHEN** the host clears the catalog override
- **THEN** all modules SHALL resolve the shared default catalog rather than retaining a per-DLL override

### Requirement: Preparation before history traversal

CommandService SHALL provide removable UI-independent preparation callbacks before every Undo or Redo attempt, including an empty stack. Callbacks SHALL run in registration order before stack selection, and the service SHALL recheck the stack afterward. Removed callbacks SHALL NOT run later in the same preparation snapshot; newly added callbacks SHALL start with the next attempt. Recursive Undo/Redo SHALL return false during the outer preparation, command execution, and change notification.

#### Scenario: Prepare a live preview before Undo
- **WHEN** a caller requests Undo while a preparation observer owns an uncommitted preview
- **THEN** preparation SHALL finish before the command restores its previous value
- **AND** subsequent interaction completion SHALL NOT overwrite the restored value

#### Scenario: Empty or changed history
- **WHEN** the selected stack is empty initially or a preparation callback clears it
- **THEN** preparation SHALL still run and traversal SHALL return false without accessing a missing command

#### Scenario: Observer lifetime and recursion
- **WHEN** a preparation observer removes another observer or requests recursive Undo/Redo
- **THEN** the removed observer SHALL be skipped and recursive traversal SHALL return false
- **AND** the outer attempt SHALL preserve its intended single traversal

### Requirement: Cleared history generations

Clearing history SHALL invalidate the generation of an executing command. After a synchronous Do/Undo/Redo callback clears or replaces the history session, the service SHALL NOT push the old command into the current session's stacks. Commands added by the replacement session SHALL be preserved. An Undo/Redo whose command executed before Clear SHALL report true even though its old generation is no longer retained.

#### Scenario: Session changes during command notification
- **WHEN** command execution synchronously clears history and optionally creates a command in a replacement session
- **THEN** the old command SHALL NOT reappear in either stack
- **AND** the replacement session's commands SHALL remain independently undoable

### Requirement: Toolkit-independent document lifecycle

The editor SHALL provide a document service, independent of any UI toolkit, that opens, loads, saves, and tracks
the dirty state of an editable document.

#### Scenario: Dirty tracking
- **WHEN** a document is modified after loading
- **THEN** the document SHALL report itself dirty, and saving SHALL clear the dirty state

#### Scenario: Load then save round-trip
- **WHEN** a document is loaded and saved without modification
- **THEN** the saved content SHALL be equivalent to the loaded content

### Requirement: Asset-to-document mapping

The service SHALL map an asset handle to its editable document so asset editors can open a document for an asset.

#### Scenario: Open asset as document
- **WHEN** an asset is opened for editing
- **THEN** the service SHALL provide the corresponding document, creating it if none exists

### Requirement: Editor startup modes

`SandboxEditor` SHALL open the Project Manager (hub) when no `--project` is given, and SHALL open the
editor bound to that project when `--project <path>` is given.

#### Scenario: No project opens the hub

- **WHEN** the editor is launched with no `--project`
- **THEN** it SHALL show the Project Manager instead of the editor shell

#### Scenario: Project opens the editor

- **WHEN** the editor is launched with `--project <path>`
- **THEN** it SHALL read the project descriptor and open the editor bound to it

### Requirement: Project descriptor

The project descriptor (`<name>.skyproj`) SHALL be the single source of project identity and settings. Creating a
project SHALL produce the directory layout **and** a compilable game source tree: `assets/`, `configs/`, `cache/`,
`<name>.skyproj`, plus `source/` (a game module skeleton) and a project build entry, with the game module
registered in `<name>.skyproj` `modules` and the project module list.

#### Scenario: Create a project

- **WHEN** a new project is requested
- **THEN** a directory with `assets/`, `configs/`, `cache/`, `source/`, a build entry, and `<name>.skyproj` SHALL be created, and the descriptor SHALL list the game module

#### Scenario: Existing projects still load

- **WHEN** an existing project without `source/` is opened
- **THEN** it SHALL still open (the scaffold is only added on creation; migration is not required)

### Requirement: Project Manager hub actions

The Project Manager hub SHALL let the user add an existing project, create a new project, open a selected
project, remove a project from the recent list, and delete a project folder. Creating a new project SHALL
use the file browser dialog in SelectDirectory mode to choose the target directory and a name; adding or
opening a project SHALL use the file browser dialog in OpenProject mode filtered to `*.skyproj`.

#### Scenario: New project chooses a directory
- **WHEN** the user activates New Project
- **THEN** the file browser dialog SHALL open in SelectDirectory mode, and on accept the project SHALL be
  created at the chosen directory with the entered name

#### Scenario: Add project chooses a file
- **WHEN** the user activates Add Project
- **THEN** the file browser dialog SHALL open in OpenProject mode, and on accept the chosen `*.skyproj`
  SHALL be validated and added to the recent list

#### Scenario: Invalid or cancelled selection
- **WHEN** the user cancels the dialog or selects an invalid project
- **THEN** no project SHALL be created or added, and the hub SHALL remain unchanged

### Requirement: Recent projects persistence

The recent-project list SHALL be persisted under the user config path and restored on the next launch.

#### Scenario: Recent list survives restart

- **WHEN** a project is added and the editor is relaunched
- **THEN** the project SHALL appear in the recent list

### Requirement: Engine-version validation

On open/add the editor SHALL compare the project's `engineVersion` with the running engine and SHALL
refuse a project newer than the engine, allowing an older one with a warning.

#### Scenario: Newer project refused

- **WHEN** the project's engine version is newer than the engine
- **THEN** the open/add SHALL be refused with a message

### Requirement: Single-instance hard lock

The editor SHALL hold a hard lock per project (`<project>/cache/editor.lock`) and SHALL refuse a second
editor on the same project while the owner process is alive; a stale lock (owner gone) SHALL be
reclaimed.

#### Scenario: Second instance refused

- **WHEN** a second editor opens a project already held by a live process
- **THEN** it SHALL refuse and exit

#### Scenario: Stale lock reclaimed

- **WHEN** the lock's owner process is gone
- **THEN** a new editor SHALL reclaim the lock and open

### Requirement: Project asset mounting

Opening a project SHALL mount the engine bundle assets read-only and the workspace `assets/` writable
(the workspace shadowing the engine), and SHALL set the runtime work filesystem to the project directory.

#### Scenario: Workspace shadows engine

- **WHEN** the same asset path exists in the workspace and the engine bundle
- **THEN** the workspace asset SHALL resolve

### Requirement: Console control

The editor SHALL run as a GUI-subsystem application with no console by default and SHALL attach a
console (stdout/stderr) when `--console` is passed.

#### Scenario: Default no console

- **WHEN** the editor is launched without `--console`
- **THEN** no console window SHALL open

#### Scenario: `--console` attaches a console

- **WHEN** the editor is launched with `--console`
- **THEN** a console SHALL be attached and logs SHALL appear on it

### Requirement: No standalone preview window

The editor SHALL NOT open a separate standalone preview window by default (the preview is a docked panel
in the target design).

#### Scenario: No preview window

- **WHEN** the editor opens (hub or a project)
- **THEN** no separate preview window SHALL be created

### Requirement: Observable editor selection

The editor SHALL expose the current selection (world/entities/assets) and the active editor context as a service
independent of any UI toolkit, allowing views to subscribe and be notified of changes.

#### Scenario: Subscribers notified on change
- **WHEN** the selection changes
- **THEN** all subscribed views SHALL be notified of the new selection

#### Scenario: Selection is read without a toolkit
- **WHEN** the selection is queried in a headless test
- **THEN** it SHALL return the current selection without requiring a window or UI toolkit

### Requirement: Single owner of selection

The selection SHALL be owned by the service; the outliner, inspector, and viewport SHALL observe it rather than
holding independent copies.

#### Scenario: Two views stay consistent
- **WHEN** the outliner changes the selection
- **THEN** the inspector and viewport SHALL observe the same updated selection

### Requirement: Toolkit-independent command service

Undo/redo SHALL be provided by a service in `EditorCore` whose contract references no UI-toolkit type. It SHALL
support executing undoable commands and undoing/redoing them.

#### Scenario: Execute then undo
- **WHEN** an undoable command is executed and then undone
- **THEN** the affected state SHALL return to its value before the command

#### Scenario: Redo reapplies
- **WHEN** a redo follows an undo
- **THEN** the command SHALL be reapplied

### Requirement: Transaction grouping

The service SHALL group multiple commands into a single user-visible undo step via transactions.

#### Scenario: Transaction is one step
- **WHEN** several commands execute inside one transaction and the transaction ends
- **THEN** a single undo SHALL revert all of them

#### Scenario: Can-undo reflects state
- **WHEN** no command has been executed
- **THEN** `CanUndo` SHALL report false, and after a command it SHALL report true

### Requirement: Reflection-driven property edits

Property edits SHALL be recorded through a generic command generated from the reflection model, capturing the
object handle, member path, and the old and new values, without per-type command code.

#### Scenario: Property undo restores old value
- **WHEN** a reflected property is changed through the property model and then undone
- **THEN** the property SHALL return to its previous value using the captured old value

### Requirement: Change notification

The service SHALL notify observers when the undo/redo stacks change so views can update enabled state.

#### Scenario: Observer notified after edit
- **WHEN** a command is executed, undone, or redone
- **THEN** registered observers SHALL be notified

### Requirement: Responsive project creation with progress

The project manager SHALL run project creation outside the UI thread and SHALL display the current reported stage and elapsed time while creation is pending. Conflicting project actions SHALL be disabled, and Quit SHALL remain available.

#### Scenario: A slow project creator
- **WHEN** the user confirms a new project and the creator is still running
- **THEN** the picker callback SHALL return without waiting for creation
- **AND** the hub SHALL continue processing input and rendering progress
- **AND** duplicate creation and opening another project SHALL be prevented

#### Scenario: Stage updates arrive across pipe reads
- **WHEN** the creator emits a supported progress marker, including split reads or CRLF
- **THEN** the hub SHALL display the corresponding real stage without inventing a percentage

### Requirement: Bounded project creation lifecycle

Creation SHALL drain stdout and stderr without blocking the UI, retain bounded diagnostics, and terminate on timeout or shutdown. UI and registry updates SHALL occur only on the main thread.

#### Scenario: Creator fails or stalls
- **WHEN** Python cannot start, exits unsuccessfully, or exceeds the creation timeout
- **THEN** the hub SHALL restore project actions and display the failure reason
- **AND** the recent-project registry SHALL remain unchanged

#### Scenario: Shutdown during creation
- **WHEN** the user exits while creation is pending
- **THEN** the module SHALL cancel the process and join its worker before destruction or unload

### Requirement: Validated project creation result

The hub SHALL register creation only after successful process exit and validation of the generated descriptor. The creator SHALL receive paths as separate arguments.

#### Scenario: Successful temporary project with special characters
- **WHEN** a new project name contains spaces and shell metacharacters
- **THEN** the creator SHALL generate the descriptor, native templates and asset directories at the requested location
- **AND** the hub SHALL register that exact descriptor and show a completion message

#### Scenario: Invalid creation result
- **WHEN** the creator exits successfully without a valid named descriptor
- **THEN** the hub SHALL report failure without registering the project
