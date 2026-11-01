# physics Specification

## Purpose

Define physics backend integration, simulation, bodies, queries, serialization and runtime assets.

## Requirements

### Requirement: Jolt is the sole shipped physics provider

SkyEngine SHALL ship Jolt as its only built-in physics provider. Runtime source, dependency manifests, plugin options, module templates, generated module lists and packaged artifacts SHALL NOT depend on or load Bullet. The migration SHALL NOT retain a Bullet alias module, forwarding header or dual-provider selection path.

#### Scenario: Default engine configuration
- **WHEN** the default engine and game-template configurations are generated
- **THEN** they SHALL enable `SKY_BUILD_JOLT` and reference `JoltPhysicsModule` without any Bullet target or dependency

#### Scenario: Physics is disabled
- **WHEN** `SKY_BUILD_JOLT` is disabled for a supported host or package configuration
- **THEN** that configuration SHALL build without loading a physics provider and consumers SHALL handle physics as unavailable

#### Scenario: Old generated binaries exist
- **WHEN** a previously built workspace is migrated and packaged
- **THEN** its active module list and package SHALL exclude old Bullet modules even if unrelated shared third-party caches still contain Bullet

### Requirement: Pinned and ABI-consistent Jolt dependency

The third-party manifest SHALL pin Jolt v5.6.0 to revision `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, use the existing bootstrap and publish matching Debug/Release artifacts. Import definitions SHALL match the SDK's ABI-affecting build options. The delivered configuration SHALL use float precision, disable C++ exceptions, and preserve the SDK license notice. RTTI SHALL be configured for the target platform and match the consuming C++ ABI rather than being unconditionally disabled; the macOS and Android Clang SDKs and consumers SHALL enable compatible RTTI, while an existing platform override SHALL remain consistent with its consumers.

#### Scenario: Rebuild from an existing dependency checkout
- **WHEN** the Jolt dependency is rebuilt from a stale checkout
- **THEN** the existing version-pinning mechanism SHALL select the pinned revision before configuration and compilation

#### Scenario: Consumer links the dependency
- **WHEN** the backend is compiled against an installed Debug or Release SDK
- **THEN** its CRT, assertion, profiling, debug-renderer, ObjectLayer and SIMD settings SHALL match that SDK

#### Scenario: Platform consumer requires RTTI
- **WHEN** a supported platform's C++ consumers require SDK RTTI symbols, including macOS Clang
- **THEN** the SDK and consumers SHALL use matching RTTI settings while C++ exceptions remain disabled
- **AND** changing these settings SHALL require rebuilding the affected SDK artifacts and consumers

#### Scenario: Android NDK consumer links the arm64 SDK
- **WHEN** Android NDK Clang consumers link the Debug or Release arm64-v8a Jolt SDK
- **THEN** the Android manifest override SHALL enable SDK RTTI to match those consumers while keeping C++ exceptions disabled
- **AND** both configurations SHALL be rebuilt and checked for the selected ABI and emitted RTTI symbols before package/runtime validation

### Requirement: Physics layering and metadata ownership survive replacement

`engine/physics` SHALL remain free of plugin and Jolt types. SDK implementation, Framework integration and module entry SHALL live together in the Jolt plugin adaptor. Native headers SHALL remain private to adaptor implementation. The plugin SHALL NOT introduce a separate core target without independent domain logic. Aurora asset conversion SHALL remain an independent render bridge; neither implementation target SHALL depend on the other. The module definition SHALL be the sole owner of physics component/config metadata.

#### Scenario: Consumer resolves physics
- **WHEN** terrain, navigation, editor or a physics component needs the physics world
- **THEN** it SHALL resolve the engine interface through the established `Physics` subsystem contract without accessing a concrete backend world

#### Scenario: Runtime restarts
- **WHEN** the module runtime is stopped and started again within the same process definition
- **THEN** services SHALL be registered symmetrically and metadata SHALL NOT be registered again or hidden behind a duplicate-registration guard

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

### Requirement: Cleanup preserves history and unrelated work

The migration SHALL delete active Bullet implementation and configuration, update current usage instructions, and preserve historical archives and dated technology comparisons as historical evidence. It SHALL NOT erase unrelated working-tree changes or shared third-party caches outside the workspace.

#### Scenario: Final source and packaging audit
- **WHEN** the migration is accepted
- **THEN** active source/build/runtime/package scans SHALL have no Bullet dependency, and any remaining textual reference SHALL be historical or a migration record

### Requirement: Migration preserves the delivered Fast capability boundary

The Jolt backend SHALL advertise Fast with `deterministic=false` until a separate cross-platform validation delivers the existing Exact contract. It SHALL reject Exact requests and SHALL NOT describe a state snapshot as a complete solver rollback image.

#### Scenario: A project requests Exact
- **WHEN** a project selects Exact or enables the deterministic world setting in this migration
- **THEN** initialization SHALL fail with an explicit unsupported-mode result rather than creating a Fast world silently

### Requirement: World assembly reports failures before runtime publication

World loading SHALL construct only registered component types and keep loaded actors detached until subsystem validation and construction succeed. Component dependencies and attachment failures SHALL participate in the construction result. Document loading and Play SHALL reject failed candidates and preserve the current document. Duplicate subsystem names SHALL NOT replace an existing registration owner.

#### Scenario: Missing provider or failed configuration
- **WHEN** an enabled subsystem is unregistered, rejects its config or fails to initialize
- **THEN** World construction SHALL return a reason, publish no partially built runtime and allow a corrected candidate to be retried

#### Scenario: Collision product fails during attachment
- **WHEN** a loaded body's mesh product cannot be resolved
- **THEN** actor attachment and newly attached subsystems SHALL be rolled back and Play SHALL remain in editing state

#### Scenario: Reflected value is not a component
- **WHEN** a scene or creation request names the UUID of a reflected non-component type
- **THEN** Actor SHALL reject it through ComponentFactory without casting an untyped allocation to ComponentBase

### Requirement: Editor configuration and creation use shared undoable contracts

World configuration edits SHALL take effect in the next newly constructed Play world. Undo/Redo SHALL resolve the current configuration by document lifetime, subsystem name and type rather than retaining inline Any storage addresses. Component requirements SHALL be declared in ComponentFactory. Actor presets SHALL be supplied through EditorActorCreation and exposed through EditorActionRegistry.

#### Scenario: Config storage moves or the document reloads
- **WHEN** another subsystem expands the configuration vector
- **THEN** configuration Undo/Redo SHALL operate on the current config object
- **AND WHEN** the document reloads
- **THEN** old commands SHALL be rejected without accessing replaced storage

#### Scenario: Add a component with subsystem requirements
- **WHEN** a registered component requires Physics or another registered subsystem
- **THEN** the editor SHALL add the component and enable those requirements as one undoable operation, or reject the operation if a required provider is unavailable

#### Scenario: Plugin adds and removes an actor preset
- **WHEN** a creator is registered or unregistered
- **THEN** its menu action SHALL appear or disappear without a host enum/switch change
- **AND** outstanding creation commands SHALL retain the actor and callback code needed for Undo/Redo

### Requirement: Collision mesh consumption matches the cooked asset contract

The Aurora bridge SHALL honor vertex stride, POSITION offset/format, U16/U32 indices and submesh ranges in cooked products. Collision-only mesh references and compound children SHALL be included in typed World assetRoots and checked by the package product auditor.

#### Scenario: Interleaved cooked mesh
- **WHEN** a mesh product contains normals/UVs alongside POSITION and either supported index width
- **THEN** collision geometry SHALL use only the declared positions and correct indices, and malformed ranges SHALL fail explicitly

#### Scenario: Missing collision-only product
- **WHEN** a World references a mesh only through a collider and its selected cooked bundle lacks that product
- **THEN** package auditing SHALL fail rather than omitting the reference

### Requirement: Backend lifecycle hooks

The physics backend factory SHALL expose explicit initialization and shutdown hooks invoked during registration and unregistration. Initialization SHALL report failure without exceptions. Shutdown SHALL remove factory access immediately and release backend-owned state only after all world leases that require it have been released.

#### Scenario: Initialize a backend on registration
- **WHEN** a physics backend is registered
- **THEN** its initialization hook SHALL succeed before any physics world or shape is created

#### Scenario: Initialization fails
- **WHEN** backend initialization fails
- **THEN** registration SHALL report failure, no partially initialized backend SHALL be published, and acquired resources SHALL be released

#### Scenario: Shut down a backend on unregistration
- **WHEN** a physics backend is unregistered
- **THEN** new creation SHALL become unavailable and SDK teardown SHALL occur only after its outstanding world leases are released

#### Scenario: A backend with no global state
- **WHEN** a test backend does not require global state
- **THEN** its lifecycle hooks SHALL be satisfiable as no-ops without changing engine behavior

### Requirement: Heightfield collision shape

The physics module SHALL describe a heightfield collision shape as backend-neutral data comprising grid width, grid height, height samples with a declared sample format, height scale, height offset, and up axis, and the backend factory SHALL provide a creation method for it.

#### Scenario: Describe a heightfield

- **WHEN** a heightfield shape description is created with grid dimensions, samples, scale, offset, and up axis
- **THEN** the physics module SHALL accept it without referencing any backend type

#### Scenario: Backend creates a heightfield shape

- **WHEN** the backend factory creates a shape from a heightfield description
- **THEN** the backend SHALL produce a collision shape matching the described grid and vertical mapping

### Requirement: Capsule collision shape

The physics module SHALL describe a capsule collision shape (radius, height, pivot) as backend-neutral data and the backend factory SHALL provide a creation method for it.

#### Scenario: Create a capsule shape

- **WHEN** the backend factory creates a shape from a capsule description
- **THEN** the backend SHALL produce a capsule collision shape with the described radius and height

### Requirement: Shape creation delegates cooking to the backend

Shape descriptions SHALL carry plain data only; any backend-specific preparation (such as cooking a heightfield or triangle mesh) SHALL occur inside the backend, and the engine-side shape contract SHALL NOT expose backend types.

#### Scenario: Cooking stays in the backend

- **WHEN** a backend requires cooked geometry for a shape
- **THEN** the cooking SHALL happen inside the backend and the engine-side description SHALL remain unchanged

### Requirement: Backend capability descriptor

The physics backend contract SHALL expose a capability descriptor reporting at least the active simulation math mode (`Exact` or `Fast`) and whether that mode is deterministic, whether it supports job-based stepping, supports continuous collision detection, supports constraints, supports characters, and which shape kinds it implements.

#### Scenario: Capabilities are queryable before simulation

- **WHEN** a backend is registered
- **THEN** the engine SHALL be able to query its capabilities without creating a world

#### Scenario: Engine adapts to capabilities

- **WHEN** a feature is requested that the backend does not advertise
- **THEN** the engine SHALL report the feature as unavailable instead of invoking unsupported behavior

#### Scenario: Backend reports shape support

- **WHEN** a body uses a shape kind the backend does not implement
- **THEN** the capability query SHALL let a consumer detect this before authoring the body

### Requirement: World creation contract

The backend SHALL create and destroy physics worlds through the engine contract, and each created world SHALL own the objects created through it.

#### Scenario: Create a world

- **WHEN** the engine requests a world from the backend
- **THEN** the backend SHALL return a world implementing the engine world contract, or report failure when it cannot

#### Scenario: World owns its objects

- **WHEN** a world is destroyed
- **THEN** all objects created through it SHALL be destroyed with it and their handles SHALL become invalid

### Requirement: Single active backend registration

The engine registry SHALL maintain exactly one active backend at a time. Registering a new backend SHALL unregister the previous factory and release the registry's ownership of it without invalidating existing worlds' owned SDK state; defining DLL code SHALL remain process resident. World destruction SHALL use its creating owner's lifetime contract, not whichever backend is currently active.

#### Scenario: Replace the active backend
- **WHEN** a new backend is registered while one is active
- **THEN** the previous backend SHALL stop accepting creation, while outstanding worlds SHALL remain safely destructible

### Requirement: Jolt implements the engine shape contract

The Jolt backend SHALL implement box, sphere, capsule, heightfield, triangle mesh, convex hull and compound descriptors without exposing SDK objects. It SHALL honor pivots, body-origin transforms and supported scale, validate native creation results, and fail the entire request if a compound child cannot be created.

#### Scenario: Capsule dimensions
- **WHEN** a capsule descriptor supplies radius and cylindrical-section height
- **THEN** the resulting shape SHALL have that radius and cylinder height, excluding end caps

#### Scenario: Heightfield coordinates
- **WHEN** a heightfield is created
- **THEN** horizontal coordinates SHALL be centered using grid extents and vertical coordinates SHALL equal `sample * heightScale + heightOffset` along its declared up axis

#### Scenario: Grid does not fit the native heightfield representation
- **WHEN** a valid rectangular or small grid cannot be represented by the selected native heightfield shape
- **THEN** the backend SHALL cook equivalent static triangle geometry without discarding or inventing terrain samples

#### Scenario: Compound child fails
- **WHEN** a nested child is malformed or unsupported
- **THEN** creation SHALL fail and release all intermediate shapes without publishing a partial compound

#### Scenario: Unsupported dynamic mesh
- **WHEN** a world requests dynamic triangle-mesh support in this delivered configuration
- **THEN** creation or option validation SHALL report Unsupported rather than silently creating an invalid dynamic mesh

### Requirement: Native capacity is explicit and distinct from gameplay budgets

World creation SHALL accept validated native resource limits with finite defaults. A zero gameplay max-body or max-active-body budget SHALL mean no additional gameplay restriction, not unlimited native allocation. Unsupported growth, out-of-range limits and native exhaustion SHALL return explicit failure.

#### Scenario: Body capacity is reached
- **WHEN** creation exceeds the world's configured native body capacity
- **THEN** creation SHALL return an invalid handle with a capacity error and existing objects SHALL remain valid

#### Scenario: Failed option update
- **WHEN** an option update exceeds fixed native capacity or requests unsupported behavior
- **THEN** it SHALL return a failure status and leave the previously accepted options unchanged

### Requirement: Full collision masks survive native filtering

The backend SHALL preserve all 32 bits of collision group/mask data and apply the existing bilateral FilterAccepts rule to simulation. Query filtering SHALL apply the full mask and all ignored handles. Runtime filter changes SHALL invalidate affected cached contact decisions.

#### Scenario: Upper collision bits are used
- **WHEN** two objects interact through bits above bit 15
- **THEN** collision and query acceptance SHALL follow the engine masks without truncation

#### Scenario: An existing contact becomes filtered
- **WHEN** a live body's filter changes to reject a current contact
- **THEN** subsequent simulation SHALL stop that interaction and emit the appropriate end event without requiring body recreation

### Requirement: Live body property edits preserve identity transactionally

Supported mass and shape edits SHALL validate and cook candidates before changing the live body. Success SHALL retain the body handle, velocity and connected constraint handles, updating inertia and constraint COM offsets as required. Failure SHALL preserve the old component data and body and SHALL reach the editor command result.

#### Scenario: Invalid shape edit
- **WHEN** an edit supplies invalid geometry or an unavailable mesh product
- **THEN** the edit SHALL fail without destroying the previous body or entering undo history as a successful edit

#### Scenario: Constrained moving body is edited
- **WHEN** a valid mass, shape or scale change affects a constrained body
- **THEN** existing handles and constraints SHALL remain valid and their COM-dependent anchors SHALL be adjusted

### Requirement: Body kinds

The physics module SHALL describe a body as exactly one of static, dynamic, or kinematic, and the world SHALL apply the matching simulation behavior per kind.

#### Scenario: Static body does not move under simulation

- **WHEN** a static body is added and the world steps
- **THEN** the body SHALL remain at its authored transform unless explicitly moved

#### Scenario: Dynamic body responds to forces

- **WHEN** a dynamic body is added under gravity and the world steps
- **THEN** the body SHALL move according to the simulated forces

#### Scenario: Kinematic body follows authored motion

- **WHEN** a kinematic body's transform is set between steps
- **THEN** the body SHALL move to that transform and push dynamic bodies per the backend's kinematic behavior

### Requirement: Backend-neutral shape descriptors

The physics module SHALL describe collision shapes as plain data, covering at least box, sphere, capsule, heightfield, and triangle mesh, and SHALL allow a compound shape composed of multiple descriptors; shape data SHALL NOT contain backend types.

#### Scenario: Describe a shape without a backend

- **WHEN** a body descriptor carries a shape description
- **THEN** it SHALL reference engine data only and SHALL be accepted without any backend header

#### Scenario: Compound shape

- **WHEN** a body descriptor carries a compound of several shape descriptions
- **THEN** the backend SHALL be able to build a collision shape combining them

#### Scenario: Heightfield and triangle mesh remain distinct

- **WHEN** a heightfield or triangle-mesh shape is described
- **THEN** the physics module SHALL preserve the distinction and pass it to the backend without loading a render mesh

### Requirement: Mass, inertia, and damping

The physics module SHALL allow a body descriptor to specify mass, linear damping, and angular damping, with inertia either computed by the backend from the shape or supplied explicitly.

#### Scenario: Damping is applied

- **WHEN** a body is created with linear and angular damping
- **THEN** the backend SHALL apply those damping values to the simulated body

#### Scenario: Mass drives inertia

- **WHEN** a dynamic body is created with a mass and no explicit inertia
- **THEN** the backend SHALL compute its local inertia from the collision shape

### Requirement: Sleep control

The physics module SHALL allow a body to specify whether sleeping is enabled and an initial sleep state, and SHALL allow a sleeping body to be woken by external input.

#### Scenario: Body falls asleep when idle

- **WHEN** a dynamic body with sleeping enabled comes to rest and the world steps
- **THEN** it SHALL be allowed to enter a sleeping state

#### Scenario: External input wakes a body

- **WHEN** a force, impulse, or transform is applied to a sleeping body
- **THEN** the body SHALL wake and resume simulation

### Requirement: Continuous collision detection configuration

The physics module SHALL allow continuous collision detection to be requested per body and SHALL expose whether the active backend supports it.

#### Scenario: CCD is opt-in

- **WHEN** a body is created without requesting CCD
- **THEN** the backend SHALL NOT apply continuous collision detection to it

#### Scenario: Unsupported CCD is reported

- **WHEN** a body requests CCD and the backend does not support it
- **THEN** the world SHALL report the capability as unavailable rather than silently mis-simulating

### Requirement: Character controller descriptor

The physics module SHALL describe a character controller as plain data comprising a capsule (radius and height), step height, slope limit, gravity, up axis, and an initial transform, and the world SHALL allow characters to be addressed by `PhysicsObjectId`.

#### Scenario: Describe a character

- **WHEN** a character descriptor is created
- **THEN** it SHALL reference engine data only and SHALL be created through the world, returning a handle

#### Scenario: Runtime capsule change

- **WHEN** the capsule dimensions of a live character are changed
- **THEN** the world SHALL rebuild the underlying character collision without losing its handle

### Requirement: Character movement semantics

The character controller SHALL accept a displacement for the next movement step and SHALL report whether the displacement was accepted, while resolving collisions against the simulated world.

#### Scenario: Move is applied

- **WHEN** a displacement is submitted to a grounded character
- **THEN** the character SHALL be moved by the resolved displacement against collisions and SHALL report acceptance

#### Scenario: Move before attach

- **WHEN** a displacement is submitted before the character is attached to a world
- **THEN** the controller SHALL report it was not applied rather than crash

### Requirement: Grounded and world-transform queries

The character controller SHALL report whether it is grounded and SHALL allow reading and writing its world transform.

#### Scenario: Grounded after landing

- **WHEN** a character falls onto walkable geometry and settles
- **THEN** the controller SHALL report grounded

#### Scenario: Read back transform

- **WHEN** the character has moved under simulation
- **THEN** reading its world transform SHALL reflect the simulated position

### Requirement: Baseline character implementation

The active backend SHALL provide a baseline capsule character controller matching this contract, and the engine SHALL function without it by treating characters as unavailable.

#### Scenario: Backend provides a character

- **WHEN** a character is created through a backend that supports characters
- **THEN** it SHALL move, report grounded state, and respect step height and slope limit

#### Scenario: Backend lacks characters

- **WHEN** characters are requested from a backend without character support
- **THEN** the world SHALL report the capability as unavailable

### Requirement: Backend-neutral constraint descriptors

The physics module SHALL describe fixed, hinge, slider, distance and generic 6-DOF constraints as plain data with body handles, body-origin-local anchors/axes, limits and motors. An explicitly invalid handle SHALL denote a world anchor; a supplied stale or wrong-kind handle SHALL be rejected. Motor data SHALL specify Velocity or Position mode, defaulting to Velocity for existing data.

#### Scenario: Describe a joint without a backend
- **WHEN** a constraint descriptor is created
- **THEN** it SHALL reference engine data and body handles only and SHALL NOT include backend types

#### Scenario: Single-body and two-body constraints
- **WHEN** a constraint references one live body and an explicit world anchor, or two live bodies
- **THEN** the backend SHALL implement the corresponding single-body or two-body constraint

#### Scenario: Supplied body is stale
- **WHEN** a supplied non-invalid body handle cannot be resolved
- **THEN** creation SHALL fail without treating that handle as a world anchor

### Requirement: Constraint lifetime is world-owned

Constraints SHALL be created and removed through the world, addressed by `PhysicsObjectId`, and their lifetime SHALL follow the world.

#### Scenario: Create and remove a constraint

- **WHEN** a constraint is created and later removed
- **THEN** the world SHALL return a handle on creation and SHALL invalidate it on removal

#### Scenario: Removing a body removes its constraints

- **WHEN** a body referenced by a constraint is removed
- **THEN** the world SHALL remove the dependent constraint rather than leave a dangling reference

### Requirement: Constraint support is capability-gated

The world SHALL report which constraint kinds the active backend supports, and requesting an unsupported kind SHALL fail safely.

#### Scenario: Unsupported constraint is reported

- **WHEN** a constraint kind is requested that the backend does not implement
- **THEN** the world SHALL report it as unsupported and SHALL NOT create a partially working constraint

### Requirement: Limits and drives are describeable

Constraint descriptors SHALL express angular/linear limits and explicit motor modes. The backend SHALL apply supported fields and reject nondefault unsupported fields before creating any constraint. Hinge SHALL use angularX and the angular motor; Slider SHALL use linearX and the linear motor; Generic6DOF SHALL apply all six limits and bind the existing scalar motors to their local X axes. A Generic6DOF axis with limited=false SHALL be free; equal lower/upper bounds SHALL lock it.

#### Scenario: Configure limits
- **WHEN** a hinge is described with angularX limits
- **THEN** the backend SHALL enforce those limits in simulation

#### Scenario: Choose motor mode
- **WHEN** a supported motor selects Position mode
- **THEN** its position target and force/torque limit SHALL be applied rather than being interpreted as a velocity drive

#### Scenario: Unsupported drive is requested
- **WHEN** a fixed or distance constraint carries an unsupported enabled motor
- **THEN** creation SHALL return an explicit unsupported result and SHALL NOT publish a partially working constraint

### Requirement: Distance constraints enforce authored length

A distance constraint SHALL maintain its authored nonnegative anchor distance. A zero distance SHALL use an equivalent point constraint. It SHALL NOT silently implement every distance as coincident anchors.

#### Scenario: Nonzero distance
- **WHEN** two bodies are connected with a nonzero distance
- **THEN** simulation SHALL preserve that anchor separation within solver tolerance

### Requirement: Connected collision exclusion is pair-specific

When collideConnected is false, the backend SHALL exclude only the referenced body pair from mutual collision. Removing that constraint SHALL restore the pair's normal filter behavior unless another live constraint still excludes it.

#### Scenario: Three-body chain
- **WHEN** A-B and B-C constraints exclude their connected pairs
- **THEN** A-C collision SHALL remain governed by its ordinary masks

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

### Requirement: Selectable simulation math mode interface

The engine SHALL expose a selectable `PhysicsMathMode` with at least `Fast` and `Exact`, and every backend/world SHALL report its active mode together with whether that mode is deterministic, so that consumers can decide whether determinism-dependent features are available.

#### Scenario: Query the active mode

- **WHEN** a consumer queries the world's capabilities
- **THEN** it SHALL receive the active `PhysicsMathMode` and a deterministic flag for that mode

#### Scenario: Requesting an unavailable mode is rejected

- **WHEN** a world is requested in `Exact` mode and no deterministic backend is registered
- **THEN** the engine SHALL report the mode as unavailable rather than silently creating a `Fast` world

### Requirement: Fast mode is the delivered mode and is not deterministic

This change SHALL deliver the `Fast` mode (native `float`), and a `Fast`-mode world SHALL report that determinism is unavailable so that determinism-dependent features are not silently used.

#### Scenario: Fast reports non-deterministic

- **WHEN** the shipped `Fast` backend is queried for capabilities
- **THEN** it SHALL report that determinism is unavailable

#### Scenario: Determinism-dependent features are gated

- **WHEN** a determinism-dependent feature (such as rollback re-simulation or lockstep) is requested on a `Fast` world
- **THEN** the engine SHALL report determinism as unavailable rather than claim it

### Requirement: Reserved cross-platform determinism contract for Exact

`Exact` mode SHALL, once a deterministic backend is registered, produce bit-exact results across the supported platforms for SkyEngine's own builds given the same initial state and the same ordered input sequence, independent of frame rate and wall-clock time. This requirement is **reserved**: it is not delivered by this change, which only provides the interface and the `Fast` implementation.

#### Scenario: Reserved for a future backend

- **WHEN** no deterministic backend is registered
- **THEN** `Exact` SHALL remain unavailable and this requirement SHALL be documented as reserved

#### Scenario: Satisfied when an Exact backend ships

- **WHEN** a deterministic backend is registered and a world is created in `Exact` mode
- **THEN** the same initial state and ordered input sequence SHALL yield identical object states across supported platforms

### Requirement: Reserved determinism validation

The reserved `Exact` contract SHALL be covered, in the change that implements a deterministic backend, by a reproducibility test comparing simulation and event traces across the supported platforms. This is **reserved** with the `Exact` implementation.

#### Scenario: Reserved with the implementation

- **WHEN** the deterministic backend is implemented
- **THEN** a cross-platform trace test SHALL be added to validate the contract

### Requirement: 确定性浮点构建选项

构建系统 SHALL 提供一个确定性浮点选项（`SKY_DETERMINISTIC_FP`，默认关闭），开启时禁止浮点表达式收缩为 FMA（GNU/Clang `-ffp-contract=off`、MSVC `/fp:precise`），并定义 `SKY_DETERMINISTIC_FP=1`。确定性后端（`Exact` 模式）SHALL 在该选项开启的构建中运行，以避免因架构/编译器的 FMA 收缩产生跨平台差异。

#### Scenario: 默认关闭不改变行为

- **WHEN** 未开启 `SKY_DETERMINISTIC_FP`
- **THEN** 构建与数值结果与既有保持一致

#### Scenario: 开启后禁止 FP 收缩

- **WHEN** 以 `-DSKY_DETERMINISTIC_FP=ON` 配置
- **THEN** C/C++ 编译 SHALL 使用 `-ffp-contract=off`（MSVC：`/fp:precise`）且定义 `SKY_DETERMINISTIC_FP=1`

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

### Requirement: Documented backend-neutral filter contract

The physics module SHALL define and document a backend-neutral collision filtering contract over the `CollisionFilterBit` group/mask model, and each backend SHALL map it consistently.

#### Scenario: Filter contract is defined

- **WHEN** the physics filtering contract is inspected
- **THEN** it SHALL specify how group and mask are interpreted and how a backend must map them

#### Scenario: Objects respect group and mask

- **WHEN** two objects are tested for collision with group/mask values that do not permit interaction
- **THEN** they SHALL NOT interact under the active backend

### Requirement: Bullet backend filter mapping

The Bullet backend SHALL map the group/mask contract onto Bullet's collision filtering so the contract holds for the current backend.

#### Scenario: Bullet honors group and mask

- **WHEN** Bullet-backed objects are added with group and mask values
- **THEN** collisions SHALL be filtered according to the documented contract

### Requirement: Filter contract supports a filter-shader backend

The filter contract SHALL be expressible by a backend that uses a filter function (for example a PhysX filter shader) without changing the engine-side contract.

#### Scenario: Filter is expressible as a function

- **WHEN** the documented contract is implemented as a boolean filter function of two objects' group and mask
- **THEN** it SHALL produce the same accept/reject decision as the Bullet mapping

### Requirement: Named interaction layers

The physics module SHALL allow collision filter bits to be associated with named interaction layers registered at engine level, so authors express filtering by layer name rather than raw bits.

#### Scenario: Resolve a layer by name

- **WHEN** a body is authored with a named layer and a named mask set
- **THEN** the module SHALL resolve them to group/mask bits consistently across runs

#### Scenario: Unknown layer is rejected

- **WHEN** a body references a layer name that is not registered
- **THEN** the module SHALL reject it or fall back to a documented default rather than silently using arbitrary bits

### Requirement: Query filter contract

A query filter SHALL combine a mask, an optional set of ignored handles, and an optional layer matrix, and SHALL be applied by every query kind (raycast, sweep, overlap) with consistent semantics.

#### Scenario: Ignore a handle

- **WHEN** a query carries the handle of the object issuing it as ignored
- **THEN** that object SHALL be excluded from the query results

#### Scenario: Consistent across query kinds

- **WHEN** the same filter is applied to a raycast, a sweep, and an overlap
- **THEN** the accept/reject decision for a given object SHALL be the same across all three

### Requirement: Backend mapping of the query filter

Each backend SHALL map the query filter contract onto its filtering mechanism so that query results match the documented contract.

#### Scenario: Bullet maps the query filter

- **WHEN** a filtered query is executed on the Bullet backend
- **THEN** the results SHALL honor the mask, ignored handles, and layer matrix per the contract

### Requirement: Physics material data

The physics module SHALL define backend-neutral material data containing static friction, dynamic friction, restitution, friction and restitution combine modes, and linear and angular damping, and the backend factory SHALL provide a creation method for materials.

#### Scenario: Create a material

- **WHEN** a material is created from friction, restitution, combine modes, and damping values
- **THEN** the physics module SHALL produce a backend-neutral material usable by bodies

#### Scenario: Combine modes are carried

- **WHEN** a material specifies a friction or restitution combine mode
- **THEN** the material data SHALL preserve that mode and pass it to the backend for the contact computation

#### Scenario: Damping is carried

- **WHEN** a material specifies linear or angular damping and is assigned to a body
- **THEN** the backend SHALL apply the damping to that body

### Requirement: Objects consume materials

Collision objects and rigid bodies SHALL apply the material assigned to them, and the engine SHALL NOT hardcode friction or restitution in the backend.

#### Scenario: Material values reach the backend

- **WHEN** a collision object or rigid body is assigned a material
- **THEN** the backend SHALL apply that material's friction and restitution to the underlying object

#### Scenario: Default material

- **WHEN** an object is created without an explicit material
- **THEN** a documented default material SHALL be applied rather than a backend-specific constant

### Requirement: Per-body material override

A body SHOULD be able to override or disable the world default material, and the backend SHALL apply the effective material to the body's contacts.

#### Scenario: Body overrides default material

- **WHEN** a body carries an explicit material
- **THEN** the backend SHALL apply that material's values instead of the world default

#### Scenario: Body without a material uses the default

- **WHEN** a body carries no material
- **THEN** the world default material SHALL apply

### Requirement: Jolt material mapping has explicit supported semantics

The Jolt backend SHALL apply effective friction, restitution and damping from engine material data. Its delivered single-friction contact model SHALL require equal static and dynamic friction values and SHALL reject unequal values as Unsupported rather than discard one. Invalid handles, non-finite values and invalid coefficients SHALL fail before mutation.

#### Scenario: Supported material is applied
- **WHEN** a material with equal valid static/dynamic friction is assigned
- **THEN** its friction, restitution and effective damping SHALL be applied to the body and subsequent contacts

#### Scenario: Separate friction coefficients are requested
- **WHEN** staticFriction differs from dynamicFriction
- **THEN** material creation or assignment SHALL return Unsupported and preserve existing body material state

#### Scenario: A stale material handle is reused
- **WHEN** a deleted material's slot has been reused and the old handle is supplied
- **THEN** the request SHALL fail rather than select the new material

### Requirement: Contact combine rules are backend-independent

The backend SHALL combine the two effective material values using their selected Average, Min, Multiply or Max mode. When the materials select different modes, priority SHALL be `Max > Multiply > Min > Average`. The computation SHALL be symmetric in body ordering and SHALL be applied to both friction and restitution.

#### Scenario: Different combine modes meet
- **WHEN** one material selects Average and the other selects Max
- **THEN** the corresponding contact coefficient SHALL be the maximum of the two values regardless of body order

### Requirement: Damping limitations and precedence are explicit

The delivered backend SHALL accept isotropic per-body damping and reject unequal XYZ components as Unsupported. A nonzero isotropic body damping value SHALL override the corresponding material damping; an all-zero vector SHALL select that material damping. Later material reassignment SHALL preserve explicit body damping overrides.

#### Scenario: Body damping overrides a material
- **WHEN** a body provides equal nonzero linear damping components
- **THEN** those components SHALL define its scalar linear damping even after another material is assigned

#### Scenario: Directional damping is requested
- **WHEN** a body descriptor supplies unequal damping components
- **THEN** creation SHALL fail explicitly without silently reducing them to a scalar

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

### Requirement: Stable descriptor field names

Physics descriptors SHALL have stable, documented serialized field names and a version marker, so that saved physics data can be read back across code changes.

#### Scenario: Field names are stable

- **WHEN** a physics descriptor is serialized
- **THEN** the emitted field names SHALL be documented, correctly spelled, and not silently renamed between versions

#### Scenario: Shape data round-trips

- **WHEN** a shape descriptor is serialized and deserialized
- **THEN** all of its data required to rebuild the shape SHALL be preserved, with no field omitted

### Requirement: Body descriptor round-trip

A body descriptor SHALL serialize and deserialize without loss, preserving kind, shape, mass, damping, material reference, filter, CCD, gravity, and initial transform.

#### Scenario: Body round-trip

- **WHEN** a body descriptor is serialized and then deserialized
- **THEN** the reconstructed descriptor SHALL equal the original

### Requirement: Physics component round-trip

Physics components SHALL serialize and deserialize so that saving and restoring a scene preserves each component's physics configuration.

#### Scenario: Component round-trip

- **WHEN** an actor with a physics component is serialized and restored
- **THEN** the component SHALL be recreated with the same configuration and SHALL re-establish its physics object through a handle

### Requirement: Material and filter round-trip

Material data and collision filter data SHALL serialize and deserialize without loss, including combine modes, damping, and group/mask values.

#### Scenario: Material and filter round-trip

- **WHEN** material and filter data are serialized and deserialized
- **THEN** the reconstructed values SHALL equal the originals

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

### Requirement: Backend migration preserves serialized identity

Replacing the provider SHALL preserve the `Physics` subsystem name, `PhysicsBody` component name, existing sky::phy type identities, enum values and serialized field names. Additive fields SHALL have explicit defaults. Saved data SHALL NOT contain SDK types or require a legacy Bullet module to deserialize.

#### Scenario: Existing world is loaded
- **WHEN** a scene saved with the existing PhysicsBody component and PhysicsSubSystemConfig is opened after migration
- **THEN** those types SHALL resolve to the Jolt plugin's registrations without changing their stored identities

### Requirement: Shape collections are fully serialized

Shape reflection SHALL preserve heightfield samples, convex hull points and recursive compound children in addition to all existing scalar fields. Runtime mesh pointers SHALL NOT be persisted as native addresses; asset identity SHALL remain the source for asset-backed mesh resolution.

#### Scenario: Compound shape round-trip
- **WHEN** a compound containing a heightfield and convex child is serialized and read back
- **THEN** its child hierarchy, pivots, height samples and hull points SHALL match the original descriptors

#### Scenario: Older data lacks newly registered collections
- **WHEN** older data omits a collection field
- **THEN** it SHALL be read using that field's documented default and any resulting invalid shape SHALL be reported explicitly rather than invented

### Requirement: Capture world state

A physics world SHALL capture its simulation state into a backend-neutral snapshot addressed by stable `PhysicsObjectId`, including a step/frame counter and per-body kinematic state.

#### Scenario: Capture over stable ids

- **WHEN** a snapshot is captured from a running world
- **THEN** it SHALL record the step counter and per-body transform, linear velocity, angular velocity, and sleep state keyed by `PhysicsObjectId`

#### Scenario: Snapshot scope is selectable

- **WHEN** a snapshot is requested with the default scope
- **THEN** it SHALL include dynamic and kinematic bodies and MAY omit static state; a full scope SHALL include all objects

### Requirement: Restore world state

A physics world SHALL restore a previously captured snapshot, replacing the simulation state while preserving object identity so that existing handles remain valid.

#### Scenario: Restore reproduces captured state

- **WHEN** a snapshot is restored
- **THEN** subsequent reads SHALL report the captured kinematic state for the captured ids

#### Scenario: Handles survive restore

- **WHEN** a snapshot is restored into the same world
- **THEN** `PhysicsObjectId` handles that were valid before the restore SHALL remain valid

#### Scenario: Mismatched snapshot is rejected

- **WHEN** a snapshot produced by an incompatible world or version is restored
- **THEN** the restore SHALL fail safely without partially corrupting the world

### Requirement: Snapshots support editor replay

The engine SHALL allow buffering snapshots so the editor can pause, single-step, and replay simulation, without the snapshot containing any render or backend types.

#### Scenario: Buffer and replay

- **WHEN** the editor buffers snapshots over several steps and restores an earlier one
- **THEN** the world SHALL resume from the restored state and stepping SHALL continue from the restored step counter

### Requirement: Rollback uses snapshot restore

The world SHALL support rollback by restoring a captured snapshot. Re-simulation from a confirmed snapshot is reserved for the future `Exact` mode (`physics-determinism`) and SHALL be reported unavailable on the delivered `Fast` mode.

#### Scenario: Restore rolls back state

- **WHEN** a previously captured snapshot is restored
- **THEN** subsequent reads SHALL report the captured state and stepping SHALL continue from the captured step counter

#### Scenario: Re-simulation is reserved

- **WHEN** re-simulation-based rollback is requested on the delivered `Fast` mode
- **THEN** the world SHALL report it unavailable and use snapshot restore

### Requirement: Fixed-timestep advancement

Physics simulation SHALL advance on a configurable fixed timestep driven by the engine, and a frame SHALL run zero or more fixed steps according to an accumulator rather than a single variable-delta step.

#### Scenario: Accumulator runs the correct number of steps

- **WHEN** a frame delta is submitted with a configured fixed timestep
- **THEN** the simulation SHALL execute a whole number of fixed steps equal to the accumulated time divided by the fixed timestep

#### Scenario: Spiral of death is bounded

- **WHEN** a frame delta is much larger than the fixed timestep
- **THEN** the number of steps SHALL be capped by a configured maximum and the excess time SHALL be discarded

### Requirement: Deterministic simulation inputs

The engine and backend step paths SHALL NOT read wall-clock time or the rendered frame delta as simulation inputs; simulation SHALL depend only on the fixed timestep, the object state, and commands issued between steps.

#### Scenario: No frame-delta dependence

- **WHEN** the same sequence of commands is applied with different render frame rates
- **THEN** the simulated result SHALL be the same for the same fixed-step count

#### Scenario: Reproducible on the same build

- **WHEN** the same world is simulated twice from the same initial state and input sequence on the same build
- **THEN** the resulting object states SHALL be identical

#### Scenario: Cross-platform reproducibility is reserved

- **WHEN** cross-platform bit-exact reproducibility is required
- **THEN** it SHALL be provided only by the reserved `Exact` mode (`physics-determinism`), not by the delivered `Fast` mode

### Requirement: Stable object ordering during a step

The engine SHALL present objects to the backend in a deterministic order (stable id or insertion order) so that simulation results do not depend on container iteration or hashing.

#### Scenario: Ordering is independent of storage

- **WHEN** a step is executed
- **THEN** object processing order SHALL be deterministic and not vary between runs

### Requirement: Transform interpolation for rendering

The world SHALL expose, per body, both the last simulated transform and an interpolated transform derived from the previous and current fixed steps, so rendering can be decoupled from the fixed step.

#### Scenario: Interpolated transform is provided

- **WHEN** rendering occurs between two fixed steps with a fractional accumulator remainder
- **THEN** the world SHALL provide a blend of the previous and current simulated transforms

### Requirement: Job-friendly step pipeline with fallback

The step SHALL be expressed as ordered phases with a defined read/write contract, and the engine SHALL dispatch them through a job seam when the active backend advertises job support; otherwise the step SHALL run single-threaded. Any phase accumulation or reduction SHALL use a deterministic order so parallel execution yields the same result as single-threaded execution.

#### Scenario: Single-threaded fallback

- **WHEN** the backend does not advertise job support or the job system is unavailable
- **THEN** the step SHALL execute correctly on a single thread

#### Scenario: Phases respect the read/write contract

- **WHEN** phases are dispatched in parallel and a phase declares write access to an object
- **THEN** no other phase SHALL concurrently write the same object

#### Scenario: Parallel result matches single-threaded

- **WHEN** the same step is run with job stepping enabled and disabled
- **THEN** the resulting simulation state SHALL be identical

### Requirement: Step statistics

The world SHALL expose per-step statistics (step count, active body count, and per-phase timing when profiling is enabled) without requiring a render dependency.

#### Scenario: Stats are available after a step

- **WHEN** stats are queried after stepping
- **THEN** the world SHALL report the number of steps executed and the active body count

### Requirement: Step completion reports native failure

World Step SHALL return an engine PhysicsStatus. Native capacity/update errors SHALL be translated into that status after all scheduled work is collected. A failed native update SHALL put the world in a faulted state, prevent further stepping and expose the failure to its host; it SHALL NOT publish the partial update as a successful simulation frame or claim that it was rolled back.

#### Scenario: Native contact capacity is exceeded
- **WHEN** a native update reports contact or pair capacity exhaustion
- **THEN** Step SHALL complete all jobs, return CapacityExceeded, fault the world and withhold normal successful-frame notification

#### Scenario: Caller steps a faulted world
- **WHEN** another Step is requested before explicit world reconstruction
- **THEN** it SHALL return InvalidState without further native simulation

#### Scenario: Invalid timestep
- **WHEN** the fixed timestep is zero, negative or non-finite
- **THEN** Step SHALL reject it before native mutation

### Requirement: Tick and single-step share completion behavior

PhysicsSystem Tick and SingleStep SHALL use the same fixed-step completion path, including status handling, character input consumption, pose synchronization, event dispatch and interpolation updates. Paused time SHALL NOT accumulate hidden simulation steps.

#### Scenario: Editor single-step
- **WHEN** a paused world receives SingleStep
- **THEN** it SHALL execute one fixed step and publish its resulting poses and events using the same completion rules as Tick

#### Scenario: Character command spans a render frame
- **WHEN** a displacement is queued and a render frame causes multiple fixed steps
- **THEN** the queued displacement SHALL be consumed once rather than applied in full on every substep

### Requirement: Worker callbacks do not directly invoke gameplay

Native contact callbacks SHALL only update world-owned synchronized contact data. After all jobs finish, the host thread SHALL reduce subshape contacts to object pairs and emit events ordered by stable engine handles. Sleeping persistent contacts SHALL NOT produce false end events.

#### Scenario: Compound body has several touching children
- **WHEN** several native subshape pairs represent the same object contact
- **THEN** the host SHALL emit one logical begin/stay/end sequence for that object pair

#### Scenario: Contact persists while both objects sleep
- **WHEN** native contact callbacks are not repeated for sleeping bodies
- **THEN** the engine SHALL preserve the persistent contact until removal or actual separation

### Requirement: Scene input precedes simulation and dynamic output follows it

Actor transform edits SHALL update static/dynamic body poses or submit kinematic targets. World Tick SHALL run actor input, subsystem simulation and then component PostTick. Physics output SHALL update only dynamic actor poses and SHALL prevent transform feedback into the backend.

#### Scenario: Kinematic actor moves
- **WHEN** an actor supplies a new kinematic transform
- **THEN** the next physics step SHALL move toward that target, and stale interpolation SHALL NOT overwrite the actor's authored transform

#### Scenario: Dynamic interpolation is published
- **WHEN** World finishes a physics tick
- **THEN** the actor SHALL receive the interpolation result from that completed tick rather than the preceding host tick

### Requirement: Adaptor event batches have a bounded host-frame lifetime

PhysicsSystem SHALL dispatch all ordered events from the completed host Tick/SingleStep to listeners. Its polling batch SHALL be consumable until the next host tick, after which it SHALL be retired; it SHALL NOT accumulate an unbounded history when nobody polls. Listener Begin/End delivery SHALL NOT depend on polling.

#### Scenario: Ordinary host never drains events
- **WHEN** persistent contacts run for many host ticks without a polling consumer
- **THEN** retained adaptor events SHALL be bounded by the current host batch rather than simulation duration

#### Scenario: Multiple listeners observe a trigger
- **WHEN** a body enters and leaves a trigger across host steps
- **THEN** each registered listener SHALL see the ordered Enter/Exit sequence, while polling SHALL expose only the current batch

### Requirement: Authoring builders follow the existing developer build boundary

The Jolt builder directory SHALL be enabled only when `SKY_DEVELOP` or `SKY_BUILD_TEST` is enabled. Existing tool and sandbox options SHALL continue to enable developer facilities through their shared option contract. Shipping without those facilities SHALL retain the runtime, render bridge and shared asset codec without building, statically registering or linking Jolt's authoring builder. An enabled builder SHALL use the existing tagged module entry so its static factory matches CMake-generated registration while preserving the dynamic entry contract.

#### Scenario: Shipping static launcher
- **WHEN** a static launcher is configured with developer facilities, tools, sandbox and tests disabled
- **THEN** Jolt Builder targets SHALL be absent and its factory SHALL appear in neither generated registration nor launcher link inputs
- **AND** the Jolt runtime and render modules SHALL remain available

#### Scenario: Developer or test host requires the builder
- **WHEN** Jolt is configured for developer facilities, a tool, sandbox or tests
- **THEN** its builder targets SHALL remain available to those consumers
- **AND** static registration SHALL request the factory emitted by the builder's tagged entry rather than the dynamic-only entry

### Requirement: iOS Jolt consumer RTTI parity

The pinned iOS Jolt Debug and Release SDK builds SHALL enable C++ RTTI to match the existing adaptor subclasses. C++ exceptions SHALL remain disabled, and the change SHALL NOT introduce RTTI downcasts or alter other platforms' effective build options.

#### Scenario: Link adaptor subclasses on an iOS device build

- **WHEN** the actual iOS application links the adaptor's group filter and geometry collector against the rebuilt SDK
- **THEN** their Jolt base-class typeinfo SHALL resolve without disabling RTTI in the application or adaptor
