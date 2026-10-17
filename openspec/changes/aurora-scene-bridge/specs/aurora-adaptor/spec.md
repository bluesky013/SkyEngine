## ADDED Requirements

### Requirement: One-way scene hand-off

The aurora adaptor SHALL transfer framework scene state (actor identity, component data, transform) through owned FramePackets submitted to IRenderer. Only the renderer coordinator SHALL apply them to scene ECS at its Apply boundary. The bridge SHALL NOT access renderer stores directly or retain borrowed World/component pointers in packets. The transfer SHALL be one-way; aurora SHALL NOT write scene state back into framework components.

#### Scenario: Component state reaches the scene ECS

- **WHEN** an actor with an aurora component (for example a static mesh) is attached to a world
- **THEN** the bridge SHALL enqueue an owned create command and the coordinator SHALL create the scene ECS entry when applying the accepted packet

#### Scenario: Submission is busy

- **WHEN** IRenderer returns Busy for a sealed packet
- **THEN** the bridge SHALL retain its contents for bounded retry, without losing lifecycle commands or directly mutating ECS

#### Scenario: No write-back

- **WHEN** the bridge has transferred state
- **THEN** it SHALL NOT read aurora ECS state back into framework components

#### Scenario: Source objects disappear after packet sealing

- **WHEN** the World or component is destroyed after its values have been copied into a sealed packet
- **THEN** retry or renderer application SHALL use only the owned values and generation-qualified identities, without dereferencing the destroyed source

### Requirement: Render state stays in aurora

Render state SHALL live only in aurora (scene ECS and feature processors). Framework component persisted data SHALL carry only plain data, `Uuid`, and engine values.

#### Scenario: Persisted data remains render-free

- **WHEN** an aurora component's serialized data is inspected
- **THEN** it SHALL NOT contain aurora/render implementation types

### Requirement: Lifecycle-driven create and remove

The bridge SHALL enqueue scene create commands on framework component attach and remove commands on detach, driven by lifecycle events. Generation-qualified IDs SHALL NOT be reused before renderer retirement acknowledgement.

#### Scenario: Detach removes the scene entry

- **WHEN** a bridged component is detached from its actor or world
- **THEN** the bridge SHALL enqueue removal and the coordinator SHALL remove the entry at the corresponding Apply boundary

#### Scenario: Late event after detach and reattach

- **WHEN** an old component or resource event arrives after detach and a new attachment has acquired a new generation or binding revision
- **THEN** the bridge SHALL reject the stale event, retain old identity tombstones until retirement acknowledgement and leave the new attachment unchanged

### Requirement: Transform transfer uses the corrected world transform

The bridge SHALL transfer the component's world transform, so the aurora scene receives correct world placement for parented actors.

#### Scenario: Parented actor world transform

- **WHEN** an actor is parented and its ancestor transform changes
- **THEN** the aurora scene SHALL receive the updated world transform for that actor
