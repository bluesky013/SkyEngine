## ADDED Requirements

### Requirement: Nav mesh from terrain LOD0

An end-to-end build SHALL construct a nav mesh from terrain LOD0 geometry through the recast backend, producing a walkable surface over flat terrain and a correctly bounded walkable surface over sloped terrain (steep areas excluded by the agent's max slope).

#### Scenario: Flat terrain is walkable

- **WHEN** a nav mesh is built from flat terrain LOD0
- **THEN** the resulting nav mesh SHALL contain a walkable surface covering the built region

#### Scenario: Slope limits walkability

- **WHEN** a nav mesh is built from terrain LOD0 that includes a slope steeper than the agent's maximum slope
- **THEN** the steep region SHALL NOT be part of the walkable surface

### Requirement: Combined geometry sources

A nav mesh build SHALL combine terrain-provided geometry with collision-component geometry so that both sources contribute to the resulting nav mesh.

#### Scenario: Terrain and collision both contribute

- **WHEN** a build runs with a terrain geometry provider and collision-component meshes present
- **THEN** the resulting nav mesh SHALL reflect geometry from both sources

#### Scenario: Terrain-only build unaffected

- **WHEN** a build runs with only the terrain provider and no collision geometry
- **THEN** the resulting nav mesh SHALL match a terrain-only build
