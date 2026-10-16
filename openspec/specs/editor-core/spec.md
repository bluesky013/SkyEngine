# editor-core Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Render- and toolkit-independent core module
The editor core SHALL live in `engine/sandbox/core` and build the `EditorCore` target as a static library that
links only `Core` and `Framework`. It SHALL build independently of the legacy Qt editor (`engine/editor`), which
is not modified. No public header under `include/editor/core/` SHALL include an `aurora/`, `render/`, or `ui/`
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
