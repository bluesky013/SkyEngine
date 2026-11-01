# navigation-aurora-integration Specification

## Purpose

Define the ownership and lifecycle boundaries between renderer-independent navigation, plugin runtime and cooking, editor authoring, and optional Aurora debug visualization.

## Requirements

### Requirement: Independent navigation responsibilities

Runtime SHALL expose engine contracts without editor/renderer dependencies. Plugin implementation, editor authoring, offline cooking and rendering SHALL have independent source owners. Tests SHALL link production implementation targets.

#### Scenario: Headless navigation

- **WHEN** the backend loads without editor/render modules
- **THEN** worlds SHALL create navigation, generate/load meshes and query paths without renderer calls or editor actions

#### Scenario: Consumer boundary

- **WHEN** terrain consumes navigation
- **THEN** it SHALL use an engine navigation interface instead of the Recast world implementation

### Requirement: Deferred editor build

Build Navigation SHALL register through the common action registry, use the active editor world, enqueue work and report explicit failure. Duplicate in-flight builds SHALL be suppressed. Runtime factory construction SHALL NOT register editor operations.

#### Scenario: Build action

- **WHEN** the action is invoked for a navigation world
- **THEN** editor Tick SHALL initiate generation and observe completion outside event dispatch

#### Scenario: Collision resources are pending

- **WHEN** a queued editor build encounters components whose world preparation is still pending
- **THEN** the editor SHALL poll their explicit state on later frames and defer geometry gathering without blocking the UI or advancing simulation to force readiness

#### Scenario: Component attachment fails

- **WHEN** a component reports a world-preparation or native binding failure
- **THEN** the editor SHALL stop that queued build and report the actor, component and cause instead of generating from incomplete geometry

#### Scenario: Editor shutdown

- **WHEN** the editor module shuts down
- **THEN** its actions SHALL be removed and surviving work SHALL retain its required lifetime

### Requirement: Optional Aurora debug consumer

The render bridge SHALL consume plain navigation triangles and submit owned Aurora mesh resources/entities through the existing frame producer. An editor action SHALL control visibility. Core types SHALL NOT hold GPU/editor objects.

#### Scenario: Debug enabled

- **WHEN** debug visibility is enabled on a built mesh
- **THEN** walkable triangles SHALL be submitted into the active editor scene

#### Scenario: Debug retired

- **WHEN** visibility is disabled, the world changes or the bridge shuts down
- **THEN** stale debug entities/resources SHALL be retired

### Requirement: Canonical debug material binding and ordering

The navigation rendering bridge SHALL bind its tint to the resolved material parameter `gMaterial.tint` and submit its translucent debug surface after ordinary scene geometry using the existing scene-pass ordering contract. It SHALL retain depth testing and SHALL NOT write scene depth.

#### Scenario: Material parameters use resolved names

- **WHEN** the source scene material exposes `gMaterial.tint` rather than the authoring alias `tint`
- **THEN** the bridge SHALL apply its navigation debug tint to that canonical parameter

#### Scenario: Debug geometry overlaps ground

- **WHEN** navigation debug triangles overlap ordinary scene ground
- **THEN** their debug surface SHALL be ordered after ordinary geometry and alpha-blended with depth testing, without replacing or writing the ground's depth
