## ADDED Requirements

### Requirement: Biome authoring

The editor vegetation tool SHALL allow creating and editing biomes with their placement rules (terrain layer/splat gating, slope limits, palette selection, density) and saving them into the vegetation asset/source.

#### Scenario: Create a biome

- **WHEN** the user creates a biome and configures its rules
- **THEN** the biome SHALL be persisted in the vegetation asset/source

### Requirement: Density painting

The editor tool SHALL allow painting and erasing vegetation density on the terrain surface, storing the result in world-space density maps.

#### Scenario: Paint density

- **WHEN** the user paints density over a region
- **THEN** the density value for the affected world positions SHALL increase in the density map

#### Scenario: Erase density

- **WHEN** the user erases density over a region
- **THEN** the density value for the affected world positions SHALL decrease in the density map

### Requirement: Preview matches runtime placement

The editor preview SHALL use the same deterministic placement as runtime, so the previewed distribution matches what the runtime produces for the same inputs.

#### Scenario: Preview equals runtime

- **WHEN** the editor previews placement for a region and configuration
- **THEN** the resulting distribution SHALL match the runtime distribution for the same terrain and biome data

### Requirement: Bake

The editor tool SHALL bake the authored biome/density configuration into the vegetation asset used at runtime.

#### Scenario: Bake vegetation asset

- **WHEN** the user bakes the vegetation configuration
- **THEN** a vegetation asset SHALL be produced containing the authored biomes, palettes, and density maps
