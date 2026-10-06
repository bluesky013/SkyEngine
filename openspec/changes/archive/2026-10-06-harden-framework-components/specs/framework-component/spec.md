## ADDED Requirements

### Requirement: Component lifecycle symmetry

Adding a component to an actor that is attached to a world SHALL invoke the component's attach hook, and removing a component from an attached actor SHALL invoke its detach hook before the component is destroyed. A component SHALL NOT be destroyed from an actor that is still attached to a world without its detach hook having run.

#### Scenario: Remove from attached actor detaches first

- **WHEN** a component that overrides its detach hook is removed from an actor whose world is non-null
- **THEN** the detach hook SHALL run before the component is destroyed

#### Scenario: Add to attached actor attaches

- **WHEN** a component is added to an actor whose world is non-null
- **THEN** the component's attach hook SHALL run

#### Scenario: Detached actor removal is silent

- **WHEN** a component is removed from an actor whose world is null
- **THEN** no attach or detach hook SHALL be required to run

### Requirement: Safe component construction from a type id

Constructing or loading a component by type id SHALL NOT dereference a null type node, and SHALL reject unknown or non-constructible component types without crashing.

#### Scenario: Unknown type id is rejected

- **WHEN** a component is added or loaded by a type id that is not registered
- **THEN** the call SHALL fail without dereferencing a null type node

#### Scenario: Type without a constructor is rejected

- **WHEN** a registered component type has no construct function
- **THEN** the call SHALL skip construction without invoking a null constructor

### Requirement: Transform hierarchy derivation

A transform component SHALL treat its local transform as the serialized, authored source of truth and SHALL derive its world transform as `parent world * local world`. Setting a local transform repeatedly SHALL NOT compound the world transform. Resolving hierarchy on load SHALL preserve the serialized local transform. Reparenting SHALL support both preserving the local transform and preserving the world transform.

#### Scenario: Repeated local set does not compound

- **WHEN** the local translation of a component is set more than once
- **THEN** its world transform SHALL equal the parent world transform composed with the latest local transform

#### Scenario: Load preserves serialized local

- **WHEN** a world with a parent-child transform hierarchy is saved and loaded
- **THEN** each component's local transform SHALL equal the serialized value, and the child's world transform SHALL equal parent world composed with local

#### Scenario: Reparent preserving world

- **WHEN** a component is reparented and world preservation is requested
- **THEN** its world transform SHALL be unchanged and its local transform SHALL be recomputed

#### Scenario: Parent actor change is reported

- **WHEN** an actor is reparented by an actor operation
- **THEN** the previous parent SHALL be reported to the parent-changed event

### Requirement: Transform change propagation

When a transform component's transform changes, its descendants SHALL be updated to reflect the new ancestor transform.

#### Scenario: Moving a parent moves its children

- **WHEN** a parent transform is changed
- **THEN** each child's world transform SHALL be recomputed from the parent's new world transform and the child's local transform, recursively

### Requirement: Deterministic component ordering

Per-actor component iteration used for ticking and serialization SHALL be deterministic and stable across runs.

#### Scenario: Tick order is stable

- **WHEN** an actor is ticked more than once with an unchanged component set
- **THEN** components SHALL be ticked in the same order every time

#### Scenario: Serialized order is stable

- **WHEN** an actor is saved twice with an unchanged component set
- **THEN** components SHALL be serialized in the same order

### Requirement: Framework component model boundary

The framework component system SHALL remain a traditional object-oriented component model. The framework SHALL NOT define or ship an ECS entity manager; data-oriented ECS SHALL live in `engine/core/ecs` and be consumed by render-side implementations.

#### Scenario: No ECS entity manager in framework

- **WHEN** the framework world module is inspected
- **THEN** it SHALL NOT provide an entity manager or a competing entity identifier type

#### Scenario: Render ECS is separate

- **WHEN** a render-side implementation needs data-oriented storage
- **THEN** it SHALL use `engine/core/ecs` rather than framework components

### Requirement: Persisted component data is logic-only

A component's serialized data SHALL contain only plain data, `Uuid`, and engine value types. It SHALL NOT contain render or network implementation types.

#### Scenario: Serialized data stays logic-only

- **WHEN** a component's data struct is inspected
- **THEN** it SHALL contain only plain data, `Uuid`, and engine value types

### Requirement: Runtime caches are non-authoritative and reconstructible

A component MAY hold non-serialized runtime state (asset handles, engine-interface references, cached results). Such state SHALL be reconstructible from the persisted data plus assets and SHALL NOT be authoritative scene state.

#### Scenario: Runtime cache is excluded from persistence

- **WHEN** a component holds a non-serialized asset handle
- **THEN** the handle SHALL NOT appear in the serialized data, and the component SHALL be usable after reconstructing it from the persisted data

### Requirement: Render state ownership stays render-side

A component SHALL NOT own render/GPU state (renderer objects or GPU resources). Render state SHALL be owned by the render side; the component MAY hold a handle to it.

#### Scenario: Component does not own a renderer

- **WHEN** a component references render-side state
- **THEN** it SHALL hold a handle or identifier, not the owning renderer or GPU object

### Requirement: Layer-appropriate component dependencies

A component in the framework layer SHALL depend only on `framework`, its own module's types, and `core`. An adaptor-layer component MAY depend on its adaptor module (for example aurora assets), but SHALL NOT include another layer's implementation headers.

#### Scenario: Framework component has no adaptor includes

- **WHEN** a framework-layer component header is inspected
- **THEN** it SHALL NOT include aurora/render or network implementation headers

#### Scenario: Adaptor component stays within its module

- **WHEN** an adaptor-layer component header is inspected
- **THEN** it SHALL only include its own adaptor module, `framework`, and `core`
