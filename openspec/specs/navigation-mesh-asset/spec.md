# navigation-mesh-asset Specification

## Purpose
TBD - created by archiving change navigation-mesh-asset-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Backend-agnostic nav mesh asset payload

`engine/navigation` SHALL define a nav mesh asset payload (`NaviMeshData`) with an export mode, build params, and
either per-tile blobs (Tiled) or a single blob (Full), containing no backend-specific types.

#### Scenario: Payload has no backend types

- **WHEN** a consumer includes the nav mesh asset header
- **THEN** it SHALL compile without any backend (recast) header

#### Scenario: Export mode is recorded

- **WHEN** a `NaviMeshData` is produced
- **THEN** it SHALL carry its `NaviMeshExportMode` and build params

### Requirement: Asset serialization

`NaviMeshData` SHALL serialize to and deserialize from a binary archive, preserving build params, per-tile
blobs (`tx`, `ty`, `layer`, bytes), and the full blob.

#### Scenario: Tiled round-trip

- **WHEN** a Tiled `NaviMeshData` with per-tile blobs is saved and loaded
- **THEN** the export mode, build params, and every tile blob SHALL be preserved

#### Scenario: Full round-trip

- **WHEN** a Full `NaviMeshData` with a single blob is saved and loaded
- **THEN** the export mode and the full blob SHALL be preserved

### Requirement: Build params consistency

The persisted build params SHALL be sufficient for the runtime to reconstruct identical Detour nav mesh and tile
cache parameters: agent config, resolution, bounds, `maxSimplificationError`, `borderSize`, and a version.

#### Scenario: Runtime rebuilds parameters from the asset

- **WHEN** the runtime loads a nav mesh asset
- **THEN** it SHALL derive `dtNavMeshParams` and `dtTileCacheParams` from the persisted build params

### Requirement: Interface-level tile emission

`NaviMeshGenerator` SHALL expose the build params and a tile-emission entry so tooling can collect a nav mesh asset
without touching backend types.

#### Scenario: Backend emits tiles behind the interface

- **WHEN** `CollectTiles` is called on a completed generation
- **THEN** it SHALL return per-tile payloads keyed by `(tx, ty, layer)` through the interface type

### Requirement: Runtime tiled load

The runtime SHALL restore a Tiled nav mesh from a `NaviMeshData`: rebuild the nav mesh and tile cache from the
build params, add the tile blobs, build the nav mesh tiles, and rebuild the query.

#### Scenario: Tiled asset restores a queryable mesh

- **WHEN** a Tiled `NaviMeshData` is loaded
- **THEN** the nav mesh and query SHALL be available for queries

#### Scenario: Full load is rejected until supported

- **WHEN** a Full `NaviMeshData` is loaded before Full support exists
- **THEN** `LoadData` SHALL report failure without crashing

