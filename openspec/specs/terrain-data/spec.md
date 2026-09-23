# terrain-data Specification

## Purpose
TBD - created by archiving change terrain-large-world-core. Update Purpose after archive.
## Requirements
### Requirement: Tiled terrain asset with manifest

The terrain asset SHALL carry terrain metadata plus a tile manifest describing every available tile by coordinate, world bounds, and height range, and SHALL allow addressable access to individual tile payloads by tile coordinate.

#### Scenario: Address a tile by coordinate

- **WHEN** a tile payload is requested for a coordinate present in the manifest
- **THEN** the asset SHALL return that tile's payload

#### Scenario: Missing tile coordinate

- **WHEN** a tile payload is requested for a coordinate absent from the manifest
- **THEN** the asset SHALL report that the tile is unavailable

### Requirement: Height tile payload with scale and offset

The terrain asset SHALL store per-tile height samples in a declared height format (16-bit unsigned normalized or 32-bit float) and SHALL interpret a stored sample as world height using `sample * heightScale + heightOffset`.

#### Scenario: Interpret a stored height sample

- **WHEN** a stored height sample is converted to world height
- **THEN** the result SHALL equal the sample value multiplied by the metadata height scale and added to the metadata height offset

#### Scenario: Preserve height format

- **WHEN** a terrain asset is saved and loaded
- **THEN** the declared height format SHALL be preserved and samples SHALL be interpreted with the same format

### Requirement: Per-tile multi-level LOD payloads

The terrain asset SHALL store a LOD chain per tile, where LOD level `L` stores `tileSize >> L` quads per side down to a minimum, so that far clipmap rings can load a coarse payload instead of a full-resolution one.

#### Scenario: Address a tile LOD

- **WHEN** a tile payload is requested for a coordinate and a LOD level present in the asset
- **THEN** the asset SHALL return that LOD's payload

#### Scenario: LOD resolution halves per level

- **WHEN** LOD level `L` of a tile is read, for `L > 0`
- **THEN** its vertex resolution SHALL be half of LOD level `L - 1`

#### Scenario: Coarser LOD uses fewer samples

- **WHEN** a LOD level coarser than LOD0 is stored
- **THEN** it SHALL store fewer samples than LOD0

#### Scenario: Manifest records LOD availability

- **WHEN** the tile manifest is read
- **THEN** each tile entry SHALL describe which LOD levels are available for that tile

### Requirement: Splatmap tile payloads

The terrain asset SHALL store per-tile material layer weights as splatmap tiles that pack four layers per RGBA tile, sharing the height tile grid, so that the number of splatmap tiles per terrain tile is `ceil(layerCount / 4)`.

#### Scenario: Four layers per splatmap tile

- **WHEN** a terrain with more than four layers is created
- **THEN** the asset SHALL store additional splatmap tiles per terrain tile such that each RGBA tile carries at most four layer weights

### Requirement: Terrain asset serialization round-trip

The terrain asset SHALL support binary serialization such that saving and reloading preserves metadata, the tile manifest, and all tile payloads without loss.

#### Scenario: Round-trip terrain asset

- **WHEN** a terrain asset is serialized and then deserialized
- **THEN** the reloaded metadata, manifest, and tile payload contents SHALL match the original

### Requirement: Logic-only terrain component data model

The terrain component SHALL hold only plain data (terrain metadata, terrain asset or source identifier, material identifier, layer definitions, and streaming parameters) and SHALL NOT declare or reference any render resource type.

#### Scenario: Reflect terrain component

- **WHEN** the terrain component is serialized and restored
- **THEN** all of its configuration SHALL round-trip as plain data and asset identifiers

### Requirement: Hole / no-data tiles

The terrain manifest SHALL mark tiles that have no surface data (holes) via a `hasData` flag, and streaming SHALL NOT load hole tiles so that queries and collision report no data there.

#### Scenario: Hole tile is not loaded

- **WHEN** a manifest entry marks a tile as a hole
- **THEN** streaming SHALL NOT load that tile and terrain queries at its location SHALL report no data

#### Scenario: Hole survives round-trip

- **WHEN** a terrain asset with a hole tile is serialized and reloaded
- **THEN** the hole marking SHALL be preserved

