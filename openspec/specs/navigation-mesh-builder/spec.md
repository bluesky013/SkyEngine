# navigation-mesh-builder Specification

## Purpose
TBD - created by archiving change navigation-mesh-builder. Update Purpose after archive.
## Requirements
### Requirement: Framework-level nav mesh builder

The nav mesh builder SHALL be a `sky::AssetBuilder` registered by a navigation-owned plugin builder module. Its implementation SHALL link `Framework`, `Navigation` and the plugin's shared source/asset owner through `NavigationBuilder.Static`. It SHALL NOT link runtime, editor, render, Aurora or concrete Recast backend implementation targets. The cook host SHALL load the backend separately and generation SHALL use navigation interfaces. Source reflection SHALL have one backend metadata owner, separate from builder registration.

#### Scenario: Builder links framework, navigation and shared schema

- **WHEN** the navigation builder module is built
- **THEN** `SkyNavigation.Builder` SHALL link `NavigationBuilder.Static`, whose dependencies SHALL be `Framework`, `Navigation` and `RecastNavigation.Shared.Static`, without linking `RecastNavigation.Runtime.Static`, `RecastNavigation.Static`, Aurora or the recast third-party

#### Scenario: Builder registers with the framework

- **WHEN** the backend and builder module definitions are created
- **THEN** the backend metadata owner SHALL reflect the `.navmesh` source once, and the builder definition SHALL register its `sky::AssetBuilder` with `AssetBuilderManager` without repeating source reflection

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

### Requirement: Plugin-owned offline cook and failure reporting

Source schema and cook implementation SHALL belong to the navigation plugin. Cooking SHALL consume engine contracts/shared asset code without linking editor/render targets. Its host SHALL load the backend dependency. Failed generation SHALL return an explicit error and SHALL NOT publish an asset.

#### Scenario: Generation failure

- **WHEN** generation fails after work completes
- **THEN** cooking SHALL report its diagnostic and SHALL NOT save a success product

### Requirement: Explicit offline world preparation

Before collecting source geometry, the offline builder SHALL require every attached component to report completed world preparation. Deferred asset or native-shape attachment SHALL have explicit Ready, Preparing and Failed states. Preparation and binding failures SHALL identify the actor, component and cause, and SHALL prevent navigation product publication.

#### Scenario: Collision resources prepare asynchronously

- **WHEN** the source world's actor attachment is accepted while collision resources are still preparing
- **THEN** the builder SHALL finish preparation before gathering geometry and SHALL NOT publish an empty success product merely because those resources were pending

#### Scenario: Collision product or native binding fails

- **WHEN** a required ProductsOnly collision input is unavailable or native body binding fails
- **THEN** cooking SHALL fail with the affected actor/component diagnostic and SHALL NOT treat the omitted geometry as ready

### Requirement: Cook preparation avoids worker self-wait

The builder SHALL advance preparation through a bounded number of explicit stages. A cook worker MAY drain the independent loader pool, but SHALL NOT wait for its own cook pool. Readiness SHALL NOT depend on guessed sleeps or advancing world simulation.

#### Scenario: Prepare from the cook worker

- **WHEN** a cook request needs loader or native shape-preparation work
- **THEN** it SHALL wait only for loader work and continue polling preparation without waiting on the request itself

#### Scenario: Preparation does not converge

- **WHEN** the source world remains Preparing after the configured stage bound
- **THEN** cooking SHALL return an explicit preparation failure instead of hanging or publishing incomplete geometry

### Requirement: Real scene geometry survives the cook boundary

Valid walkable collision triangles in a source world SHALL contribute to the persisted navigation product after resource preparation and decoding.

#### Scenario: Cook a world with walkable ground and an obstacle

- **WHEN** valid collision products provide walkable ground and a blocking obstacle to a `.navmesh` build
- **THEN** the cooked mesh SHALL contain navigation geometry and a loaded runtime path SHALL respect that obstacle

