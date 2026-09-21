## ADDED Requirements

### Requirement: Framework-level nav mesh builder

The nav mesh builder SHALL be a `sky::AssetBuilder` registered by a navigation-owned builder module, linking only
`Framework` and `Navigation`, with no aurora dependency and no direct recast dependency.

#### Scenario: Builder links only framework and navigation

- **WHEN** the navigation builder module is built
- **THEN** it SHALL link only `Framework` + `Navigation` (and the builder static library), not aurora or recast

#### Scenario: Builder registers with the framework

- **WHEN** the module initializes
- **THEN** it SHALL register the nav mesh builder with `AssetBuilderManager` and reflect the `.navmesh` source

### Requirement: `.navmesh` source asset

A `.navmesh` source SHALL describe the scene to cook, the agent/resolution/bounds build params, and the export
mode.

#### Scenario: Source carries scene and params

- **WHEN** a `.navmesh` source is loaded
- **THEN** it SHALL provide a scene reference, agent/resolution/bounds params, and an export mode

### Requirement: Offline scene cook

The builder SHALL load the referenced scene into a `World`, attach a `NavigationSystem`, apply the authored params,
generate through the navigation interface, and write the nav mesh asset.

#### Scenario: Cook produces a nav mesh asset

- **WHEN** the builder processes a valid `.navmesh` source
- **THEN** it SHALL generate tiles through `NaviMeshFactory`/`NaviMeshGenerator` and save a nav mesh asset

#### Scenario: Missing scene does not crash

- **WHEN** the referenced scene cannot be loaded
- **THEN** the build SHALL report failure without crashing

### Requirement: Export modes

The builder SHALL support a `Tiled` export (per-tile blobs) and a `Full` export (single monolithic blob), selected
from the source.

#### Scenario: Tiled export emits per-tile payloads

- **WHEN** the export mode is `Tiled`
- **THEN** the asset SHALL contain per-tile blobs keyed by `(tx, ty, layer)`

#### Scenario: Full export emits one blob

- **WHEN** the export mode is `Full`
- **THEN** the asset SHALL contain a single serialized nav mesh blob

### Requirement: Full mesh serialization and load

The backend SHALL serialize a built `dtNavMesh` to a versioned blob and restore it, and the runtime `LoadData`
SHALL load a `Full` asset by deserialization.

#### Scenario: Full round-trip loads and queries

- **WHEN** a `Full` asset is loaded
- **THEN** the nav mesh and query SHALL be restored from the blob

#### Scenario: Invalid blob is rejected

- **WHEN** the blob version is unknown or its sizes are inconsistent
- **THEN** loading SHALL fail without crashing

### Requirement: Incremental rebuild

The generator SHALL support restricting a build to a subset of tiles, and the nav mesh SHALL support removing a
single tile from both the nav mesh and its tile cache.

#### Scenario: Subset rebuild

- **WHEN** a set of tile coordinates is provided to the generator
- **THEN** only those tiles SHALL be built

#### Scenario: Tile removal

- **WHEN** a tile is removed
- **THEN** it SHALL be removed from the nav mesh and the tile cache
