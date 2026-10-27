---
title: "World Subsystems"
description: "Declarative world subsystems: the process-wide registry, WorldDesc, JSON persistence, and the editor world-config surface."
updated: "2026-10-07"
---

## Scope

How a `sky::World` is assembled from **registered subsystems** instead of hard-wired code:

1. A process-wide **`WorldSubSystemRegistry`** where plugins declare how to create a subsystem.
2. A declarative **`WorldDesc`** (an ordered set of `{name, config, enabled}`) that `World::Build`
   turns into live subsystems.
3. **JSON persistence** of a world's subsystem set through the existing `World::SaveJson/LoadJson`.
4. The Sandbox editor's **world-config surface**: a dockable panel that lists registered subsystems,
   enables/disables them, and edits each subsystem's reflected config via `ReflectedFormView`.

The engine-side seam lives in `engine/framework` (`framework/world/WorldSubSystemRegistry.h`,
`framework/world/WorldDesc.h`); the subsystem **implementations** stay in their plugins. Per
`AGENTS.md`, `engine/*` defines the interface and never depends on a plugin.

## Why a registry

A world previously attached subsystems by direct calls (e.g. the physics backend's
`PhysicsBackendRegistry::SetWorldAttacher`, or a nav factory wired by hand in a builder). That couples
the host to specific subsystems and makes "which subsystems does this world have, and with what
config?" impossible to answer generically — and impossible to serialize or edit.

The registry inverts this: a plugin **registers once** (name, factory, optional reflected config type,
default-config maker, optional validator), and any host builds a world from a data description. The
host links no plugin; it only knows registry names.

## Registry (framework)

`WorldSubSystemRegistration` is everything a subsystem declares:

| Field | Meaning |
|---|---|
| `factory(World &, const Any &config)` | Builds the subsystem (returns `std::unique_ptr<IWorldSubSystem>`). |
| `configType` | Reflected type of the config (`nullptr` = no config). Enables persistence + editing. |
| `makeDefaultConfig()` | A default config `Any` the editor uses when a world has none yet. |
| `validate(config, error)` | Optional; returns `false` + reason to reject a config. |

`WorldSubSystemRegistry` is a **`Singleton<T>`** (`framework/world/WorldSubSystemRegistry.h`) so a
single instance is shared **across module DLLs** (stored in the process `Environment`). This is
critical: `Framework`/`Core` are static libs linked separately into each module DLL, so a plain
function-local static would give each DLL its own copy and plugins' registrations would be invisible
to the host. `Register` rejects an existing name; only the successful registrant unregisters it. `GetNames` returns the registered set. Registrations and live subsystems retain their module code leases.

## `WorldDesc` and `World::Build`

```cpp
struct WorldSubSystemDesc {
    Name name;
    Any  config;            // optional, matches the registration's configType
    bool enabled = true;    // order is preserved
};
struct WorldDesc {
    std::vector<WorldSubSystemDesc> subSystems;
};
```

`World::Build(const WorldDesc &desc)` is a one-time initialization operation returning `bool` and `GetLastError()`:

1. Validate duplicate names, enabled registrations/configuration and each component's declared subsystem requirements.
2. Construct all subsystem candidates before attaching any of them.
3. Attach subsystems, then attach the loaded actors/components through the fallible attachment boundary.
4. Roll back new attachments on failure; the document/Play factory publishes only a successful candidate.

`LoadJson` returns a result and prepares detached actors. It constructs components only through `ComponentFactory`'s typed factories. Hierarchy is resolved before actor attachment. Subsystems no longer need feature-specific scans to repair actor load order.

`GetMutableWorldDesc()` supplies authoring settings. Editing them does not reconfigure an already built runtime: the editor applies them to the next fresh Play world. Calling Build twice returns an error. `TryAttachToWorld` retains the caller's actor on attachment failure; attachment callbacks must not modify the world's actor collection.

## Persistence

`World::SaveJson/LoadJson` serialize the subsystem set alongside actors:

```jsonc
{
  "subSystems": [
    { "name": "Physics",    "enabled": true,  "config": { /* reflected config */ } },
    { "name": "Navigation", "enabled": false }
  ]
}
```

The `config` object is written with `SaveValueObject(Any)` and read back with
`LoadValueById(configType->registeredId)`, so it round-trips through the reflected type registered by
the plugin. All descriptor names/enabled flags are restored; typed config data requires the corresponding registered config type. An enabled missing provider fails Build rather than being silently skipped.

## Plugin registration (examples)

- **Physics** (`plugins/jolt/adaptor/JoltModule.cpp`): registers under `PHYSICS_SYSTEM_NAME` (`"Physics"`)
  with `PhysicsSubSystemConfig` (8 fields) as its config type; the factory creates a `PhysicsSystem`.
- **Navigation** (`plugins/recast/RecastModule.cpp`): registers under `NavigationSystem::NAME`
  (`"Navigation"`) with `NavigationSubSystemConfig` (6 fields); the factory creates a
  `NavigationSystem`.

Both modules unregister on `Shutdown`, and both list themselves in `configs/modules_editor.json` so
the editor loads them. For the editor to see these subsystems, the modules must be in that manifest.

## Editor world-config surface

- **`WorldDocument`** owns the authoring world and loads into a fresh candidate. A failed load preserves the current document. `CreatePlayWorld` serializes that authoring state, loads and builds a new runtime; failure prevents Play.
- **`WorldConfigPanel`** binds the generic reflected form to the selected config. Displaying a row/default config does not enable a subsystem. Checkboxes and fields use CommandService, mark the document dirty and support Undo/Redo; Save persists them, and the next Play applies them.
- Config values may move when the subsystem vector grows. History commands therefore capture document lifetime + subsystem name + type, and resolve the current object for every edit/undo/redo. Reload invalidates old commands. A configuration structure revision triggers form rebinding, while normal field edits retain their interaction state.
- Reflected bool setters report rejection through property commands and CommandService; failed edits do not enter the undo history. The form's reset baseline remains the reflected type default.
- **Menus**: **File > New World…** (name + location dialog → create + save a new `.world`),
  **File > Open World…** (file browser filtered to `*.world`), **File > Save World** (`Ctrl+S`), and
  **File > Close World** (`Ctrl+W`, saves if dirty then unloads); **File > Quit** saves a dirty world
  before exiting.

Configuration is saved through the normal document Save/close flow; toggling or selecting a row does not auto-save.

## Component and actor creation

`ComponentFactory::ComponentInfo` declares a typed constructor and `requiredSubSystems`. The editor verifies availability and enables requirements together with component insertion in one undo step. Physics declares `Physics`; animation declares `Animation`. Build also enforces those requirements for serialized scenes, including an explicit failure if a required subsystem is disabled.

`EditorActorCreation` holds preset id, label/group, detached-actor callback and DLL lease. Sandbox derives creation actions from its revision and invokes common actor commands; there is no host enum/switch for creator types. Removing a registration disables stale actions. Commands retain the creator lease for undo even after its menu entry disappears.

## Tests and limits

- Framework: typed/non-component UUID construction, descriptor validation, missing/failing factory, rollback, one-time Build and actor load lifecycle.
- EditorCore: configuration reallocation, failure-aware history, reload invalidation, next-Play settings and undoable enable/disable.
- EditorShell/SandboxProject: reflected configuration, generic component dependencies, live creator menu registration, stale callbacks and undo code leases.
- Jolt: loaded actor binding after subsystem construction, missing collision product rollback/retry, static/kinematic input and dynamic PostTick output.

Hot replacement of running subsystems is not implemented. Subsystem factories must report failure with a null result; fallible component hooks return a reason and support partial cleanup via OnDetachFromWorld. Runtime services still use Init/Shutdown symmetry and process-owned metadata.
