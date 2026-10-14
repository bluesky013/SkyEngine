## Context

`ModuleManager::LoadModules` builds each module (creating the instance for dynamic modules) and calls `Init`, then
`StartModules` calls `Start`. `UnLoadModules` calls `Shutdown` and drops the instances. Load/unload/reload is a
first-class lifecycle (`UnLoadModules` + `StopModule` + `Environment::Detach` exist for it, and
`RecastModuleLoadTest.ReloadAfterUnload` exercises it).

In production `Application` calls `LoadModules` once, so duplicates do not appear there. The problem is structural:
modules register **process-global** things (types via `SerializationContext`, asset handlers via `AssetManager`,
component groups via `ComponentFactory`, offline builders via `AssetBuilderManager`) from the **repeatable** load
path. Those registries are singleton/process-wide and are never undone by `Shutdown` (and must not be - reflection
types may be shared by other modules). So any second load re-registers; the old code asserted
(`SKY_ASSERT(!types.count(id))`) and crashed. `RecastModuleLoadTest.ReloadAfterUnload` reproduced it. Earlier work
patched individual callsites with `sky::Once` / idempotent registries, which is inconsistent and forgettable.

## Goals / Non-Goals

**Goals:**

- Make registration of process-global metadata happen **exactly once per process per module**, by design, so reload
  and multi-DLL cannot duplicate it - without per-callsite guards.
- Keep the per-load lifecycle (`Init`/`Start`/`Shutdown`) for things that are actually started and stopped each load.

**Non-Goals:**

- No change to the deprecated legacy render stack (`engine/render`, `engine/shader`).
- No change to what is registered (same types/handlers/builders) or to loading order.
- Not making the registries idempotent (that masks the mistake rather than preventing it).

## Decisions

### Decision: A dedicated process-once phase `IModule::RegisterMeta()`

`IModule` gains `virtual void RegisterMeta()`. `ModuleManager` calls it **once per process per module**, before
`Init`, guarded by the process-global `sky::Once` with key `"ModuleMeta:" + moduleName`. On reload the `Once` returns
false and the phase is skipped; the first registration persists. This puts the "once" guarantee in the framework
where it cannot be forgotten by a module author.

- Alternatives: per-callsite `sky::Once` inside each `Reflect` (rejected: easy to omit, and every author must know
  the rule); idempotent registries such as `SerializationContext::Register` (rejected: turns a design error into
  silent tolerance and hides real conflicts); unregistering on unload (rejected: unsafe for shared process-global
  metadata and explicitly not wanted).

### Decision: What belongs in `RegisterMeta` vs `Init`/`Start`

The rule is **"is it undone in `Shutdown`?"**

- **`RegisterMeta` (process-once, never undone)** - reflection `Reflect(...)`, `AssetManager::RegisterAssetHandler`,
  `RegisterXxxAssetType`, `ComponentFactory::RegisterComponent`, offline `AssetBuilderManager::RegisterBuilder`.
- **`Init`/`Start` (per-load, undone by `Shutdown`)** - physics/audio backends, `NaviMeshFactory`,
  `WorldSubSystemRegistry` entries, `CompressionManager` compressor, shader compiler factory. These are already
  symmetric under reload (Shutdown unregisters) so they stay on the load path.

### Decision: Shared entry points keep their internal `Once`

`AuroraReflection` and `ContentReflection` are invoked by multiple modules (runtime/cook/editor). This change does
not remove their internal `sky::Once`; that guards the genuinely cross-module shared registration (per-module
`RegisterMeta` would run once per calling module, i.e. several times). The per-module `Once` and the shared entry
point's `Once` cover complementary cases: reload vs. multi-owner.

### Decision: Revert the idempotency patches

The earlier `SerializationContext`/`ComponentFactory`/`CompressionManager`/`AssetBuilderManager` idempotency edits
are reverted. With registration on the process-once path they are unnecessary, and keeping them would hide a future
regression where someone registers persistent state from `Init` again.

## Risks / Trade-offs

- [A module author puts a persistent registration in `Init` out of habit] -> The registries stay strict (assert),
  so the reload test / a second module load still fails loudly. The rule is documented on `RegisterMeta`.
- [`Once` key collides across two different modules with the same name] -> Module names are unique target names
  (the same key used for load order), so no practical collision.
- [`RegisterMeta` running before `Init` changes ordering for a module that reads its own types during `Init`] ->
  Registration is additive and idempotent-per-process; `RegisterMeta` running first is strictly safer.
- [Editor modules (`Aurora.Render.Editor`, `Content.Views`) also call the shared reflection] -> Unchanged; they rely
  on the shared entry point's internal `Once`, exactly as before.

## Migration Plan

1. Add `RegisterMeta` to `IModule` and run it once in `ModuleManager`.
2. Move persistent registrations from `Init`/`Start` into `RegisterMeta` for the affected modules.
3. Revert the idempotency patches.
4. Verify: build + full `ctest` (includes `RecastModuleLoadTest.ReloadAfterUnload`). Rollback is reverting the
   moves; no data/format change.

## Open Questions

- Should `RegisterMeta` eventually be used to drop `AuroraReflection`/`ContentReflection`'s internal `Once` by
  designating a single owning module? Deferred; the current combination is correct and minimal.
