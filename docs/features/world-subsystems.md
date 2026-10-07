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
to the host. `Register` overrides (and logs) an existing name; `GetNames` returns the registered set.

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

`World::Build(const WorldDesc &desc)` stores the description and, for each **enabled** entry:

1. skips a subsystem already present (duplicate guard),
2. skips names with no registration (logged),
3. runs the registration's `validate` if present (asserts in debug, skips in release),
4. calls the factory and `AddSubSystem`s the result.

`World::GetWorldDesc()` returns the description the world was built from (null if built manually);
`World::GetMutableWorldDesc()` lazily creates a mutable description for an editor to edit **before**
`Build`. `AddSubSystem` and the `IWorldSubSystem` callbacks (`OnAttachToWorld`, `StartSimulation`,
`Tick`, …) are unchanged.

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
the plugin. On load, only entries whose name resolves to a registration **with a `configType`** are
restored. This replaces the previously unused `worldConfigs` map as the source of truth.

## Plugin registration (examples)

- **Physics** (`plugins/bullet/BulletModule.cpp`): registers under `PHYSICS_SYSTEM_NAME` (`"Physics"`)
  with `PhysicsSubSystemConfig` (8 fields) as its config type; the factory creates a `PhysicsSystem`.
- **Navigation** (`plugins/recast/RecastModule.cpp`): registers under `NavigationSystem::NAME`
  (`"Navigation"`) with `NavigationSubSystemConfig` (6 fields); the factory creates a
  `NavigationSystem`.

Both modules unregister on `Shutdown`, and both list themselves in `configs/modules_editor.json` so
the editor loads them. For the editor to see these subsystems, the modules must be in that manifest.

## Editor world-config surface

- **`WorldDocument`** (`engine/sandbox/core`, `editor/core/document/WorldDocument.h`) owns a
  `sky::World` and `Load`/`Save`s it through the framework JSON archive (the `.world` file). It also
  exposes the config surface: `SetSubSystemEnabled`, `IsSubSystemEnabled`, `Rebuild`
  (rebuilds from the description, adding enabled missing subsystems), and `EnsureSubSystemConfig`
  (finds or creates the stored config from the registry default so a form can edit it in place).
  **Pointer invariant**: `EnsureSubSystemConfig` returns a pointer into `WorldDesc::subSystems`, and
  `sky::Any` stores its value inline — so any later append (`SetSubSystemEnabled` / `EnsureSubSystemConfig`
  adding an entry) reallocates the vector and invalidates previously returned pointers. Callers must
  re-fetch (the panel rebinds after every toggle).
- **`WorldConfigPanel`** (`engine/sandbox/shell`) is a dockable panel (tabbed with the Outliner):
  the left side lists registered subsystems as rows with an **enable checkbox** (subsystems are enabled by
  default; clicking the checkbox toggles + `Rebuild`s + `Save`s so the selection persists with the world) and
  the subsystem name; clicking the name selects it. The right side hosts a persistent `ReflectedFormView`
  bound to `PropertyObject{ config.Data(), GetTypeNode(config) }` for the selected subsystem. Selecting a row
  persists pending edits (via `WorldDocument::Save`) before rebinding.
  Before a world is opened the panel shows a **"No world open"** empty state (it never fabricates
  defaults), and it rebinds automatically when a world is opened/closed. Edits through the form mark
  the document dirty (`ReflectedForm::SetOnChanged` → `WorldDocument::MarkDirty`), so Save persists
  them and the title/status show the unsaved `*`. The reflected form's **reset baseline is the type's
  default-constructed value** (`ReflectedForm` / `MakeDefaultValue`), so a field changed from the
  default keeps its reset button across load/save and reset writes the default back (same rule for the
  Inspector). `EnsureSubSystemConfig` still uses the registration's `makeDefaultConfig()` to create a
  missing config, which is a default-constructed config for the built-in subsystems.
- **Menus**: **File > New World…** (name + location dialog → create + save a new `.world`),
  **File > Open World…** (file browser filtered to `*.world`), **File > Save World** (`Ctrl+S`), and
  **File > Close World** (`Ctrl+W`, saves if dirty then unloads); **File > Quit** saves a dirty world
  before exiting.

Config edit persistence is currently triggered on subsystem switch and on quit, not on every
keystroke.

## Tests

- `FrameworkTest` `WorldSubSystemRegistryTest` — register/create + override, unknown-name skip,
  `Build` ordering + disabled entries, duplicate guard, validator accept.
- `EditorCoreTest` `WorldDocumentTest` — enable/disable + save/load round-trip.
- `EditorShellTest` — world-config panel toggles a subsystem.

## Current limitations / follow-ups

- **Config → runtime mapping**: a subsystem's reflected config is stored and edited, but (except where
  a factory consumes it) the values are not yet applied to the runtime subsystem. Physics still routes
  through `PhysicsBackendRegistry::SetWorldAttacher`; folding that into the registry is deferred until a
  non-render host actually attaches physics.
- **Removal**: `WorldDocument::Rebuild` only adds enabled, missing subsystems; it does not remove
  disabled ones from a live world yet.
- **Render scene subsystem** (`IRenderSceneSubSystem` + the aurora bridge) is designed but not yet
  registered; it lands with the render main loop.
