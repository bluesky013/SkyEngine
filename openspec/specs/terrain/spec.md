# terrain Specification

## Purpose

Define terrain data, generation, residency, collision and domain integration contracts.

## Requirements

### Requirement: Per-tile terrain collider construction

A terrain collision layer SHALL build one static collision object per loaded terrain tile, using that tile's full-resolution height samples and terrain metadata (height scale, height offset, resolution, and world origin), positioned at the tile's world location.

#### Scenario: Build a collider when a tile loads

- **WHEN** a terrain tile becomes loaded in the terrain sub-system
- **THEN** a static collider matching that tile's height samples SHALL be added to the physics world at the tile's world position

#### Scenario: Collision uses full resolution

- **WHEN** a collider is built for a terrain tile
- **THEN** it SHALL use the tile's full-resolution height samples and SHALL NOT depend on the render clipmap LOD

### Requirement: Collision streaming lifecycle

Terrain colliders SHALL be created and destroyed in lockstep with terrain tile load and unload, and SHALL be scoped to the owning world.

#### Scenario: Remove a collider when a tile unloads

- **WHEN** a terrain tile is unloaded from the terrain sub-system
- **THEN** its collider SHALL be removed from the physics world

#### Scenario: Two worlds collide independently

- **WHEN** two worlds each host terrain with collision
- **THEN** adding or removing a collider in one world SHALL NOT change the colliders in the other

### Requirement: Terrain sample access for non-render consumers

The terrain core SHALL expose loaded tile height samples and metadata to consumers that do not use the render layer, so that collision and navigation can consume terrain geometry without depending on rendering.

#### Scenario: Collision reads core samples

- **WHEN** the terrain collision layer builds a collider
- **THEN** it SHALL read height samples and metadata from the terrain core without referencing any render type

#### Scenario: Navigation can obtain terrain height

- **WHEN** a navigation build needs terrain geometry
- **THEN** it SHALL be able to obtain terrain height data from the terrain core without depending on the render layer

### Requirement: Runtime physics availability for terrain collision

The runtime SHALL be able to create a physics world sub-system for a world that hosts terrain, and the physics backend module SHALL be loadable at game runtime.

#### Scenario: Attach physics at runtime

- **WHEN** a world with terrain is created at runtime with collision enabled
- **THEN** a physics world sub-system SHALL be attached and terrain colliders SHALL be registered with it

#### Scenario: Physics backend module loads in game

- **WHEN** the game module configuration is loaded
- **THEN** it SHALL be able to load the physics backend module so terrain collision functions outside the editor

### Requirement: Render-agnostic terrain core module

The terrain core SHALL be provided as a standalone static module (target `Terrain` under `engine/terrain`) that links only `Framework` and SHALL NOT include or link any `render/` or `rhi/` header, library, or symbol.

#### Scenario: Core builds without rendering

- **WHEN** the terrain core module is compiled and linked
- **THEN** it SHALL produce a static target whose only library dependency is `Framework`

#### Scenario: No render includes in core headers

- **WHEN** the terrain core public headers are inspected
- **THEN** they SHALL NOT include `engine/aurora` RHI paths

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

### Requirement: Focus-driven proximity paging with hysteresis

The terrain sub-system SHALL page tiles around a settable focus position, loading tiles within a load radius and unloading tiles beyond a larger unload radius, so that the gap between the radii provides hysteresis.

#### Scenario: Load tiles near focus

- **WHEN** the streaming focus is set to a position and streaming is enabled
- **THEN** tiles within the load radius whose payloads are available in the manifest SHALL be requested for loading

#### Scenario: Unload tiles beyond the unload radius

- **WHEN** a loaded tile's distance from the focus exceeds the unload radius
- **THEN** the tile SHALL be removed from the runtime field

#### Scenario: No thrash inside the hysteresis band

- **WHEN** the focus remains between the load radius and the unload radius of an already loaded tile
- **THEN** the tile SHALL remain loaded

### Requirement: LOD-aware paging

The sub-system SHALL page tiles per terrain LOD level, where LOD `q` is loaded within an annulus bounded by a per-LOD radius that grows with `q`, and SHALL track loaded and pending state by tile coordinate and LOD so that a coordinate may hold several LODs during transitions.

#### Scenario: Load coarser LODs farther out

- **WHEN** streaming pages a tile whose distance falls in a coarser LOD's annulus
- **THEN** it SHALL request the tile at that LOD rather than always at the highest-detail LOD

#### Scenario: Near tiles use highest detail

- **WHEN** streaming pages a tile within the innermost LOD radius
- **THEN** it SHALL request LOD0

#### Scenario: Per-LOD unload

- **WHEN** a loaded (coordinate, LOD) entry leaves that LOD's unload radius
- **THEN** that entry SHALL be unloaded independently of other LODs of the same coordinate

### Requirement: Off-tick asynchronous tile prefetch

Tile payload preparation SHALL run as an asynchronous task off the world tick, and the completed payload SHALL be applied to the runtime field on the main thread within a configurable per-tick apply budget.

#### Scenario: Apply finished loads within budget

- **WHEN** more tile payloads finish prefetching than the per-tick budget allows
- **THEN** the sub-system SHALL apply at most the budget count this tick and apply the remainder on subsequent ticks

### Requirement: Pending load cancellation and payload failure tolerance

The sub-system SHALL cancel pending tile loads whose tiles move outside the unload radius before application, and SHALL tolerate missing or stale payloads without failing.

#### Scenario: Cancel a drifted pending load

- **WHEN** a pending tile's distance from the focus exceeds the unload radius before its payload is applied
- **THEN** the pending load SHALL be cancelled and the tile SHALL not be added to the field

#### Scenario: Missing payload

- **WHEN** a requested tile payload cannot be produced or is unavailable
- **THEN** the sub-system SHALL continue paging remaining tiles without error

### Requirement: Per-world tile state isolation

Each terrain sub-system instance SHALL own its tile manifest, loaded set (keyed by coordinate and LOD), and pending set, so that multiple worlds do not share terrain tile state.

#### Scenario: Two worlds page independently

- **WHEN** two worlds each host a terrain sub-system
- **THEN** loading or unloading tiles in one world SHALL NOT change the loaded tiles of the other

### Requirement: Invalidation on terrain change

When the terrain data or manifest changes, the sub-system SHALL invalidate the loaded set so that paging re-establishes the correct tiles on subsequent ticks.

#### Scenario: Invalidate after manifest change

- **WHEN** the terrain asset or manifest is replaced
- **THEN** the loaded set SHALL be cleared and tiles SHALL be re-paged against the new manifest
