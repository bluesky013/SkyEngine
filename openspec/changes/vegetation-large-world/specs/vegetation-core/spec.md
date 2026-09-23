## ADDED Requirements

### Requirement: Render-agnostic vegetation core module

The vegetation core SHALL be a standalone static module (`engine/vegetation`, target links `Framework` only) that SHALL NOT include or link any `render/`, `rhi/`, or `terrain/` header, library, or symbol, and SHALL own the biome/palette model, placement rules, density/LOD rules, per-world subsystem, and streaming state.

#### Scenario: Core builds without rendering or terrain

- **WHEN** the vegetation core module is compiled and linked
- **THEN** its only library dependency SHALL be `Framework`

#### Scenario: No render or terrain includes in core headers

- **WHEN** the vegetation core public headers are inspected
- **THEN** they SHALL NOT include paths from `engine/render`, aurora RHI, `rhi/`, or `engine/terrain`

### Requirement: Surface provider abstraction

The core SHALL define an abstract surface-provider interface supplying world-space height, slope/normal, terrain-style layer weights, cell bounds/range, and a surface-change notification, and SHALL consume the world surface only through this interface.

#### Scenario: Synthetic provider

- **WHEN** a non-terrain surface provider is supplied to the core
- **THEN** placement and streaming SHALL operate against it without code changes

#### Scenario: Surface change notification

- **WHEN** the surface provider reports that a region changed
- **THEN** the core SHALL invalidate the overlapping cells

### Requirement: Deterministic terrain-driven placement

Placement SHALL be a deterministic function of provider-sampled height, layer weights, and slope, plus authored biome density and a seed, evaluated in world space, so the same inputs produce an identical distribution independent of evaluation order.

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
