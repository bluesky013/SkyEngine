## Context

Two scene models coexist: framework `World`/`Actor`/`ComponentAdaptor<Data>` (OOP authoring) and aurora's data-oriented scene ECS in `engine/aurora/core`. Aurora adaptor components already hold `Uuid` + POD and asset holders. Missing: the transfer path into the scene ECS. This extends the existing `aurora-adaptor` capability.

## Goals / Non-Goals

**Goals:**

- One-way transfer framework scene → aurora scene ECS.
- Render state lives only in aurora.
- Components stay render-free at the persistence layer.

**Non-Goals:**

- The legacy `engine/render` renderer.
- Two-way synchronization.
- Changing aurora RHI or the framework component API.

## Decisions

### D1: `RenderSceneBridge` subscribes to lifecycle and transform

Lives in `engine/aurora/adaptor`; subscribes to actor/component attach/detach and `TransformComponent` changes; creates/removes scene ECS entries and writes world transforms. Reuses the existing `Event<T>` mechanism and the corrected transform propagation.

### D2: Strictly one-way

Reads framework state, writes ECS state; never reads ECS back into components.

### D3: Persistence layer stays render-free

The bridge consumes only `Uuid` + POD members and the transform world matrix. Non-serialized runtime caches (asset handles) are allowed on adaptor components but not in persisted data (see `harden-framework-components` decoupling rules).

### D4: Frame sync point

Transfer per frame at a defined point after the framework update and before aurora build, plus event-driven create/remove on attach/detach.

## Risks / Trade-offs

- [Frame sync point ordering] → Define one point after `World::Tick` and before the aurora build.
- [Asset readiness lag] → Defer the ECS entry until referenced assets load (reuse `IAssetReadyNotifier`).
- [Transform correctness] → Depends on the fixed propagation; test world matrices survive the transfer.

## Migration Plan

- Additive adaptor; depends on `harden-framework-components`.
- Rollback: remove the bridge.

## Open Questions

- First components covered (static mesh / light / camera).
- Entity identity mapping (`Actor` uuid → aurora `EntityId`).
