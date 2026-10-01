## Why

`add-network-core` deliberately stops at transport and sessions; it synchronizes no game state. A server-authoritative multiplayer game is useless without a replication layer, yet that layer is genre- and world-specific and must not leak into the frozen core. This change introduces replication as the `engine/network/world` bridge, keeping `engine/network` unchanged and allowing the World/ECS model to evolve independently.

## What Changes

- **New `engine/network/world` bridge** (target `NetworkWorld`, links `Network` + `Framework`); `engine/network` core is not modified. Mirrors the `engine/navigation/terrain` bridge pattern.
- **Data-driven replication source seam.** Replication reads simulation state through an engine-side source interface that exposes batched, dense iteration with stable, cross-module type ids and per-field (SoA) columns. Both the `Actor`/`ComponentBase` model and the data-oriented ECS (`core/ecs`) adapt to it, so replication is not bound to one simulation model.
- **Component/property replication authoring.** Replicated components declare which fields replicate, built on the existing reflection and `ComponentBase::SaveBin`/`LoadBin` serialization seam. Clients hold read-only replicas.
- **Server-authoritative snapshot + delta.** Each fixed network tick the server builds a per-connection snapshot from relevant components, delta-compresses it against that connection's last acknowledged baseline, and sends it on the unreliable state channel. Application-level acknowledgement uses the core `UnreliableSequenced` sequence numbers.
- **Interest management and dormancy.** Entities are filtered by area of interest (AoI) using a spatial structure; dormant (unchanging) entities are omitted from steady-state ticks; per-connection priority and a bandwidth budget decide what fits (uses the core MTU limit for packing).
- **Discrete events over a reliable channel.** Spawn/despawn and gameplay events use a reliable-ordered channel, separate from the unreliable state channel to avoid head-of-line blocking.
- **Fixed network tick with render interpolation.** Replication advances on a fixed tick independent of frame rate, and remote entities are rendered by interpolating between snapshots, reusing the physics accumulator pattern.
- **Non-goals**: client-side prediction/rollback (`add-network-prediction`), deterministic lockstep (separate mode/change), client authority, dedicated-server target, editor tooling for marking replicated fields.

## Capabilities

### New Capabilities

- `network-replication`: the authoritative replication model — per-connection baseline/delta, snapshot build and apply, application-level acknowledgement, and reliable event delivery.
- `network-replication-source`: the data-driven source seam — batched dense iteration, stable cross-module type identity, field-column (SoA) access, and deterministic ordering, adaptable by both the component model and the ECS.
- `network-component-replication`: replicated component/property authoring on top of reflection and the existing binary serialization seam, including replica creation/destruction on clients.
- `network-interest-management`: area-of-interest filtering, dormancy, priority ordering, and per-connection bandwidth budgeting.
- `network-tick`: the fixed network tick, snapshot cadence, and render interpolation of remote entities.

### Modified Capabilities

<!-- None: this adds a new bridge layer; no existing spec requirements change. -->

## Impact

- **Engine**: three new sub-targets under `engine/network/` — `NetworkReplication` (algorithm + source seam; links `Network` only, no `Framework`), `NetworkWorld` (links `NetworkReplication` + `Framework`), and `NetworkEcs` (links `NetworkReplication` + `Core`). `engine/network` core remains unchanged.
- **World/ECS**: a replication-source seam with two adapters — `Actor`/`ComponentBase` and the data-oriented ECS (`core/ecs` `SparseSet`/`View`); clients instantiate replicas from replication events.
- **Serialization**: network type identity uses a stable cross-module id (ECS tag-hash `TypeId` / component `Uuid`); reflection (`SerializationContext`) is used only for field enumeration; replicated-field metadata and per-field delta encoding are added.
- **Consumers**: game code marks replicated fields and handles authoritative events; no consumer depends on a concrete transport backend.
- **Compatibility**: additive; no existing runtime data is affected. Depends on `add-network-core` (not yet implemented).
