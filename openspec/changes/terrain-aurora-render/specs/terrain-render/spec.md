## ADDED Requirements

### Requirement: Aurora terrain render feature processor

The aurora render layer SHALL provide a terrain feature processor that participates in the aurora scene lifecycle, pulls terrain render data from the core terrain sub-system each frame, and owns all terrain GPU resources.

#### Scenario: Register terrain feature

- **WHEN** the aurora terrain render module is initialized
- **THEN** a terrain feature processor SHALL be available to aurora scenes without the core terrain module referencing any render type

#### Scenario: Consume core terrain data

- **WHEN** the feature processor updates
- **THEN** it SHALL obtain terrain metadata, the visible tile set, and tile payloads from the core terrain sub-system

### Requirement: Per-LOD GPU heightmap and splatmap atlas

The render layer SHALL build and maintain per-LOD GPU atlases of terrain heightmap and splatmap tiles from the core tile payloads, updating them as tiles and their LODs are streamed in and out.

#### Scenario: Upload a newly loaded tile LOD

- **WHEN** a terrain tile LOD becomes loaded in the core sub-system
- **THEN** the render layer SHALL make that LOD's height and splat data available in the corresponding LOD atlas

#### Scenario: Release an unloaded tile LOD

- **WHEN** a terrain tile LOD is unloaded from the core sub-system
- **THEN** the render layer SHALL release or reuse that LOD's atlas region

#### Scenario: Far ring samples a coarse LOD

- **WHEN** a clipmap ring maps to a coarse tile LOD
- **THEN** the render layer SHALL sample that LOD's atlas for the ring's blocks

### Requirement: Clipmap LOD geometry and instanced rendering

The render layer SHALL own the terrain LOD geometry (clipmap block mesh, per-level layout, snap-to-grid camera tracking) and SHALL render visible blocks using instanced draws within the aurora scene.

#### Scenario: Render clipmap around the camera

- **WHEN** a scene with terrain is rendered
- **THEN** the render layer SHALL draw the clipmap levels centered on the camera using instanced block draws

### Requirement: LOD crack stitching

The render layer SHALL stitch seams between adjacent clipmap LOD levels and between adjacent tiles rendered at different LOD, using a skirt-based stitch by default and SHALL support vertex geomorphing as an optional smoothing technique, so that differing vertex spacing does not produce visible cracks or abrupt popping.

#### Scenario: Skirt stitch at LOD boundaries

- **WHEN** two adjacent LOD levels with different vertex spacing meet
- **THEN** the render layer SHALL hide the seam with a downward skirt generated along the block edges

#### Scenario: Optional geomorph smoothing

- **WHEN** geomorphing is enabled
- **THEN** coarse-level vertices SHALL blend toward the finer neighbor's height so the transition does not pop

#### Scenario: Tile LOD adjacency

- **WHEN** two adjacent tiles are rendered at different LODs
- **THEN** their shared edge SHALL be stitched with the same technique used at clipmap ring boundaries

#### Scenario: Stitch does not affect query or collision

- **WHEN** a LOD seam is stitched for rendering
- **THEN** the terrain core data, CPU queries, and collision SHALL be unaffected and remain LOD-independent

### Requirement: Data-level seam continuity

The terrain core SHALL ensure adjacent tiles are seam-free at the data level by sampling continuous fields in world space and sharing boundary samples, so render stitching only has to handle geometry LOD transitions.

#### Scenario: Shared boundary samples

- **WHEN** two adjacent tiles are generated independently
- **THEN** their shared boundary samples SHALL be equal

#### Scenario: Sampling across tile borders

- **WHEN** a query or finite difference crosses a tile border
- **THEN** it SHALL produce a continuous result (no seam) using the shared boundary samples

### Requirement: Terrain material and technique binding

The render layer SHALL bind the terrain material and techniques (color, depth, shadow) to the terrain draws, including height scale/offset, layer colors, and splatmap sampling.

#### Scenario: Bind terrain material

- **WHEN** the terrain draws are recorded
- **THEN** the configured terrain material and its techniques SHALL be bound with the terrain material parameters

### Requirement: Render-agnostic core handoff

The core terrain module SHALL NOT reference the aurora render layer or any GPU type; the handoff from core to render SHALL be plain data and identifiers only.

#### Scenario: Core has no aurora dependency

- **WHEN** the core terrain module is linked without the aurora render layer
- **THEN** it SHALL compile and link successfully

### Requirement: GPU-driven terrain node

The render layer SHALL support a GPU-driven path where per-block/per-tile visibility, LOD selection, instance compaction, and indirect draw arguments are computed on the GPU from CPU-provided residency and LOD data; the core SHALL provide an incremental residency change (added/removed tile-LOD entries) so GPU residency data updates incrementally.

#### Scenario: GPU selects and draws blocks

- **WHEN** a GPU-driven terrain frame is rendered
- **THEN** block/tile culling and LOD selection SHALL run on the GPU and produce indirect draw arguments, without the CPU iterating per block or per instance

#### Scenario: Incremental residency update

- **WHEN** streaming adds or removes tile-LOD entries
- **THEN** the render layer SHALL update the GPU resident tile data incrementally from the core's residency change delta

#### Scenario: CPU-driven fallback

- **WHEN** a backend lacks compute/indirect support (or the fallback is selected)
- **THEN** the render layer SHALL render terrain via the CPU-driven path

#### Scenario: GPU-driven does not affect core

- **WHEN** terrain is rendered GPU-driven
- **THEN** core data, CPU queries, and collision SHALL be unaffected (LOD independence)
