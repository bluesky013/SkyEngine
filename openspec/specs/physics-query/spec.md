# physics-query Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Raycast query

The physics world SHALL provide a raycast query that returns the closest hit against physics objects within a maximum distance and honoring a filter, using a backend-neutral result.

#### Scenario: Ray hits an object

- **WHEN** a raycast is issued along a direction that intersects a collision object within the maximum distance
- **THEN** the query SHALL report a hit with the hit position, normal, and distance

#### Scenario: Ray misses

- **WHEN** a raycast is issued and nothing intersects within the maximum distance
- **THEN** the query SHALL report no hit

#### Scenario: Filtered raycast

- **WHEN** a raycast is issued with a collision filter
- **THEN** only objects matching the filter SHALL be considered

### Requirement: Sweep query

The physics world SHALL provide a sweep query that moves a shape along a direction and reports the closest contact within a maximum distance, using a backend-neutral result.

#### Scenario: Sweep reports closest contact

- **WHEN** a sweep is issued with a shape and direction and it contacts geometry within the maximum distance
- **THEN** the query SHALL report the contact point and the distance traveled

### Requirement: Overlap query

The physics world SHALL provide an overlap query that reports the objects intersecting a shape at a given pose, honoring a filter.

#### Scenario: Overlap reports intersecting objects

- **WHEN** an overlap query is issued for a shape at a pose
- **THEN** the query SHALL report the objects that intersect the shape

### Requirement: Backend-neutral query results

All physics query results SHALL be expressed in backend-neutral engine types and SHALL NOT expose backend handles or types.

#### Scenario: Results are backend neutral

- **WHEN** a query returns a hit or an overlap
- **THEN** the returned data SHALL consist of engine types and SHALL be usable without including a backend header

### Requirement: Bullet backend implements the query API

The Bullet backend SHALL implement raycast, sweep, and overlap so the abstract query API works with the current backend.

#### Scenario: Bullet raycast matches geometry

- **WHEN** a raycast is issued against Bullet-backed collision objects
- **THEN** the Bullet implementation SHALL return a result consistent with the actual collision geometry

