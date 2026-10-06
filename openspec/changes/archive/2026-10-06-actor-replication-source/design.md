## Context

`engine/network/replication` is data-driven and `Framework`-free: `IReplicationSource::ForEachRecord` iterates densely with deterministic order (`IReplicationSource.h`), and `EcsReplicationSource` wraps pools. `ReplicationTypeId = uint32_t`, `ReplicatedEntityId = uint64_t`, `FieldMask = uint64_t`. Specs: server authoritative, clients read-only; component fields declared replicated reuse reflection + binary serialization.

The framework side stores components per actor in a `std::map` (ordered by type id) and has an O(1) actor index, but no dense per-type component index and no replication flags.

## Goals / Non-Goals

**Goals:**

- A framework `IReplicationSource` usable by the same algorithm as the ECS adapter.
- Field selection from reflection metadata.
- A defined, safe apply point and side-effect-free application.

**Non-Goals:**

- Changing the replication algorithm, baselines, ack/repair, channels.
- Adding a framework structural-change API (command sink) — not needed given the apply point.
- Adding authority to `Actor`/`ComponentBase`.
- Changing `engine/core/ecs`.

## Decisions

### D1: Source-side per-type grouping

The adapter builds per-type buckets by iterating the world's actor list once per snapshot and grouping replicated components, then sorting each bucket by stable actor id. Rationale: the seam needs batch-per-type deterministic iteration, but that view has a single consumer (replication), so it stays in the adapter rather than adding an index to `World` (which keeps to scene/script duties).

### D2: `ActorReplicationSource : IReplicationSource`

Implement the seam over `World`/`Actor`/`ComponentBase`, mirroring `EcsReplicationSource`. It reads `REPLICATED` member metadata and encodes/applies via the member `getter`/`setter` + type info + `BinaryArchive`.

### D3: Apply point replaces the command sink; host owns actor lifecycle

The network host runs on the same thread and resolves incoming spawn/despawn/state **after `World::Tick` returns** (i.e., outside component/actor iteration), then starts the next tick. No `IWorldCommandSink` or command PODs are added; the framework stays unchanged. Rationale: structural mutation is only unsafe **during** iteration; a defined post-tick, same-thread apply point makes create/destroy safe without new framework concepts. This is a hard requirement, not an assumption — the replication host MUST NOT resolve structural changes from inside a `World::Tick`.

Actor creation/destruction is host-owned: the host spawns/despawns actors (transmitting the actor's full identity out of band) and the source only adds/removes replica components on existing actors. Rationale: the seam's `CreateReplica(entity,type)` only carries a `uint64` entity id, which is not enough to reconstruct a full `Uuid` actor identity.

### D4: Side-effect-free apply path

Applying replicated values through ordinary component setters can trigger gameplay side effects — evidence: `PhysicsBodyComponent::SetMass -> Recreate()` and `SetShape -> ShapeChanged() -> Recreate()` (`plugins/bullet/src/components/PhysicsBodyComponent.cpp:158-179`). The adapter therefore needs an apply path that suppresses side effects, e.g. a scoped `ReplicationApplyScope` flag the component can check in its mutators, or a dedicated apply setter. Rationale: client replicas should receive state without re-running construction/network send logic.

### D5: Identity and O(1) actor resolution

`ReplicationTypeId` comes from the stable component type id (from `reflection-replication-fields`). `ReplicatedEntityId` is a deterministic 64-bit hash of the full 128-bit actor uuid, so both peers derive the same id without transmitting any mapping. The source keeps an O(1) `entity -> Actor*` index, seeded in its constructor and kept in sync by subscribing to `WorldEvent::OnActorAttached/Detached`; resolution never scans the actor list. No runtime reflection as identity.

Alternative considered: a side table assigning arbitrary ids. Rejected — it would force the host to transmit an id↔uuid mapping (protocol/host coupling) to solve a collision probability that the 64-bit hash already makes negligible.

### D6: Authority and spawn/despawn live in the network layer

Authority (server vs client replica) and replica lifetime are owned by `engine/network`, keyed by stable actor id; the framework adapter queries it. No authority members are added to the scene model.

```
network (algorithm, same thread)
  ReplicationHost ── IReplicationSource (bytes + FieldMask)
                        ▲
                        │ implements
              ActorReplicationSource (links framework)
                 ├ iterate World per-type index (dense)
                 ├ encode/apply REPLICATED fields (reflection + BinaryArchive), side-effect-free
                 └ headless of authority/lifetime (owned by network)
   apply point: after World::Tick (outside iteration, same thread)
```

## Risks / Trade-offs

- [Apply point discipline] → The replication host must not resolve structural changes inside `World::Tick`; document and assert where feasible.
- [Side-effect suppression] → Suppression must be explicit in component mutators; audit replicates-heavy components (physics).
- [Identity] → 64-bit deterministic hash of the full uuid; collision probability is negligible at realistic scene sizes.
- [Adapter lifetime] → The source holds `World&` and a world-event subscription; it MUST NOT outlive its world (destructor disconnects).

## Migration Plan

- Additive; the algorithm and ECS path are untouched.
- Depends on `reflection-replication-fields` and `harden-framework-components`.
- Rollback: remove the adapter + index; no serialized format change.

## Open Questions

- Index scope: only types with replicated fields, or all? (Leaning replicated-only.)
- Side-effect suppression mechanism: scoped flag vs apply-specific setters.
