## Why

Process-global metadata - reflection types, asset types/handlers, component groups, and cook builders -
was registered inside `IModule::Init`/`Start`. But a module's load lifecycle is **repeatable** (unload/reload is
supported; the same type can also be reachable from several module DLLs that each link the same `*.Static` library),
while the registration targets are **process-global and are not torn down on unload** (reflection cannot be, and must
not be, unregistered because other modules may depend on it). The two lifetimes do not match: a second load
re-registers, which hit `SKY_ASSERT(!types.count(id))` (`SerializationContext::Register`) and crashed, or silently
grew `ComponentFactory`/`AssetBuilderManager` lists. `RecastModuleLoadTest.ReloadAfterUnload` reproduces the crash.

The previous mitigation - hand-written `sky::Once`/idempotent guards at each callsite - is a patch: it is easy to
forget (recast did) and it is inconsistent. The root cause is that a process-once concern was hung on a
repeatable lifecycle.

## What Changes

- Add a dedicated **process-once module phase** `IModule::RegisterMeta()`, run by `ModuleManager` at most once per
  process per module (guarded process-globally), never re-run on reload.
- **Move the persistent registrations** (reflection `Reflect(...)`, `RegisterAssetHandler`, `RegisterXxxAssetType`,
  component-group registration, offline cook `RegisterBuilder`) out of `Init`/`Start` and into `RegisterMeta`.
- Keep the **per-load registrations** (backends, factories, world subsystems, compressor) in `Init`/`Start`, because
  `Shutdown` undoes them - those are already symmetric under reload.
- Revert the earlier idempotency patches (`SerializationContext`/`ComponentFactory`/`CompressionManager`/
  `AssetBuilderManager`): with registration running once, they are unnecessary and would mask a regression.
- Do not touch the deprecated legacy render stack (`engine/render`, `engine/shader`).

## Capabilities

### New Capabilities

- `module-registration`: the module lifecycle contract that separates process-once metadata registration
  (`RegisterMeta`) from the repeatable per-load lifecycle (`Init`/`Start`/`Shutdown`).

### Modified Capabilities

<!-- none. -->

## Impact

- `engine/framework/include/framework/interface/IModule.h` - adds `RegisterMeta()`.
- `engine/framework/src/application/ModuleManager.cpp` - runs `RegisterMeta` once per module before `Init`.
- Feature modules move their registrations: `plugins/recast`, `plugins/bullet`, `plugins/audio` (+ builder),
  `plugins/terrain` (runtime + builder), `plugins/vegetation`, `engine/content` (runtime + builder),
  `engine/aurora/adaptor` (`AuroraModule`), `engine/aurora/cook`, `engine/navigation/builder`.
- No behavioral change for a normal single-load process; reload and multi-DLL are now safe by construction.
