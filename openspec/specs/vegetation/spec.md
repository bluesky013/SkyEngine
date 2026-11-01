# vegetation Specification

## Purpose

Define vegetation data, placement, surfaces, residency and domain integration contracts.

## Requirements

### Requirement: Render-agnostic vegetation core module

The vegetation core SHALL be a standalone static module (`engine/vegetation`, target links `Framework` only) that SHALL NOT include or link any `render/`, `rhi/`, or `terrain/` header, library, or symbol, and SHALL own the biome/palette model, placement rules, density/LOD rules, per-world subsystem, and streaming state.

#### Scenario: Core builds without rendering or terrain

- **WHEN** the vegetation core module is compiled and linked
- **THEN** its only library dependency SHALL be `Framework`

#### Scenario: No render or terrain includes in core headers

- **WHEN** the vegetation core public headers are inspected
- **THEN** they SHALL NOT include aurora RHI or `engine/terrain` paths

### Requirement: Surface provider abstraction

The core SHALL define an abstract surface-provider interface supplying world-space height, slope/normal, terrain-style layer weights, cell bounds/range, and a surface-change notification, and SHALL consume the world surface only through this interface.

#### Scenario: Synthetic provider

- **WHEN** a non-terrain surface provider is supplied to the core
- **THEN** placement and streaming SHALL operate against it without code changes

#### Scenario: Surface change notification

- **WHEN** the surface provider reports that a region changed
- **THEN** the core SHALL invalidate the overlapping cells

### Requirement: Deterministic terrain-driven placement

Placement SHALL be a deterministic function of provider-sampled height, layer weights, and slope, plus density maps and a seed, evaluated in world space, so the same inputs produce an identical distribution independent of evaluation order.

#### Scenario: Reproducible distribution

- **WHEN** placement is evaluated twice with the same surface data, biome data, and seed
- **THEN** the resulting vegetation distribution SHALL be identical

#### Scenario: Provider surface is authoritative

- **WHEN** placement samples the world surface
- **THEN** it SHALL use the provider's highest-detail surface data and SHALL NOT use a render LOD

### Requirement: Biome, palette, and density rules

The core SHALL model biomes (with placement rules), species/palette entries (mesh or billboard, scale/rotation jitter, wind response, density), and density rules that modulate instance density by biome, terrain layer/splat weights, slope, and distance.

#### Scenario: Layer and slope gating

- **WHEN** a biome rule restricts vegetation to specific terrain layers or a maximum slope
- **THEN** placement SHALL only emit instances where the terrain splat weights and slope satisfy the rule

#### Scenario: Distance density

- **WHEN** a cell is evaluated at a distance within a density LOD band
- **THEN** the emitted density SHALL be reduced according to that band's rule

### Requirement: Vegetation subsystem lifecycle

The core SHALL provide a vegetation world sub-system implementing the engine world sub-system lifecycle that owns the biome configuration, placement, density/LOD rules, and streaming state for one world.

#### Scenario: Attach vegetation to a world

- **WHEN** a vegetation component is attached to a world that has a vegetation sub-system
- **THEN** the component SHALL push its biome configuration to the sub-system without referencing any render type

#### Scenario: Detach vegetation from a world

- **WHEN** the vegetation component is detached from the world
- **THEN** the sub-system SHALL release the placement/streaming state for that component

### Requirement: Render-agnostic handoff and presence queries

The core SHALL expose plain-data render handoff (cell bounds, biome/palette parameters, density data reference, terrain height reference) and vegetation presence/density queries, without referencing render types.

#### Scenario: Render consumes plain data

- **WHEN** the render layer requests vegetation data for a cell
- **THEN** the core SHALL return plain data and identifiers only

#### Scenario: Gameplay density query

- **WHEN** a caller queries vegetation presence or density at a world position
- **THEN** the core SHALL answer from its biome/density rules without a render dependency

### Requirement: Vegetation asset with biomes, palettes, and density

The vegetation asset SHALL carry a biome set (biome definitions + placement rules), species/palette definitions (mesh/billboard reference, scale/rotation jitter, wind response, density), and per-biome density (distribution) maps, with addressable access by biome.

#### Scenario: Address a biome

- **WHEN** a biome is requested by id from a vegetation asset that contains it
- **THEN** the asset SHALL return that biome's rules, palette, and density data

#### Scenario: Missing biome

- **WHEN** a biome is requested by an id absent from the asset
- **THEN** the asset SHALL report that the biome is unavailable

### Requirement: Density map payload

Density maps SHALL be stored as addressable payloads sampled in world space, so placement can read density at a world position independently of the cell partition.

#### Scenario: World-space sampling

- **WHEN** a density value is read for a world position
- **THEN** it SHALL be derived from the density payload by world-space addressing

### Requirement: Vegetation asset serialization round-trip

The vegetation asset SHALL support binary serialization such that saving and reloading preserves biomes, palettes, density payloads, and rules without loss.

#### Scenario: Round-trip vegetation asset

- **WHEN** a vegetation asset is serialized and then deserialized
- **THEN** the reloaded biomes, palettes, density payloads, and rules SHALL match the original

### Requirement: Logic-only vegetation component

The vegetation component SHALL hold only plain data (vegetation asset/source identifier, biome set id, seed, density/LOD parameters, streaming parameters) and SHALL NOT declare or reference any render resource type.

#### Scenario: Reflect vegetation component

- **WHEN** the vegetation component is serialized and restored
- **THEN** all of its configuration SHALL round-trip as plain data and asset identifiers

### Requirement: Instances

The vegetation asset SHALL carry an instance list (position, rotation, scale, biome, species); cells whose area contains instances SHALL use them in place of procedural placement, while cells without instances continue procedural placement.

#### Scenario: Instances override procedural placement

- **WHEN** a cell contains instances
- **THEN** that cell SHALL emit the instances instead of procedurally generated ones

#### Scenario: Procedural placement for cells without instances

- **WHEN** a cell contains no instances
- **THEN** that cell SHALL emit procedurally generated instances

#### Scenario: Instances round-trip

- **WHEN** a vegetation asset with instances is serialized and reloaded
- **THEN** the instances SHALL be preserved

### Requirement: Focus-driven cell paging with hysteresis

The vegetation sub-system SHALL page vegetation cells around a settable focus position, loading cells within a load radius and unloading cells beyond a larger unload radius, so the gap provides hysteresis, with cell bounds supplied by the surface provider (aligned to terrain tiles when the provider is terrain).

#### Scenario: Load cells near focus

- **WHEN** the streaming focus is set and streaming is enabled
- **THEN** cells within the load radius whose terrain surface is resident SHALL be requested for loading

#### Scenario: Unload cells beyond the radius

- **WHEN** a loaded cell's distance from the focus exceeds the unload radius
- **THEN** the cell SHALL be unloaded

#### Scenario: No thrash inside the hysteresis band

- **WHEN** the focus remains between a cell's load and unload radii
- **THEN** the cell SHALL remain loaded

### Requirement: Per-cell distance density LOD

Each cell SHALL select a density LOD band from its distance to the focus, reducing instance density with distance, with fades between bands so transitions are not visually abrupt.

#### Scenario: Density decreases with distance

- **WHEN** a cell is at a distance in a coarser density LOD band
- **THEN** its emitted density SHALL be lower than at a nearer band

#### Scenario: Fade between bands

- **WHEN** a cell's distance places it near a band boundary
- **THEN** the transition SHALL be blended rather than a hard change

### Requirement: Off-tick prefetch and per-tick budget

Cell input preparation SHALL run as an asynchronous task off the world tick, and completed cells SHALL be applied on the main thread within a configurable per-tick budget.

#### Scenario: Apply within budget

- **WHEN** more cells finish preparing than the per-tick budget allows
- **THEN** the sub-system SHALL apply at most the budget count this tick and the remainder on later ticks

### Requirement: Per-world isolation and invalidation

Each vegetation sub-system instance SHALL own its cells and placement state, and terrain tile changes SHALL invalidate the overlapping vegetation cells.

#### Scenario: Two worlds page independently

- **WHEN** two worlds each host a vegetation sub-system
- **THEN** loading or unloading cells in one world SHALL NOT change the cells of the other

#### Scenario: Invalidate on terrain change

- **WHEN** a terrain tile changes
- **THEN** the overlapping vegetation cells SHALL be invalidated and re-established from the updated surface

### Requirement: Surface provider interface

The vegetation core SHALL define an abstract surface-provider interface exposing world-space height sampling, slope/normal sampling, layer-weight sampling, cell bounds/range for streaming, and surface-change notifications, using only engine-core types.

#### Scenario: Sample the surface

- **WHEN** placement or streaming queries the provider for a world position or region
- **THEN** the provider SHALL return height, slope/normal, and layer weights for that input

#### Scenario: Cell bounds from the provider

- **WHEN** streaming needs the cells covering a region
- **THEN** the provider SHALL supply the cell bounds/range for that region

#### Scenario: Change notification

- **WHEN** a region's surface data changes
- **THEN** the provider SHALL notify registered listeners so overlapping cells can be invalidated

### Requirement: Terrain surface bridge

A terrain surface bridge (linking `Terrain` + `Vegetation`) SHALL implement the surface provider using terrain LOD0 data and the terrain tile addressing, so terrain is one pluggable surface implementation.

#### Scenario: Terrain-backed provider

- **WHEN** the terrain bridge is registered for a world with terrain
- **THEN** the vegetation core SHALL sample height, slope, and layer weights from terrain LOD0 through the provider

#### Scenario: Cells align to terrain tiles

- **WHEN** the terrain bridge supplies cell bounds
- **THEN** the cells SHALL align to terrain tiles

#### Scenario: Terrain changes invalidate cells

- **WHEN** a terrain tile changes
- **THEN** the bridge SHALL raise a surface-change notification for the affected region

### Requirement: Provider is optional and replaceable

The core SHALL operate with any provider implementation and SHALL NOT require the terrain bridge, so vegetation can run against a synthetic or alternative surface.

#### Scenario: No terrain bridge present

- **WHEN** no terrain bridge is registered but another provider is supplied
- **THEN** the core SHALL still place and stream vegetation

#### Scenario: No provider present

- **WHEN** no provider is registered
- **THEN** the core SHALL report that the surface is unavailable rather than fabricating data
