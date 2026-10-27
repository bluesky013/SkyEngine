## ADDED Requirements

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

The third-party manifest SHALL pin Jolt v5.6.0 to revision `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, use the existing bootstrap and publish matching Debug/Release artifacts. Import definitions SHALL match the SDK's ABI-affecting build options. The delivered configuration SHALL use float precision, disable C++ exceptions and RTTI, and preserve the SDK license notice.

#### Scenario: Rebuild from an existing dependency checkout
- **WHEN** the Jolt dependency is rebuilt from a stale checkout
- **THEN** the existing version-pinning mechanism SHALL select the pinned revision before configuration and compilation

#### Scenario: Consumer links the dependency
- **WHEN** the backend is compiled against an installed Debug or Release SDK
- **THEN** its CRT, assertion, profiling, debug-renderer, ObjectLayer and SIMD settings SHALL match that SDK

### Requirement: Physics layering and metadata ownership survive replacement

`engine/physics` SHALL remain free of plugin and Jolt types. SDK implementation, Framework integration and module entry SHALL live together in the Jolt plugin adaptor. Native headers SHALL remain private to adaptor implementation. The plugin SHALL NOT introduce a separate core target without independent domain logic. Aurora asset conversion SHALL remain an independent render bridge; neither implementation target SHALL depend on the other. The module definition SHALL be the sole owner of physics component/config metadata.

#### Scenario: Consumer resolves physics
- **WHEN** terrain, navigation, editor or a physics component needs the physics world
- **THEN** it SHALL resolve the engine interface through the established `Physics` subsystem contract without accessing a concrete backend world

#### Scenario: Runtime restarts
- **WHEN** the module runtime is stopped and started again within the same process definition
- **THEN** services SHALL be registered symmetrically and metadata SHALL NOT be registered again or hidden behind a duplicate-registration guard

### Requirement: SDK state and executable code outlive physics objects

The backend and every live world SHALL hold an SDK state lease and the necessary module code lease. Unregistering a backend SHALL prevent new world creation but SHALL NOT destroy SDK state or unload code still needed by existing worlds. The last user SHALL release jobs, native objects and callbacks before releasing SDK state and code.

#### Scenario: Registry is unregistered before a world is released
- **WHEN** a backend is unregistered while a created world is still owned by a consumer
- **THEN** that world SHALL retain the state and code needed for safe access and destruction, and a new registry creation request SHALL fail

#### Scenario: World is torn down during shutdown
- **WHEN** the last world is destroyed
- **THEN** all jobs SHALL be joined and constraints, characters, bodies, shapes and listeners SHALL be released before SDK teardown

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
