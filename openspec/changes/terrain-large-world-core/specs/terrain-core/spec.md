## ADDED Requirements

### Requirement: Render-agnostic terrain core module

The terrain core SHALL be provided as a standalone static module (target `Terrain` under `engine/terrain`) that links only `Framework` and SHALL NOT include or link any `render/` or `rhi/` header, library, or symbol.

#### Scenario: Core builds without rendering

- **WHEN** the terrain core module is compiled and linked
- **THEN** it SHALL produce a static target whose only library dependency is `Framework`

#### Scenario: No render includes in core headers

- **WHEN** the terrain core public headers are inspected
- **THEN** they SHALL NOT include paths from `engine/render`, `engine/aurora` RHI, or `rhi/`

### Requirement: World-space tile addressing

The core SHALL define a terrain metadata type containing tile vertex size, meters-per-vertex resolution, height format, height scale, height offset, tile counts, world origin, and the per-tile LOD level count, and SHALL provide deterministic conversion between world positions and tile coordinates and local texel offsets.

#### Scenario: World position to tile coordinate

- **WHEN** a world position is converted with valid terrain metadata
- **THEN** the result SHALL identify the tile containing that position and the local texel offset within the tile

#### Scenario: Tile coordinate to world origin

- **WHEN** a tile coordinate is converted back to world space
- **THEN** the resulting origin SHALL map back to the same tile coordinate for all positions inside that tile

#### Scenario: LOD geometry from metadata

- **WHEN** the vertex size or sample count for a LOD level is requested from the metadata
- **THEN** the core SHALL return the level's halved resolution derived from the tile vertex size and LOD count

### Requirement: Terrain runtime field and system lifecycle

The core SHALL provide a render-agnostic runtime terrain field object that holds loaded tile samples, and a terrain world sub-system implementing the engine world sub-system lifecycle that owns the field and the tile manifest for one world.

#### Scenario: Attach terrain to a world

- **WHEN** a terrain component is attached to a world that has a terrain sub-system
- **THEN** the component SHALL push its metadata and asset references to the terrain sub-system without referencing any render type

#### Scenario: Detach terrain from a world

- **WHEN** the terrain component is detached from the world
- **THEN** the terrain sub-system SHALL release the field state associated with that component

### Requirement: CPU sampling and query API

The core SHALL expose CPU queries, resolved against loaded tile data, for terrain height at a world position, terrain normal at a world position, splatmap layer weights at a world position, and a ray intersection returning hit position, normal, and distance.

#### Scenario: Height query between texels

- **WHEN** a height query is issued for a world position that lies between stored height texels
- **THEN** the returned height SHALL be interpolated from the neighboring texel values and scaled by the metadata height scale and offset

#### Scenario: Normal query from finite differences

- **WHEN** a normal query is issued for a loaded world position
- **THEN** the returned normal SHALL be a unit vector derived from the local height gradient

#### Scenario: Raycast against loaded terrain

- **WHEN** a ray is tested against loaded terrain within the maximum distance
- **THEN** the query SHALL report a hit with position, normal, and distance, and SHALL report no hit when the ray misses all loaded tiles

#### Scenario: Query outside loaded tiles

- **WHEN** a query is issued for a position in a tile whose payload is not loaded
- **THEN** the query SHALL indicate that no terrain data is available rather than returning fabricated data

### Requirement: Clipmap LOD description as data

The core SHALL describe terrain LOD/clipmap parameters (level count, per-level resolution and scale, tile/block vertex size, and the tile LOD each level maps to) as plain data without allocating GPU buffers or referencing render types.

#### Scenario: Retrieve level description

- **WHEN** a render layer requests the terrain LOD description from the core
- **THEN** the core SHALL return per-level resolution, extent, and the desired tile LOD as plain values
