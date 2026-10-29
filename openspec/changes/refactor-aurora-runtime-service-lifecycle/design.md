## Context

The animation workflow defines `IAnimationSkeletonSource` in the consumer's engine interface and implements it in Aurora Adaptor using Aurora Skeleton products. The existing `Aurora.Render` module publishes this service before `Animation.World` loads. Its source, asynchronous requests and results carry an actual code lease because virtual calls and destruction can outlive module sessions.

`AuroraModule` also serves as a directly constructible RendererHost/Launcher adapter. Skeleton registration does not belong to that host's initialization contract. The user explicitly approved moving that lifecycle to runtime assembly.

## Decisions

1. Replace only the factory's concrete entry class in `runtime/src/AuroraRegistry.cpp`. A runtime `IModule` composes an `AuroraModule` and the Skeleton source owner. It delegates Start/Tick to the renderer adapter. Existing module names, loader dependencies, targets and static tagged-factory compile variant are retained.
2. Remove all Skeleton source members, includes and registration/teardown from `AuroraModule`. Direct provider-only and host initialization no longer require a module manager or inspect the global Skeleton service.
3. Runtime Init resolves the active ModuleManager, obtains the real `Aurora.Render` lease, constructs/registers the source, and initializes the renderer adapter. Missing lease and conflicting Skeleton owner are explicit Init failures. Renderer failure rolls back source publication and preserves services owned by others.
4. Runtime Shutdown first withdraws its own Skeleton registration, then drains/shuts down the renderer adapter, then destroys its source. Requests/results independently retain code and asset leases. Shutdown is safe after partial failure and on repeated calls; successful session re-Init does not duplicate registrations.
5. Keep metadata registration exclusively in `Aurora.Metadata::RegisterMeta`; this change introduces only restartable runtime service ownership. Do not move runtime registration into metadata callbacks, and do not silently adopt or skip another registration owner.

## Ownership and ordering

`ModuleManager -> AuroraRuntimeModule -> {Skeleton source, AuroraModule renderer}`.

The loader still initializes `Aurora.Render` before `Animation.World`. Its reverse shutdown order drains animation consumers before Aurora services are withdrawn. The runtime entry then applies `withdraw source -> drain host -> release source`. External requests/results may outlive all of those steps while retaining the DLL code; this does not keep the interface published or make a stopped service available.

The source implementation remains in `Aurora.Adaptor.Static`, compiled once and linked into the module. Runtime orchestration remains in the existing `AuroraRegistry.cpp` owner. The existing static-module test's exact compile variant continues to validate tagged entry registration.

The user subsequently clarified that plugin reload/removal is intended to take effect after process restart, with DLLs resident for the process lifetime. This change preserves the current lease API solely to complete the approved ownership split without a partial loader-contract migration; it adds no hot-unload capability. Removing distributed code leases requires the loader to guarantee process-lifetime DLL retention and service shutdown to drain outstanding work. That Framework contract change is outside this implementation.

## Validation

- Direct adapter tests: provider-only and host Init with no Skeleton source/ModuleManager; existing external source preserved on success, host failure and shutdown.
- Module tests: provider-only service publication and unregistration, partial Init rollback, duplicate owner isolation, restart, and retained requests/results after manager destruction in dynamic/static packaging.
- Animation production module test: dependency order, real source consumption, shutdown and outstanding request leases.
- Build the real adaptor and runtime module, both renderer module test variants and animation module tests in the existing VS 2022 cache. No build organization change is needed.

This is separate from the earlier renderer ECS/GPU orchestration refactor and Party integration changes. Commit/archive remain subject to the repository's confirmation rules.
