## ADDED Requirements

### Requirement: Clipmap level geometry generation
The system SHALL generate a fixed set of geometry clipmap levels, each composed of blocks sharing a single vertex/index buffer. The number of levels and block size (vertices per side) SHALL be configurable via `ClipmapConfig`.

#### Scenario: Default clipmap initialization
- **WHEN** a terrain with default `ClipmapConfig` (blockSize=64, numLevels=8) is initialized
- **THEN** 8 clipmap levels are created, each containing ring-shaped blocks of 64×64 vertex grids, with all blocks sharing one vertex buffer and one index buffer

#### Scenario: Custom block size
- **WHEN** `ClipmapConfig.blockSize` is set to 128
- **THEN** all clipmap blocks use 128×128 vertex grids

### Requirement: Camera-centered snap-to-grid tracking
Each clipmap level SHALL track the camera position and snap its center offset to integer multiples of that level's resolution (level 0 resolution × 2^level). The snap SHALL occur per-frame during the feature processor's Tick phase.

#### Scenario: Camera moves within snap threshold
- **WHEN** the camera moves less than one grid cell at a given level
- **THEN** that level's offset does not change (no vertex swimming)

#### Scenario: Camera crosses snap boundary
- **WHEN** the camera moves beyond one grid cell at level N (cell size = baseResolution × 2^N)
- **THEN** level N's center offset snaps to the nearest grid-aligned position

### Requirement: Trim and filler strips for seamless LOD transitions
The system SHALL generate trim strips (narrow connecting geometry) between adjacent clipmap levels to eliminate gaps and T-junction cracks at level boundaries.

#### Scenario: Adjacent levels with different snap offsets
- **WHEN** level N and level N+1 have different snap offsets
- **THEN** the trim strip geometry fills the gap between the inner edge of level N+1 and the outer edge of level N without visible seams

### Requirement: Instanced block rendering
All visible blocks within a single clipmap level SHALL be rendered in a single `DrawIndexedInstanced` call. Per-block instance data (world offset, level scale, heightmap UV mapping) SHALL be uploaded to a GPU instance buffer.

#### Scenario: Clipmap level with 12 visible blocks
- **WHEN** a clipmap level has 12 blocks passing frustum culling
- **THEN** the system issues one `DrawIndexedInstanced` call with instanceCount=12

### Requirement: Configurable clipmap parameters
`ClipmapConfig` SHALL support the following configurable parameters: `blockSize` (32, 64, 128, or 256), `numLevels` (1 to 12), and `resolution` (world-space meters per vertex at level 0).

#### Scenario: Runtime configuration query
- **WHEN** the terrain is initialized with blockSize=64, numLevels=6, resolution=1.0
- **THEN** level 0 covers 64m per block at 1m/vertex, level 5 covers 64×32=2048m per block at 32m/vertex
