## ADDED Requirements

### Requirement: LOD0 residency for nav builds

Navigation builds SHALL require terrain LOD0 samples for the affected region; a nav tile build SHALL be deferred and retried when the required terrain LOD0 is not yet resident.

#### Scenario: Defer when LOD0 missing

- **WHEN** a nav tile build is requested but its overlapping terrain LOD0 tiles are not resident
- **THEN** the build SHALL be deferred and retried on a later tick once the terrain LOD0 is resident

#### Scenario: Never use render LOD

- **WHEN** a nav mesh is built for a terrain region
- **THEN** it SHALL use terrain LOD0 geometry and SHALL NOT use the render clipmap LOD

### Requirement: Incremental rebuild on terrain change

When a terrain tile's data changes, the navigation system SHALL invalidate and rebuild only the nav tiles overlapping that terrain tile, using the existing incremental rebuild path.

#### Scenario: Rebuild affected nav tiles

- **WHEN** a terrain tile is edited, regenerated, or otherwise marked changed
- **THEN** the overlapping nav tiles SHALL be invalidated and rebuilt, and unaffected nav tiles SHALL be left intact

#### Scenario: No rebuild for unchanged streamed data

- **WHEN** terrain LOD0 becomes resident for a region whose data matches what nav was built from
- **THEN** no nav rebuild SHALL be triggered for that region

### Requirement: Deterministic and cacheable builds

Terrain-driven nav mesh builds SHALL be deterministic for the same terrain data and build configuration, so results can be cached and reproduced.

#### Scenario: Reproducible output

- **WHEN** the same terrain data and build configuration are used to build a nav tile twice
- **THEN** the resulting tile payload SHALL be identical
