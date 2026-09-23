## ADDED Requirements

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
