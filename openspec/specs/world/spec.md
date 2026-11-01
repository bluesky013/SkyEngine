# world Specification

## Purpose

Define actor/component ownership, world subsystems and transform lifecycle contracts.

## Requirements

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

### Requirement: World exclusively owns actors

A world SHALL be the sole owner of its actors. No public API SHALL hand out shared ownership of an actor, and an actor SHALL be destroyed when it leaves its world.

#### Scenario: No shared ownership escapes

- **WHEN** an actor is created or looked up
- **THEN** the returned value SHALL be non-owning, and destroying it SHALL NOT destroy the actor

#### Scenario: Detach destroys

- **WHEN** an actor is detached from its world and no other world owns it
- **THEN** the actor SHALL be destroyed

### Requirement: Non-owning references and explicit ownership transfer

Public references to actors SHALL be non-owning, and ownership SHALL move explicitly: attaching takes ownership of an actor, and detaching returns ownership to the caller.

#### Scenario: Detach returns ownership

- **WHEN** an actor is detached from its world
- **THEN** ownership SHALL be returned to the caller, and the actor SHALL remain alive until the caller releases it

#### Scenario: Reattach after detach

- **WHEN** a detached actor (whose ownership the caller holds) is attached to a world
- **THEN** it SHALL be attached without creating a second owner

#### Scenario: Lookups do not extend lifetime

- **WHEN** an actor is looked up
- **THEN** the returned reference SHALL be non-owning

<!-- Generation-safe handles are not planned; public actor references are non-owning. -->

### Requirement: O(1) lookup and removal

Actor lookup by identity and removal SHALL not scan the actor list.

#### Scenario: Lookup is constant time

- **WHEN** an actor is looked up by its identity in a world with many actors
- **THEN** the lookup SHALL use the index, not a linear scan

### Requirement: Dense storage preserved

Actor storage SHALL remain a packed array so iteration stays cache-friendly.

#### Scenario: Removal keeps storage packed

- **WHEN** an actor is detached
- **THEN** the actor array SHALL remain contiguous with no holes

### Requirement: Subsystem factory registry

The engine SHALL provide a `WorldSubSystemRegistry` where subsystem plugins register, under a stable name, a
factory plus an optional **reflected config type**, an optional **default config** instance, and an optional
**validator** that reports a reason, so worlds can create subsystems without hosts depending on concrete types.
Re-registering a name SHALL override the previous registration (logged).

#### Scenario: Register and create
- **WHEN** a plugin registers a factory under a name and a world requests that name
- **THEN** the registry SHALL create and return the subsystem for that world

#### Scenario: Re-registration overrides
- **WHEN** a name is registered again
- **THEN** the new registration SHALL replace the previous one and the override SHALL be logged

#### Scenario: Unknown subsystem
- **WHEN** a world requests a name that is not registered
- **THEN** creation SHALL yield no subsystem (and report the miss)

#### Scenario: Declared config type and default
- **WHEN** a registration declares a config type
- **THEN** the registry SHALL expose that type and a default config instance the editor can use

#### Scenario: No config
- **WHEN** a subsystem is registered without a config type
- **THEN** it SHALL build with an empty config and use its own defaults

#### Scenario: Config validation
- **WHEN** a config is rejected by the subsystem's validator (which supplies a reason)
- **THEN** in develop/debug builds the build SHALL assert, and in release builds the entry SHALL be skipped,
  the reason logged, and the remaining subsystems SHALL still be built

### Requirement: Declarative world construction

A world SHALL be constructible from a `WorldDesc` that lists enabled subsystems (ordered) with per-subsystem
config, instead of hosts hardcoding concrete `new` calls. Subsystems SHALL be created through the registry and
attached in the described order; lifecycle hooks SHALL fire as today.

#### Scenario: Build from a description
- **WHEN** a world is built from a `WorldDesc` enabling physics and render scene
- **THEN** those subsystems SHALL be created via the registry, attached in order, and resolvable by name

#### Scenario: Disabled entry
- **WHEN** a `WorldDesc` entry is disabled
- **THEN** that subsystem SHALL NOT be created

#### Scenario: Manual attach still works
- **WHEN** a host attaches a subsystem explicitly
- **THEN** it SHALL behave as today (world owns it; resolvable by name)

### Requirement: Project-level world subsystem configuration (Sandbox editor)

The **Sandbox** editor SHALL expose a **project-level** world configuration surface (separate from user
preferences) listing the registered subsystems and letting the user enable/disable each and edit its
configuration; the selection SHALL be stored in and serialized with the project/world document.

#### Scenario: List subsystems
- **WHEN** the user opens the project's world configuration in the Sandbox editor
- **THEN** the editor SHALL list the registry-known subsystems with their enabled state

#### Scenario: Separate from user preferences
- **WHEN** the user edits a subsystem's configuration
- **THEN** it SHALL be stored with the project/world document, not in the user `Preferences` file

#### Scenario: Sandbox only
- **WHEN** the world subsystem configuration is implemented
- **THEN** it SHALL target the Sandbox editor; the deprecated Qt editor has been removed

#### Scenario: Edit a subsystem's config
- **WHEN** the user selects a subsystem that declares a config type
- **THEN** the editor SHALL render its config fields (via reflection) from the default or stored instance and
  write edits back to the world's stored description

#### Scenario: Persist selection
- **WHEN** the user disables a subsystem and saves the world
- **THEN** the world's stored description SHALL reflect the change and rebuild accordingly on load
