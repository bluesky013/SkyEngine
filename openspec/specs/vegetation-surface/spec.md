# vegetation-surface Specification

## Purpose
TBD - created by archiving change vegetation-large-world. Update Purpose after archive.
## Requirements
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

