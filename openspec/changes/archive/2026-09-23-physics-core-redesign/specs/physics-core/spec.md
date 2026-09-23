## ADDED Requirements

### Requirement: Engine physics module is interface/data/utility-only

The `engine/physics` module SHALL contain only interfaces, plain-data descriptors, and pure utilities; it SHALL NOT contain the runtime implementation (world sub-system, components, backend, or registration).

#### Scenario: Engine module has no implementation

- **WHEN** `engine/physics` is compiled and inspected
- **THEN** it SHALL expose interfaces, descriptors, and pure utilities only, with no runtime physics implementation

#### Scenario: Engine does not depend on a plugin

- **WHEN** `engine/physics` is built
- **THEN** it SHALL NOT include or link a physics plugin's implementation, and SHALL link only engine interface libraries

### Requirement: Stable physics object handles

The physics module SHALL address every runtime object (body, character, constraint) by a stable `PhysicsObjectId` scoped to a physics world, independent of backend objects and raw pointers.

#### Scenario: Adding an object returns a handle

- **WHEN** an object is added to a physics world
- **THEN** the world SHALL return a `PhysicsObjectId` that resolves that object for the lifetime of the world

#### Scenario: Handles guard against reuse

- **WHEN** an object is removed and a later object is created
- **THEN** the removed object's id SHALL NOT resolve to the new object

### Requirement: World-owned object lifetime

A physics world SHALL own the lifetime of the objects created through it; consumers SHALL reference objects by `PhysicsObjectId` and SHALL NOT hold backend object pointers.

#### Scenario: Consumer holds a handle, not a pointer

- **WHEN** a consumer adds a body and later reads it
- **THEN** the consumer SHALL use the returned `PhysicsObjectId` and SHALL NOT retain a backend pointer

#### Scenario: Removal invalidates the handle

- **WHEN** an object is removed or the world is destroyed
- **THEN** its id SHALL become invalid and resolving it SHALL fail safely rather than access freed memory

### Requirement: Physics is resolved through the engine interface

Consumers SHALL resolve the physics implementation through the engine `PhysicsRegistry` or the world sub-system, never through a backend's concrete type.

#### Scenario: Consumer resolves at runtime

- **WHEN** a consumer needs a physics world
- **THEN** it SHALL obtain it through the engine registry/interface and SHALL NOT include a backend header

#### Scenario: Physics absent is handled

- **WHEN** no physics backend is registered
- **THEN** consumers SHALL treat physics as unavailable rather than fail to build or crash

### Requirement: Object kinds are addressable by handle

The physics world SHALL allow bodies, characters, and constraints to be addressed uniformly by `PhysicsObjectId`, and SHALL expose typed accessors to read or modify each kind.

#### Scenario: Typed access by handle

- **WHEN** a caller has a body id and requests body state
- **THEN** the world SHALL return the body's state, and requesting state for an id of another kind SHALL fail safely
