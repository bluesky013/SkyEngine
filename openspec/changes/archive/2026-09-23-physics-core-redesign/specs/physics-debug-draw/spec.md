## ADDED Requirements

### Requirement: Categorized debug geometry

Physics debug geometry SHALL be requestable per category (at least shapes, contacts, AABBs, constraints, and character), and the backend SHALL emit only the requested categories without leaking backend types.

#### Scenario: Request a category subset

- **WHEN** debug geometry is requested with only the contacts category enabled
- **THEN** the output SHALL contain contact geometry and SHALL NOT contain shape or AABB geometry

#### Scenario: Categories remain plain geometry

- **WHEN** any category is requested
- **THEN** the result SHALL be plain geometry data with no render resource type

### Requirement: Editor simulation debug consumption

The engine SHALL expose a contract by which the editor consumes physics debug geometry and simulation controls (play, pause, single-step, replay) without the physics core depending on the editor or render layer.

#### Scenario: Editor requests geometry

- **WHEN** the editor visualizes physics
- **THEN** it SHALL obtain plain debug geometry through the engine contract and SHALL own its rendering

#### Scenario: Editor controls simulation

- **WHEN** the editor pauses or single-steps simulation
- **THEN** it SHALL drive the engine stepper rather than the physics core depending on the editor
