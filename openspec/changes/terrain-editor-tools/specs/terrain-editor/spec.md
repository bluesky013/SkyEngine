## ADDED Requirements

### Requirement: Editor terrain creation

The editor terrain tool SHALL create a terrain actor with a terrain component configured from the selected terrain metadata, replacing the previously disabled creation path.

#### Scenario: Create terrain from the Build tab

- **WHEN** the user activates the terrain build tool and confirms creation
- **THEN** an actor with a terrain component carrying the selected metadata SHALL be added to the current world

### Requirement: Tile grid authoring

The editor terrain grid tool SHALL allow adding and removing individual terrain tiles for a selected terrain component, and the resulting tile set SHALL be persisted in the terrain asset/component.

#### Scenario: Add a tile

- **WHEN** the user selects a grid cell in add mode and confirms
- **THEN** the corresponding tile SHALL be added to the terrain tile set

#### Scenario: Remove a tile

- **WHEN** the user selects an existing tile in remove mode and confirms
- **THEN** the corresponding tile SHALL be removed from the terrain tile set

### Requirement: Generator correctness

The editor terrain generator SHALL use the configured generation seed, SHALL emit height samples in the height format declared by the terrain metadata, and SHALL generate splatmap tiles alongside heightmap tiles.

#### Scenario: Reproducible generation

- **WHEN** the same generation seed is used twice for the same tile
- **THEN** the generated height data SHALL be identical

#### Scenario: Height format follows metadata

- **WHEN** the terrain metadata declares a height format
- **THEN** the generated height tiles SHALL be stored in that format

#### Scenario: Splatmaps generated

- **WHEN** terrain is generated for a terrain with defined layers
- **THEN** splatmap tiles SHALL be produced and registered together with the heightmap tiles

### Requirement: Single terrain generator implementation

The terrain plugin SHALL have exactly one terrain generation implementation; the orphaned duplicate generator target SHALL be removed and the stale quadtree-named test SHALL be replaced by core-focused tests.

#### Scenario: No duplicate generator target

- **WHEN** the terrain plugin targets are inspected
- **THEN** there SHALL be no orphaned or unused generator target

### Requirement: Reserved sculpt and paint seam

The editor terrain tooling SHALL expose a documented extension seam for future sculpt and paint tools without implementing them in this change.

#### Scenario: Seam present and inert

- **WHEN** the terrain editor tool is loaded
- **THEN** a sculpt/paint extension interface SHALL be present and SHALL not alter terrain data until implemented
