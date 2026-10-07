## 1. Registry + declarative construction (no render dependency)

- [x] 1.1 Add `WorldSubSystemRegistry` (framework): `Register(const Name&, WorldSubSystemRegistration)` with factory + optional `configType`/`makeDefaultConfig`/`validate`; `GetRegistration/Unregister/IsRegistered/Create/GetNames/Clear`; re-registration overrides (logged).
- [x] 1.2 Add `WorldSubSystemDesc` / `WorldDesc` and `World::Build(const WorldDesc&)`; validate config before creating (debug assert / release skip); skip duplicates/unregistered; keep `AddSubSystem` and no behavior change to `IWorldSubSystem` callbacks.
- [x] 1.3 Tests: register/create+override, unknown skip, `Build` ordering + disabled, duplicate guard, validator accept. (`FrameworkTest`, 4 cases.)
- [ ] 1.4 Fold `PhysicsBackendRegistry::SetWorldAttacher` into the registry: the physics **backend** registers a factory under `PHYSICS_SYSTEM_NAME` that routes to the **currently active** backend (via `PhysicsBackendRegistry`), keeping `IPhysicsSystem`. **Deferred in P1:** the only physics host is the deprecated Qt editor; fold this when a non-render host (Sandbox) actually attaches physics.
- [x] 1.5 Migrate **non-render** hosts to `World::Build`: `engine/navigation/builder/src/NaviMeshBuilder.cpp` (registers a nav factory + builds). Legacy-render hosts (`SampleScene`, Qt `Document`) left as-is until P2. (Follow-up: move the nav registration into the nav module's init.)
- [x] 1.6 Serialize a world's subsystem set (`WorldSubSystemDesc { name, config, enabled }`) in `World::SaveJson/LoadJson` (`config` via `SaveValueObject(Any)` / `LoadValueById`); `World::GetWorldDesc()`. Replaces the unused `worldConfigs` as the source of truth (follow-up: drop `worldConfigs`).

## 2. Aurora render-scene bridge (render phase — depends on the render main loop)

- [ ] 2.1 Add an engine-side `IRenderSceneSubSystem` (consumer module) exposing the aurora `RenderScene` seam.
- [ ] 2.2 Add the aurora render-scene bridge subsystem (adaptor) implementing it; register it in `WorldSubSystemRegistry` under a stable name.
- [ ] 2.3 Wire main-thread scene/viewport intents through the bridge to the renderer seam (`aurora-scene-bridge` / render loop).

## 3. Editor world hierarchy / configuration

- [x] 3.1 **Sandbox** editor: world config lives in a dockable **Config panel** (`WorldConfigPanel`, sibling/tab of the Outliner), listing registry subsystems with enable/disable; toggling applies immediately (`WorldDocument::Rebuild+Save`). Menus: **File** = file ops + Preferences (`New World…`, `Preferences…`, `Quit`); the Config panel is in the default layout + View menu. `EditorShell::SetWorldDocument`; `EditorShellTest.WorldConfigPanelTogglesSubsystem`. (World config is NOT in user Preferences nor a project-settings dialog.)
- [x] 3.2 Per-subsystem config **editing** in the Config panel: selecting a subsystem hosts a `ReflectedFormView` bound to `PropertyObject{ config.Data(), GetTypeNode(config) }` (`WorldDocument::EnsureSubSystemConfig` creates the config from the registry default), editing in place; config persists through `WorldDocument::Save`.
- [ ] 3.3 Tests + manual verify: toggle a subsystem, save, reload, world rebuilds with the stored set.
- [x] 3.4 Sandbox project-world document: `editor/core/document/WorldDocument` owns a `sky::World`, `Load/Save` via the framework JSON archive, and exposes the subsystem desc (enable/disable, rebuild). `WorldDocumentTest` round-trips save/load.

## 4. Docs

- [x] 4.1 Document the subsystem registry + `WorldDesc` + the editor world config in `docs/` (`docs/features/world-subsystems.md`).
