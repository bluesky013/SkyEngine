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

### Requirement: Typed object handles in query results

Every physics query result SHALL identify the hit or overlapping object with a `PhysicsObjectId` that resolves for every object kind (body, character, or trigger), and SHALL NOT expose a backend object pointer.

#### Scenario: Rigid body is resolved

- **WHEN** a raycast hits a dynamic rigid body
- **THEN** the result SHALL carry a `PhysicsObjectId` that resolves to that body rather than a null object

#### Scenario: Overlap reports every kind

- **WHEN** an overlap query intersects bodies of different kinds
- **THEN** the result SHALL include each intersecting object addressed by its handle

### Requirement: Filtered queries skip and continue

A filtered query SHALL skip candidates that do not satisfy the filter and continue the search to the next candidate, returning the closest acceptable result rather than reporting no hit.

#### Scenario: Closest hit is filtered out

- **WHEN** the closest candidate is rejected by the query filter and a farther candidate is acceptable
- **THEN** the query SHALL return the farther acceptable candidate

#### Scenario: No acceptable candidate

- **WHEN** every candidate within range is rejected by the filter
- **THEN** the query SHALL report no hit

### Requirement: Deterministic query ordering

Query results SHALL be ordered deterministically (by ascending hit distance and then by object handle) so that repeated queries on the same world state return the same order.

#### Scenario: Repeatable overlap order

- **WHEN** an overlap query returns multiple objects
- **THEN** the returned order SHALL be the same across repeated queries on the same world state

