## ADDED Requirements

### Requirement: Deterministic seeded generation

Procedural terrain generation SHALL be deterministic: for a given seed, generation configuration, and tile coordinate, the generated height, splatmap, and LOD chain SHALL be identical regardless of generation order or parallelism.

#### Scenario: Same seed produces identical output

- **WHEN** the same seed and configuration are used to generate the same tile twice
- **THEN** the generated height, splatmap, and LOD payloads SHALL be byte-identical

#### Scenario: Order independence

- **WHEN** the same tiles are generated in different orders or concurrently
- **THEN** each tile's output SHALL be identical to the output produced in any other order

### Requirement: World-space continuous fields

Generation SHALL sample its continuous fields in world coordinates using the seed, so that adjacent tiles are seamless and no tile depends on another tile's result.

#### Scenario: Seamless tile borders

- **WHEN** two adjacent tiles are generated independently
- **THEN** the height values along their shared border SHALL match

#### Scenario: No cross-tile dependency

- **WHEN** a single tile is generated in isolation
- **THEN** it SHALL produce the same border values as when generated alongside its neighbors

### Requirement: Layered noise height configuration

Generation SHALL derive height from a configurable layered noise stack including seed, octave count, frequency, amplitude, lacunarity, and gain, with optional domain warp and ridged layers.

#### Scenario: Apply the noise configuration

- **WHEN** a generation configuration with multiple noise octaves is used
- **THEN** the resulting height field SHALL combine the octave contributions according to the configured frequency, amplitude, lacunarity, and gain

#### Scenario: Height format follows metadata

- **WHEN** the terrain metadata declares a height format and scale/offset
- **THEN** generated height samples SHALL be stored in that format and decode to world height via the declared scale and offset

### Requirement: Rule-derived splatmap generation

Generation SHALL produce splatmap tiles by mapping terrain properties (at least slope and height, optionally biome/layer rules) to four-layer RGBA weights.

#### Scenario: Slope and height drive weights

- **WHEN** a tile is generated for a terrain with defined layers
- **THEN** the generated splatmap SHALL assign layer weights from the configured slope and height rules for each sample

#### Scenario: Weights are normalized

- **WHEN** a splatmap sample is generated
- **THEN** its four layer weights SHALL sum to a normalized range suitable for the RGBA8 tile format

### Requirement: Per-tile LOD chain generation

Generation SHALL build the full-resolution LOD0 payload and then derive each coarser LOD by downsampling LOD0, averaging height and renormalizing splat weights.

#### Scenario: Coarse LOD derived from LOD0

- **WHEN** a tile's LOD chain is generated
- **THEN** each coarser LOD SHALL be produced by downsampling the next-finer LOD so that coarse samples remain consistent with LOD0

#### Scenario: LOD chain matches metadata

- **WHEN** generation completes for a tile
- **THEN** it SHALL produce exactly the LOD levels declared by the terrain metadata, with each level's resolution halving per level

### Requirement: Offline and on-demand generation modes

The same generation implementation SHALL be usable offline (asset bake) and on demand (runtime streaming), producing identical output in both modes.

#### Scenario: Offline bake

- **WHEN** the offline builder runs over a source configuration
- **THEN** it SHALL produce a terrain asset whose tiles carry the generated height, splatmap, and LOD chain

#### Scenario: On-demand generation

- **WHEN** streaming requests a tile and LOD that is not present in the cooked asset and generation is enabled
- **THEN** the generator SHALL produce that tile LOD on demand using the same algorithm as the offline bake

### Requirement: Render-free off-thread generation

The generation algorithm SHALL live in the render-agnostic terrain core, SHALL NOT reference render or RHI types, and SHALL run off the main thread.

#### Scenario: Generation has no render dependency

- **WHEN** the generation module is compiled without the render layer
- **THEN** it SHALL build and run successfully

#### Scenario: Generation runs off-tick

- **WHEN** a tile is generated
- **THEN** the work SHALL run as a background task and the result SHALL be applied on the main thread