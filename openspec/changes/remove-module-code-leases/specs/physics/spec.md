## MODIFIED Requirements

### Requirement: SDK state and executable code outlive physics objects

The backend and every live world SHALL retain the SDK state they use; defining DLL code SHALL remain process resident without per-object code tokens. Unregistering a backend SHALL prevent new world creation but SHALL NOT destroy SDK state still needed by existing worlds. The last user SHALL release jobs, native objects and callbacks before releasing SDK state. The SDK initialization guard SHALL belong to the actual native static Jolt image whose globals it initializes; independently linked images SHALL NOT share an initialization guard. Native SDK symbols and private backend types/templates SHALL remain bound to their owning image. Public Physics interfaces and process-shared registries SHALL remain visible, with one metadata owner; this change SHALL NOT add a target or public SDK lifecycle API.

#### Scenario: Registry is unregistered before a world is released
- **WHEN** a backend is unregistered while a created world is still owned by a consumer
- **THEN** that world SHALL retain the state and code needed for safe access and destruction, and a new registry creation request SHALL fail

#### Scenario: World is torn down during shutdown
- **WHEN** the last world is destroyed
- **THEN** all jobs SHALL be joined and constraints, characters, bodies, shapes and listeners SHALL be released before SDK teardown

#### Scenario: Independently linked native images use Jolt
- **WHEN** an executable and a module contain separate static Jolt images
- **THEN** each native image SHALL initialize and retain its own SDK globals for its live users
- **AND** the public physics registry SHALL remain process-shared and component/config metadata SHALL still have one definition owner

#### Scenario: Native and private implementation symbols could be coalesced
- **WHEN** the platform linker can coalesce weak SDK header or private backend template symbols across an executable and module
- **THEN** symbol visibility SHALL keep those implementations, allocations and SDK state within their owning native image
- **AND** the public Physics interface and process-shared registry SHALL remain available across module boundaries

### Requirement: Single active backend registration

The engine registry SHALL maintain exactly one active backend at a time. Registering a new backend SHALL unregister the previous factory and release the registry's ownership of it without invalidating existing worlds' owned SDK state; defining DLL code SHALL remain process resident. World destruction SHALL use its creating owner's lifetime contract, not whichever backend is currently active.

#### Scenario: Replace the active backend
- **WHEN** a new backend is registered while one is active
- **THEN** the previous backend SHALL stop accepting creation, while outstanding worlds SHALL remain safely destructible

### Requirement: Serialization implementation lives in the plugin

Reflection registration and asset/component serialization for physics SHALL be implemented in the physics plugin, while engine descriptor structs and their field names SHALL remain backend-neutral. Metadata SHALL be registered only by the unique module definition owner and SHALL rely on process-resident DLL code for persistent callbacks, retaining any captured data through actual owners. Runtime Init/Shutdown SHALL NOT repeat reflection registration.

#### Scenario: Engine stays implementation-free
- **WHEN** the engine physics module is inspected
- **THEN** it SHALL define descriptors and field names but SHALL NOT perform reflection registration or asset serialization

#### Scenario: Plugin registers serialization
- **WHEN** the physics module definition is created
- **THEN** its metadata owner SHALL register the physics components, descriptors and config types once

#### Scenario: Runtime is restarted
- **WHEN** the same module definition begins a new runtime session
- **THEN** its existing metadata SHALL be reused without duplicate reflection or an idempotent registration guard
