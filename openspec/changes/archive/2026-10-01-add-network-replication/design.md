## Context

`add-network-core` provides connections, channels with delivery modes, sessions, lanes, and stats, but no game-state synchronization. The engine has two simulation models a replication layer could read from: the `Actor`/`ComponentBase` model (`engine/framework/include/framework/world/`, per-component `SaveBin`/`LoadBin`, `ComponentAdaptor<Data>`) and the data-oriented ECS (`engine/core/include/core/ecs/`: `EntityRegistry`, `SparseSet`, `View<Ts...>`, cross-module-stable compile-time `TypeId`). A reflection system (`SerializationContext`, `TypeNode`) exists for field enumeration and editing. A fixed-step accumulator + interpolation pattern exists in `engine/physics`, and `engine/navigation/terrain` (`TerrainNavigationBridge`, target `NavigationTerrain`) is the precedent for a World-coupled bridge living in a sub-target separate from the core.

## Goals / Non-Goals

**Goals:**

- Server-authoritative state replication over the frozen `engine/network` core, with no core changes.
- A data-driven source seam so both the `Actor`/`ComponentBase` model and the `core/ecs` ECS can be replicated without binding the layer to either.
- Authoring that fits the engine's component model, and bandwidth that scales to large worlds.
- A model that later admits prediction/rollback and lockstep without reworking the transport.

**Non-Goals:**

- Client-side prediction, reconciliation, rollback (`add-network-prediction`).
- Deterministic lockstep / input replication (separate mode and change).
- Client authority or peer-to-peer replication.
- Editor tooling for marking replicated fields (later change).
- Dedicated-server build target.

**Scope note:** the Framework `Actor`/`ComponentBase` adapter (`NetworkWorld`) and its world subsystem are **deferred** because the World/ECS model may be refactored. The data-driven ECS source (`NetworkEcs`) is the reference path for this change, and its validation (loopback end-to-end, scale, field delta, interpolation, benchmark) is the active scope. Tasks 1.2/1.3/1.4/2.3/2.7/3.1/3.4/8.1 are deferred with it.

## Decisions

### D1. Component/property authoring over a snapshot/delta transport

Replicated components declare replicated fields; the replication layer serializes them into per-connection snapshot records and deltas against acked baselines.

**Why:** this is the industry norm — Unreal's property replication, Unity NGO's `NetworkVariable`, and Godot's `MultiplayerSynchronizer` all use component-oriented authoring, while high-scale transports are snapshot-based (Quake, Unity Netcode for Entities). Unreal's Iris is essentially "compile property authoring into snapshot transport". **Alternatives:** pure snapshot authoring (uniform but poor editor ergonomics, no component model fit) and pure property sending (fine-grained but poor global bandwidth control and scale).

### D2. Server is authoritative; clients hold read-only replicas

The server owns each replicated entity; clients receive spawn/despawn events and state updates and do not send authoritative changes. Client inputs, if any, are separate messages that the server validates.

**Why:** mandatory for competitive integrity and the simplest correct model. **Alternative:** client-authoritative relay — rejected (cheat-prone; inappropriate as an engine default).

### D3. Build on the existing reflection and binary serialization seam

Replication reuses `SerializationContext` / `TypeNode` and `ComponentBase::SaveBin`/`LoadBin` rather than a second serialization system. Replicated-field metadata is additive on top of reflection.

**Why:** avoids duplicating the type system and keeps replicated and persisted data consistent. **Trade-off:** `SaveBin` is full-state; delta requires either a replicated-field bitmask (chosen) or blob diffing (fallback), so components opt fields into replication.

### D4. Per-connection baseline, full-flag + gap detection, and application-level ack

Each connection tracks a baseline. Each snapshot carries an application sequence and a **full/repair flag**. The server delta-compresses against the per-connection baseline; the client applies and acknowledges a snapshot only when it is **contiguous** with its last applied sequence, or when it is marked **full** (a repair, always accepted). A gapped delta is ignored and **not acknowledged**, so the server's repair path (a full snapshot) is triggered.

**Why:** state snapshots must be unreliable (latest wins) yet ackable, which transport-level reliability cannot express. Gap detection plus a full flag prevents a single lost delta from permanently desynchronizing the client, which optimistic baseline advancement alone would allow; this was confirmed by a default-repair-threshold regression test. **Alternative:** acknowledge using the core transport sequence — rejected, it would require the server to map transport sequence numbers back to snapshots. **Alternative:** reliable state channel — rejected (head-of-line blocking on loss).

### D5. State and events use separate channels

Continuous state uses an unreliable (sequenced) channel; discrete spawn/despawn and gameplay events use a reliable-ordered channel.

**Why:** a lost snapshot must not block a reliable event and vice versa. **Trade-off:** events that depend on a specific snapshot may need to be deferred until the referenced baseline arrives.

### D6. Interest management, dormancy, and bandwidth budget

A spatial structure (grid/octree; terrain and navigation already maintain spatial data) feeds AoI filtering. Entities that have not changed are dormant and omitted. A per-connection priority (proximity, recency, gameplay relevance) and bandwidth budget decide what is sent when the AoI set exceeds the budget.

**Why:** a large open world cannot send every entity every tick; without these three the design does not scale. **Trade-off:** AoI boundaries can pop; mitigation is hysteresis on the boundary and priority for entering entities.

### D7. Fixed network tick with render interpolation

Replication advances on a fixed network tick independent of frame rate; remote entities render interpolated between the two most recent snapshots, reusing the physics accumulator pattern. Network time remains the real monotonic clock from the core.

**Why:** deterministic cadence and smooth remote motion; decouples bandwidth and CPU from render frame rate. **Trade-off:** interpolation adds up to one tick of display latency.

### D8. Replication is split into an algorithm target and adapter targets

`NetworkReplication` (`engine/network/replication/`) holds the replication algorithm and the source seam and links `Network` only — it SHALL NOT depend on `Framework` or on a concrete simulation model. The simulation models are separate adapters: `NetworkWorld` (`engine/network/world/`, links `NetworkReplication` + `Framework`, registers the world subsystem) and `NetworkEcs` (`engine/network/ecs/`, links `NetworkReplication` + `Core`). The core stays Core-only.

**Why:** the World/ECS model is expected to change and the Actor/Component vs ECS choice is the highest-risk seam; keeping the algorithm Framework-free lets adapters be swapped or added without touching the algorithm, lets a pure-ECS path avoid `Framework` entirely, and lets the algorithm be tested against a mock source. **Alternative:** one monolithic bridge target linking `Framework` — rejected, it re-couples algorithm, reflection, and the world model; **Alternative:** let the algorithm link `Framework` for convenience — rejected, it forfeits the Framework-free ECS path.

### D9. Replication reads through a data-driven source seam

Replication SHALL read simulation state through an engine-side `IReplicationSource` seam defined in `NetworkReplication`, rather than walking a concrete world model. The seam exposes batched, dense iteration over replicated entities/components keyed by a stable type id with field-column (SoA) access. `NetworkWorld` and `NetworkEcs` implement it; adapters convert `Framework` reflection metadata into Framework-free plain-data descriptors (type id, field count, field encoders) handed to the algorithm, so the algorithm never calls reflection directly.

- **Stable identity:** replicated types are identified by a cross-module stable id (the ECS compile-time tag-hash `TypeId`, or a component `Uuid`); runtime reflection is used only by adapters for field enumeration and editor presentation, never as the network identity.
- **Batch encode:** snapshots are encoded per type/batch to avoid per-entity virtual dispatch on the hot path.
- **SoA network state:** replicated state is stored in per-field columns so delta, quantization, and bandwidth budgeting are batch-friendly.
- **Deterministic order:** iteration order is stabilized (ordered by stable entity id) regardless of container layout, so `SparseSet` swap-remove reordering cannot perturb the simulation or delta baselines.

**Why:** the engine is moving toward data-oriented ECS (`core-ecs`, `core-ecs-view`) while gameplay still uses `Actor`/`ComponentBase`; a source seam lets both plug in without betting on one and keeps the hot path batch-oriented. `core/ecs`'s compile-time `TypeId` is already cross-module and call-order independent, which is exactly what network type identity needs. **Alternative:** bind directly to `framework/world` — rejected, it would force a rewrite when the ECS becomes the simulation base and would forfeit dense iteration and stable type ids.

### D10. Revision-based skip and a shared encode cache

A record MAY expose a monotonic `Revision()`. When a record's revision matches a connection's baseline, it is skipped **without encoding** (dormancy that saves CPU, not just bandwidth). A server-wide `EncodedRecordCache` keyed by `(entity, type)` and validated by revision encodes each record **once per tick** and reuses the bytes across connections.

**Why:** encoding is independent of the connection, so per-connection encoding (`O(connections x records)`) is redundant; the cache reduces it to `O(records)`. **Trade-off (measured):** end-to-end multi-connection speedup is only ~1.17x because the dominant per-connection cost is the gather/sort/delta/payload build rather than `Encode`; a future per-tick shared *gather* (the cache is a prerequisite) is the next step. Cache entries are evicted on despawn; records with `Revision() == 0` are encoded every time and remain correct.

### D11. Hardened resume tokens and honest sequence capability

The core `ResumeToken` is signed with **HMAC-SHA256** (built-in implementation, optional OpenSSL when `SKY_BUILD_OPENSSL` is available) and verified with a constant-time comparison. Backends that cannot carry a sender sequence report `realSendSequence = false`, and consumers use their own application sequence for gap detection (the replication layer already does).

**Why:** the earlier placeholder keyed hash was forgeable, and a synthesized receive sequence cannot detect loss. These are core-level changes; the main specs `network-session` and `network-transport` were updated accordingly.

## Risks / Trade-offs

- **Bandwidth blowup in dense scenes** -> priority + budget + dormancy + quantization; expose per-connection stats (from the core) for tuning.
- **Desync between server and client replicas** -> authoritative server plus periodic full-snapshot repair when a baseline is unacknowledged for too long.
- **`SaveBin` full-state vs delta** -> replicated-field bitmask; fall back to blob diff only where necessary.
- **AoI pop-in on boundary crossing** -> hysteresis and prioritized spawn of entering entities.
- **Event referencing an unarrived baseline** -> defer or bundle the event with a minimal state record.
- **Reflection cost per tick** -> cache per-type replication metadata at registration time; only dirty fields are visited.
- **Per-entity virtual dispatch on the hot path** -> batch encode per type through the source seam; avoid `SaveBin`-style per-entity virtual calls in steady state.
- **`SparseSet` swap-remove perturbs iteration order** -> stabilize order by stable entity id; never rely on dense array layout for baseline or delta correctness.
- **Fixed tick vs render frame drift** -> accumulator with clamped catch-up, same policy as physics.

## Follow-up Changes

- `add-network-prediction`: client prediction, server reconciliation, optional rollback.
- Lockstep / deterministic input replication as a distinct replication mode.
- Editor tooling to mark replicated fields and inspect replication.
- Dedicated-server target.

## Open Questions

- Which source adapters ship first (`framework/world`, `core/ecs`, or both) and whether the ECS becomes the simulation base.
- Replication cadence defaults (tick rate, AoI cell size, dormancy timeout).
- Whether replication metadata is authored via macros, attributes, or editor-marked fields.
- Delta encoding form for common types (position/rotation quantization bit widths).
- Whether spawn/despawn rides the reliable channel alone or is implied by the first snapshot that contains the entity.
- How transient entities (projectiles, VFX) opt out of baseline tracking.
