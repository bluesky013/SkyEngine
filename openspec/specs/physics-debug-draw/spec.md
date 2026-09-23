# physics-debug-draw Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Render-agnostic physics debug geometry

Physics debug output SHALL be expressed as plain geometry data (positions, colors, and line/triangle primitives) and SHALL NOT reference render resource types.

#### Scenario: Produce debug geometry

- **WHEN** a backend is asked for debug geometry
- **THEN** it SHALL return plain geometry data without any render type

#### Scenario: No render types in the interface

- **WHEN** the physics debug interface is inspected
- **THEN** it SHALL NOT include render headers or accept render resource handles

### Requirement: Physics core has no render dependency

The physics core module SHALL NOT include render headers or link render libraries.

#### Scenario: Core builds without render

- **WHEN** the physics core module is compiled and linked
- **THEN** it SHALL have no render include or render library dependency

#### Scenario: Backend debug output is render-free

- **WHEN** a physics backend supplies debug geometry
- **THEN** it SHALL be able to do so without linking a render adaptor

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

