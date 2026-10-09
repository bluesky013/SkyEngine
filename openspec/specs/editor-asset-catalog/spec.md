# editor-asset-catalog Specification

## Purpose
TBD - created by archiving change editor-asset-browser. Update Purpose after archive.
## Requirements
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

