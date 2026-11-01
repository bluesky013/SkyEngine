## MODIFIED Requirements

### Requirement: SDK state and executable code outlive physics objects

The backend and every live world SHALL retain the SDK state they use; defining DLL code SHALL remain process resident without per-object code tokens. Unregistering a backend SHALL prevent new world creation but SHALL NOT destroy SDK state still needed by existing worlds. The last user SHALL release jobs, native objects and callbacks before releasing SDK state. The SDK initialization guard SHALL belong to the actual native static Jolt image whose globals it initializes; independently linked images SHALL NOT share an initialization guard. Native SDK symbols and private backend types/templates SHALL remain bound to their owning image. Public Physics interfaces and process-shared registries SHALL remain visible, with one metadata owner; this change SHALL NOT add a target or public SDK lifecycle API.

#### Scenario: Registry is unregistered before a world is released
- **WHEN** a backend is unregistered while a created world is still owned by a consumer
- **THEN** that world SHALL retain the state and code needed for safe access and destruction, and a new registry creation request SHALL fail

#### Scenario: World is torn down during shutdown
- **WHEN** the last world is destroyed
- **THEN** all jobs SHALL be joined and constraints, characters, bodies, shapes and listeners SHALL be released before SDK teardown
