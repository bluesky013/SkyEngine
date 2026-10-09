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

