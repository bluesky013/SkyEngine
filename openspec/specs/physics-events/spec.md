# physics-events Specification

## Purpose
TBD - created by archiving change physics-core-redesign. Update Purpose after archive.
## Requirements
### Requirement: Contact event stream

The physics world SHALL emit contact events for pairs of objects whose interaction begins, persists, or ends across a step, and SHALL deliver them as an ordered stream drained by the consumer after the step.

#### Scenario: Contact begins

- **WHEN** two objects begin touching during a step
- **THEN** a contact-begin event SHOULD be delivered containing both `PhysicsObjectId` values

#### Scenario: Contact ends

- **WHEN** two objects that were touching separate
- **THEN** a contact-end event SHALL be delivered for that pair

### Requirement: Trigger events

The physics world SHALL support trigger volumes that report enter and exit for other objects without applying collision response.

#### Scenario: Trigger enter and exit

- **WHEN** an object enters and later leaves a trigger volume
- **THEN** the stream SHALL contain a trigger-enter event followed by a trigger-exit event for that pair

#### Scenario: Trigger does not block motion

- **WHEN** a dynamic body passes through a trigger volume
- **THEN** the trigger SHALL NOT apply collision response to the body

### Requirement: Backend-neutral, deterministic event payload

Event payloads SHALL contain only engine types (object ids, contact point, normal, and impulse where applicable) and SHALL be ordered deterministically so replay produces the same stream.

#### Scenario: Payload has no backend types

- **WHEN** events are inspected
- **THEN** they SHALL contain engine types only and SHALL be usable without a backend header

#### Scenario: Ordering is reproducible

- **WHEN** the same simulation is run twice from the same initial state
- **THEN** the delivered event stream SHALL have the same order

### Requirement: Events carry no render dependency

The physics event interface SHALL NOT include render headers or render resource types.

#### Scenario: Render-free events

- **WHEN** a consumer includes the physics event interface
- **THEN** it SHALL not be required to include or link a render module

