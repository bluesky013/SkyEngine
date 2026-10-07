## Context

`framework/world/World.h` already defines the seam: an owned `IWorldSubSystem`
(`OnAttachToWorld/OnDetachFromWorld/StartSimulation/StopSimulation/Tick`), `AddSubSystem(Name, IWorldSubSystem*)`,
`GetSubSystem(Name)`, and per-world `RegisterConfiguration`/`GetConfigByName`. But **creation is hardcoded
per host**: `Document.cpp`, `SampleScene.cpp`, and `NaviMeshBuilder.cpp` each `new` the concrete subsystems
with hand-written `Name(...)` strings. `PhysicsBackendRegistry` already shows the intended pattern (a plugin
registers a `PhysicsWorldAttacher`). The aurora render scene has no world-subsystem bridge yet.

## Goals / Non-Goals

**Goals:** one registry + a declarative `WorldDesc` so a world's subsystem set is data, not host code; fold
physics into it; add an aurora render-scene bridge subsystem; a **project-level** world subsystem/config
surface.

**Non-Goals:** the render main loop (render thread, command mailbox, per-scene pipeline) — separate; the
actor outliner; changing `IWorldSubSystem`'s callback set; putting this under user `Preferences` (that is
user-habit settings — see Decision 7).

## Decisions

1. **Registry of registrations.** `WorldSubSystemRegistry` (singleton). Plugins register once at module init
   under a stable `Name`:

   ```cpp
   using WorldSubSystemFactory = std::function<std::unique_ptr<IWorldSubSystem>(World &, const Any &config)>;

   struct WorldSubSystemRegistration {
       WorldSubSystemFactory               factory;              // required
       const TypeInfoRT                   *configType = nullptr; // reflected config type; null = no config
       std::function<Any()>                makeDefaultConfig;    // optional; default instance for the editor
       std::function<bool(const Any &, std::string &error)> validate; // optional; false + reason
   };

   bool Register(const Name &, WorldSubSystemRegistration);       // re-registering a name OVERRIDES (logged)
   void Unregister(const Name &);
   bool IsRegistered(const Name &) const;
   const WorldSubSystemRegistration *GetRegistration(const Name &) const; // for the editor (type/default)
   std::unique_ptr<IWorldSubSystem> Create(const Name &, World &, const Any &config) const;
   std::vector<Name> GetNames() const;
   void Clear(); // test-only isolation
   ```

   Re-registering the same name **overrides** the previous entry (supports hot-reload/dev iteration) and is
   logged. Keys are `Name` strings, consistent with the existing subsystem API.

   **Validation failure semantics.** `World::Build` validates an entry's config before creating it. On
   failure: **develop/debug builds assert** (fail loudly); **release builds skip** the entry, log the reason,
   and continue with the remaining subsystems. An **empty config passes** (validators return `true` when the
   config is absent).

   **Duplicate-entry guard.** `Build` skips a name already present on the world (e.g. a manual
   `AddSubSystem` or a repeated entry) instead of asserting/leaking, since `AddSubSystem` asserts on
   duplicates today.

2. **Config model (reflection-based, L1).** `WorldSubSystemDesc { Name name; Any config; bool enabled = true; }`.
   `Any` (`framework/serialization/Any.h`) is a type-erased value carrying runtime reflection (`TypeInfoRT*`),
   so a subsystem's config is a **registered reflected struct**; the registration declares its type, an
   optional default instance, and an optional validator. This reuses the framework reflection for editor
   forms and serialization and avoids a parallel config-class hierarchy (a polymorphic `WorldSubSystemConfig`
   base is explicitly rejected). Simple subsystems set `configType = nullptr` and carry an empty `Any`; the
   factory uses internal defaults. `configType` and `makeDefaultConfig` are independent (either may be absent).
   Factories must tolerate `config.GetAsConst<T>() == nullptr` (type absent/mismatch) and fall back to defaults.

3. **Declarative construction.** `WorldSubSystemDesc { Name name; Any config; bool enabled = true; }` and
   `WorldDesc { std::vector<WorldSubSystemDesc> subSystems; }`. `World::Build(const WorldDesc&)` creates each
   enabled subsystem via the registry (in listed order) and `AddSubSystem`s it; the existing lifecycle fires.
   `World::AddSubSystem` stays for explicit/manual use (keeps owning `unique_ptr`). `Build` only attaches — it
   does **not** call `StartSimulation`; play/pause remains host-driven.

4. **Interface resolution unchanged.** Consumers resolve a typed interface by a stable `NAME` + downcast
   (as today: `IPhysicsSystem`, `ai::NavigationSystem`). New: an engine-side `IRenderSceneSubSystem` in a
   consumer module (e.g. `engine/render` or `engine/aurora/adaptor`) exposing the aurora `RenderScene`.

5. **Physics folds in.** `PhysicsBackendRegistry::SetWorldAttacher` is replaced by the active backend
   registering a factory under `PHYSICS_SYSTEM_NAME` into `WorldSubSystemRegistry`; the factory must route to
   the **currently active** backend (through `PhysicsBackendRegistry`), not a captured backend, since backends
   are swappable. `IPhysicsSystem` is unchanged.

6. **Aurora render-scene bridge (render phase).** A bridge subsystem implementing `IRenderSceneSubSystem`
   owns/creates the aurora `RenderScene` and forwards main-thread scene/viewport intents to the renderer seam
   (`aurora-scene-bridge` / the render main loop). This change only defines/registers the bridge; the GPU
   work belongs to the renderer.

7. **Project-level world subsystem configuration (Sandbox editor).** The **Sandbox** editor exposes a
   **project-level** configuration surface (Project / World settings) — deliberately **separate from user
   `Preferences`**, which stays user-habit settings. It lists registry-known subsystems with enable/disable
   and, per subsystem, a config form built from the reflected `Any` (type + default via `GetRegistration`).
   The selection and config are stored in the project/world document and serialized (Decision 8). The sandbox
   editor links only the registry + reflection (no concrete subsystem plugins). The deprecated Qt
   `engine/editor` is out of scope. Dependency: the Sandbox editor does not own a world/document yet, so this
   surface lands once the sandbox gains a project-world (or that world is introduced as part of this work).

8. **World doc persistence.** `WorldSubSystemDesc` (name + reflected `Any` config + enabled) is serialized
   with the world (`World::SaveJson/LoadJson`) and becomes the single source of truth, replacing the
   currently-unused `World::worldConfigs` free-form map. `World.h` forward-declares `WorldDesc` to avoid
   pulling reflection into every `World.h` consumer.

## Phases

- **P1 (no render dependency):** `WorldSubSystemRegistry` + `WorldDesc`/`World::Build`; migrate the non-Qt
  hosts to `Build`; fold physics; project-level subsystem **list** (read-only + enable).
- **P2 (render):** `IRenderSceneSubSystem` + the aurora render-scene bridge registered in the world; depends
  on the render main loop seam (reserved).
- **P3:** per-subsystem config editing (reflected) in the project-level surface + persistence in the world doc.

## Risks / Trade-offs

- **[Lifecycle/order]** subsystems may depend on each other (render before physics read-back). Mitigation:
  `WorldDesc` is ordered; document ordering as part of the desc.
- **[Editor↔world sync]** the editor runs in-process today; the bridge must be safe with the render thread's
  mailbox — deferred to P2 with the render loop.
- **[Re-registration]** accidental name collisions now silently override; rely on the warning log. (Chosen
  over reject-first to allow hot-reload/dev iteration.)
- **[Migration churn]** hosts currently use `GetSubSystem(Name("RenderScene"))` literals; keep those `NAME`
  constants in the subsystem headers so callers migrate mechanically.
- **[Preferences vs project config]** world subsystem settings are project-scoped and serialize with the
  world doc; they must not be written into the user `editor-preferences.json`.
