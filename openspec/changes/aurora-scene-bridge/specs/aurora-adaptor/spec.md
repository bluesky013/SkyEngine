## ADDED Requirements

### Requirement: One-way scene hand-off

The aurora adaptor SHALL transfer framework scene state (actor identity, component data, transform) into the aurora scene ECS. The transfer SHALL be one-way; aurora SHALL NOT write scene state back into framework components.

#### Scenario: Component state reaches the scene ECS

- **WHEN** an actor with an aurora component (for example a static mesh) is attached to a world
- **THEN** the bridge SHALL create the corresponding aurora scene ECS entry with the component's data

#### Scenario: No write-back

- **WHEN** the bridge has transferred state
- **THEN** it SHALL NOT read aurora ECS state back into framework components

### Requirement: Render state stays in aurora

Render state SHALL live only in aurora (scene ECS and feature processors). Framework component persisted data SHALL carry only plain data, `Uuid`, and engine values.

#### Scenario: Persisted data remains render-free

- **WHEN** an aurora component's serialized data is inspected
- **THEN** it SHALL NOT contain aurora/render implementation types

### Requirement: Lifecycle-driven create and remove

The bridge SHALL create aurora scene entries on framework component attach and remove them on detach, driven by lifecycle events.

#### Scenario: Detach removes the scene entry

- **WHEN** a bridged component is detached from its actor or world
- **THEN** the bridge SHALL remove the corresponding aurora scene entry

### Requirement: Transform transfer uses the corrected world transform

The bridge SHALL transfer the component's world transform, so the aurora scene receives correct world placement for parented actors.

#### Scenario: Parented actor world transform

- **WHEN** an actor is parented and its ancestor transform changes
- **THEN** the aurora scene SHALL receive the updated world transform for that actor
