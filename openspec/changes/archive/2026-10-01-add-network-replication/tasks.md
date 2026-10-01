## 1. Target scaffold

- [x] 1.1 Create `engine/network/replication` (target `NetworkReplication`, links `Network` only) and register it in `engine/network/CMakeLists.txt`
- [ ] 1.2 (deferred: Framework adapter) Create `engine/network/world` (target `NetworkWorld`, links `NetworkReplication` + `Framework`) — `NetworkEcs` already exists
- [ ] 1.3 (deferred: Framework adapter) Add a world subsystem registered under a canonical network system name
- [ ] 1.4 (deferred: Framework adapter) Add the `NetworkWorldTest` target
- [x] 1.5 Verify `NetworkReplication` links no `Framework`, `engine/network` core is unmodified, and no core target references a sub-target

## 2. Replication source seam

- [x] 2.1 Define the source seam interface in `NetworkReplication`: batched dense iteration, stable type id, and per-field (SoA) column access
- [x] 2.2 Define stable network type identity (ECS compile-time tag-hash `TypeId` / component `Uuid`) and plain-data replication descriptors so the algorithm never calls reflection
- [ ] 2.3 (deferred: Framework adapter) Implement the `NetworkWorld` adapter (Framework reflection; `Actor`/`ComponentBase`)
- [x] 2.4 Implement the `NetworkEcs` adapter (`core/ecs` `EntityRegistry`/`View`; no Framework)
- [x] 2.5 Implement deterministic iteration order (stable by entity id) independent of container layout
- [x] 2.6 Add a mock-source test exercising the algorithm with no Framework dependency
- [ ] 2.7 (deferred: Framework adapter) Add tests: identical snapshot content and order across both adapters, and stability under `SparseSet` swap-remove churn

## 3. Replicated component authoring

- [ ] 3.1 (deferred: Framework adapter) Add replicated-field metadata on top of reflection (`SerializationContext` / `TypeNode`) without a second serialization path
- [x] 3.2 Provide a component macro/descriptor to mark replicated fields and their bit indices
- [x] 3.3 Cache per-type replication metadata at component registration time (avoid per-tick reflection)
- [ ] 3.4 (deferred: Framework adapter) Implement full-state component encoding using the existing `SaveBin`/`LoadBin` seam
- [x] 3.5 Implement delta encoding as a changed-field bitmask plus changed values, per field column
- [x] 3.6 Add a test asserting unmarked fields never appear in encoded replication data

## 4. Snapshot, baseline, delta, acknowledgement

- [x] 4.1 Implement per-connection baseline tracking (last acknowledged snapshot)
- [x] 4.2 Build a per-connection snapshot from relevant components each network tick
- [x] 4.3 Encode deltas relative to the connection baseline and send on the unreliable state channel
- [x] 4.4 Apply received deltas to client replicas
- [x] 4.5 Acknowledge snapshots using the core sequenced sequence number and advance the baseline
- [x] 4.6 Implement full-snapshot repair when a baseline is unacknowledged beyond a configured window
- [x] 4.7 Add tests for delta omission, baseline advance, and repair after sustained loss

## 5. Events and life cycle

- [x] 5.1 Send spawn/despawn and discrete gameplay events on the reliable-ordered channel
- [x] 5.2 Create client replicas on spawn and remove them on despawn
- [x] 5.3 Handle events that reference an unarrived baseline (defer or bundle minimal state)
- [x] 5.4 Add tests for spawn/despawn ordering and event delivery under snapshot loss

## 6. Interest management and budget

- [x] 6.1 Implement AoI filtering against a spatial structure (reuse terrain/navigation spatial data where possible)
- [x] 6.2 Implement dormancy so unchanged entities are omitted from steady-state updates
- [x] 6.3 Implement per-connection priority ordering and a bandwidth budget
- [x] 6.4 Pack messages within the transport payload limit and split when needed
- [x] 6.5 Add tests for AoI exclusion, dormancy, budget deferral, and message splitting

## 7. Network tick and interpolation

- [x] 7.1 Implement the fixed network tick with a bounded accumulator on the real monotonic clock
- [x] 7.2 Implement client interpolation between the two most recent snapshots
- [x] 7.3 Confirm the network tick continues while the world simulation is paused
- [x] 7.4 Add tests for tick cadence independence from frame rate and bounded catch-up

## 8. Validation

- [ ] 8.1 (deferred: Framework adapter) Build the engine and run `NetworkWorldTest`
- [x] 8.2 End-to-end check: server mutates a replicated component, client replica updates and interpolates
- [x] 8.3 Confirm `engine/network` core remains Core-only and unchanged
- [x] 8.4 Document deferred items: prediction/rollback, lockstep, editor replication tooling (see design.md scope note; Framework adapter deferred)

## 10. Hardening and performance (added during implementation)

- [x] Revision-based encode skip for dormancy (`IReplicationRecord::Revision`)
- [x] Per-server shared encode cache across connections, evicted on despawn (`EncodedRecordCache`)
- [x] Expose `realSendSequence` capability and document synthesized-sequence limits
- [x] HMAC-SHA256 resume tokens with constant-time verification (built-in; optional OpenSSL path)
- [x] Core hardening: OwnedThread outbound serialization, ack full-flag + client gap detection
- [x] Benchmark suite and baseline record (`engine/network/replication/bench/BASELINE.md`)
- [x] Code cleanup: unified byte codec (`network/detail/ByteCodec.h`), shared test harness, removed redundant/unused APIs
