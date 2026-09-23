# terrain-collision Specification

## Purpose
TBD - created by archiving change terrain-large-world-core. Update Purpose after archive.
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

