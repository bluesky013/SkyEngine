## Why

The network layer already defines a `Framework`-free replication seam (`IReplicationSource`) and an ECS adapter, and `network-replication-source` requires the same algorithm over both the `Actor`/`ComponentBase` model and the ECS. The framework adapter does not exist, and the OOP scene lacks dense per-type access and per-field replication flags. It needs no new structural-change machinery: the network host runs on the same thread and applies received spawn/despawn/state at a defined point after the world update.

## What Changes

- Add a dense per-type component index to `World` so replicated components iterate in batches with deterministic order (by stable actor id), matching the seam's requirement. (The O(1) actor index already exists.)
- Add `ActorReplicationSource : IReplicationSource` (links `framework`) that encodes/applies replicated state through reflection + binary serialization, reading the `REPLICATED` metadata flags.
- **Define an apply point** instead of a command sink: the network host resolves spawn/despawn and applies state **after `World::Tick`, on the same thread** (no framework structural-change API is added; the framework is untouched).
- **Provide a side-effect-free apply path**: applying replicated fields MUST NOT trigger gameplay side effects (for example `PhysicsBodyComponent::SetMass -> Recreate`). Provide a replication-apply guard/variant so setter side effects are suppressed during application.
- Keep authority and spawn/despawn ownership in the network layer (keyed by stable actor id); no authority state is added to `Actor`/`ComponentBase`.
- Keep the replication algorithm dependent only on the seam, never on `Actor`/`ComponentBase`.

## Capabilities

### New Capabilities
- `actor-replication-source`: the framework-side replication adapter over `Actor`/`ComponentBase`: dense per-type access, metadata-driven field replication with a side-effect-free apply path, and a defined apply point.

### Modified Capabilities
<!-- network-replication-source defines the seam; this adds the framework implementation. -->

## Impact

- Code: `engine/framework/include/framework/world/World.h`, `engine/framework/src/world/World.cpp` (per-type index only), new `ActorReplicationSource` adapter (framework-linked).
- Depends on `reflection-replication-fields` (the `REPLICATED` flags) and `harden-framework-components` (deterministic order).
- No change to `engine/network` algorithm or `engine/core/ecs`.
- Consumes the network layer's authority/spawn-despawn ownership; does not add those to the framework.
