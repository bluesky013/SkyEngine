## Why

World subsystems are attached **ad-hoc by each host**: `Document.cpp`, `SampleScene.cpp`, and
`NaviMeshBuilder.cpp` each `new RenderSceneProxy/PhysicsSystem/NavigationSystem` with hardcoded `Name(...)`
strings and manual ordering. There is no single place that declares "what subsystems a world has", so every
host must know every subsystem type, registration is duplicated, and the editor cannot configure a world's
subsystem set. Integrating the aurora render scene + physics needs one declarative seam.

## What Changes

- Add a **`WorldSubSystemRegistry`** (plugin factory registry, mirroring `PhysicsBackendRegistry`): each
  subsystem plugin registers, under a stable `Name`, a factory plus an optional **reflected config type**,
  **default config**, and **validator** (L1: config carried as a reflected `Any`, not a parallel config-class
  hierarchy); `World` resolves subsystems by name through the registry.
- Make world construction **declarative**: a `WorldDesc` (ordered subsystem names + per-subsystem config)
  describes a world; `World` instantiates and stops/starts subsystems from it. Keep `AddSubSystem` for
  explicit/manual use.
- Register **aurora render-scene** (a bridge subsystem in the sandbox/render adaptor) and **physics** through
  the registry; fold `PhysicsBackendRegistry::SetWorldAttacher` into it.
- **Sandbox editor**: a project-level world subsystem configuration surface (list subsystems, enable/disable,
  edit each subsystem's config) — separate from user `Preferences` — bound to the project/world doc and
  persisted. The deprecated Qt `engine/editor` is **out of scope** (not migrated).
- Hosts (`Document`, launcher, builder) switch from hand-wiring to building the world from a `WorldDesc`.

## Capabilities

### New Capabilities
- `world-subsystem-registry`: name-keyed subsystem factory registry + declarative `WorldDesc` construction +
  editor world-hierarchy configuration.

### Modified Capabilities
- *(none yet — `World`/subsystem APIs are implementation-level; `physics-*` may fold its attacher in a
  follow-up.)*

## Non-goals

- The aurora **render main loop** (render thread + command mailbox + per-scene pipeline) — developed
  separately; this change only provides the render-scene subsystem **bridge** registered in the world.
- Replacing the actor/entity ownership model (`world-entity-ownership`); this builds on it.
- A visual world-outliner hierarchy of actors (that is the outliner; here "world hierarchy" means the
  subsystem/configuration hierarchy).

## Impact

- `engine/framework`: new `WorldSubSystemRegistry`; `World` gains declarative construction + a `WorldDesc`.
- `engine/physics`: `PhysicsBackendRegistry` folds into the new registry (keeps `IPhysicsSystem`).
- `engine/aurora/adaptor` (or sandbox/render): a `RenderScene` bridge subsystem registered under a name.
- `engine/sandbox` (the target editor): the project-level world subsystem configuration surface; hosts build
  worlds from a `WorldDesc`.
- **Out of scope**: the deprecated Qt `engine/editor` (no migration; consistent with `world-entity-ownership`).
