# terrain-navigation-geometry Specification

## Purpose
TBD - created by archiving change terrain-navigation-integration. Update Purpose after archive.
## Requirements
### Requirement: Terrain LOD0 geometry provider

A terrain geometry provider SHALL implement the navigation geometry-provider interface and emit triangles derived from terrain core LOD0 tile samples, without depending on the render layer or the physics heightfield.

#### Scenario: Emit terrain triangles

- **WHEN** the terrain provider collects geometry for a bounds that overlaps resident terrain LOD0 tiles
- **THEN** it SHALL emit two world-space triangles per terrain quad over the overlapped region

#### Scenario: No render or physics dependency

- **WHEN** the terrain provider module is compiled
- **THEN** it SHALL depend only on the terrain core and the navigation provider interface, not on render or physics types

### Requirement: World-space triangle generation

Terrain triangles SHALL be produced in world space using the terrain metadata addressing and height decoding, with consistent upward winding so the nav mesh builder derives correct walkable slopes.

#### Scenario: Correct world positions

- **WHEN** a terrain quad is emitted as triangles
- **THEN** each vertex position SHALL equal the tile world origin plus the local texel offset times the resolution, with height from the decoded LOD0 sample

#### Scenario: Consistent winding

- **WHEN** terrain triangles are emitted
- **THEN** their winding SHALL face the terrain's up direction so recast computes walkable slope from the correct normal

### Requirement: Build-region clipping at quad granularity

The provider SHALL clip emitted geometry to the requested bounds and map nav build tiles to terrain tiles by world-space overlap, sampling only the needed sub-rect of each terrain tile.

#### Scenario: Partial tile overlap

- **WHEN** a nav build tile overlaps only part of a terrain tile
- **THEN** the provider SHALL emit only the terrain quads that overlap the requested bounds

#### Scenario: Independent tessellation sizes

- **WHEN** the nav tile size differs from the terrain tile size
- **THEN** the provider SHALL still map the nav tile's world bounds onto the correct terrain tiles and sub-rects

### Requirement: Missing LOD0 handling

The provider SHALL skip terrain tiles whose LOD0 data is not resident and SHALL report that the requested region was not fully covered, so the caller can defer the nav tile build.

#### Scenario: Non-resident LOD0

- **WHEN** a requested region overlaps a terrain tile whose LOD0 is not resident
- **THEN** the provider SHALL omit that tile's geometry and report incomplete coverage

