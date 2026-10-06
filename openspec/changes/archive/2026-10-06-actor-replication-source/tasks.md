## 1. Deterministic batch iteration

- [x] 1.1 Source builds per-type buckets from `World::GetActors()` (no framework index).
- [x] 1.2 Deterministic order by stable actor id (sorted per bucket).
- [x] 1.3 Test `ActorReplicationSourceTest.IterationOrderIsDeterministic`.

## 2. Apply point and host-owned lifecycle

- [x] 2.1 Documented: host resolves spawn/despawn/state after `World::Tick`, same thread.
- [x] 2.2 Host owns actor create/destroy and transmits full identity; source manages replica components only.
- [x] 2.3 No framework command sink added.

## 3. Side-effect-free apply

- [x] 3.1 `ReplicationApplyScope` + `IsApplyingReplication()`; applied around `Apply`/`ApplyField`.
- [x] 3.2 Test asserts the guard is active during a replicated setter.
  (Wiring the guard into specific component mutators is per-component work, done when a component opts into replication.)

## 4. ActorReplicationSource adapter

- [x] 4.1 `ActorReplicationSource : sky::net::IReplicationSource` in `engine/network/world` (target `NetworkWorld`, links `Framework`).
- [x] 4.2 `ForEachRecord` over per-type buckets; `Encode`/`Apply` + `EncodeField`/`ApplyField` via reflection + `BinaryArchive` using `REPLICATED`.
- [x] 4.3 Identity: `ReplicationTypeId` from the component type; `ReplicatedEntityId` = deterministic 64-bit hash of the full actor uuid (`EntityIdFor`, public for the host).
- [x] 4.4 Authority/spawn-despawn owned by the network layer (host spawns actors before replicas).
- [x] 4.5 O(1) `entity -> Actor*` index seeded in the constructor and maintained via `WorldEvent::OnActorAttached/Detached`.
- [x] 4.6 Tests: `EncodeApplyReplicatedFields`, `FieldEncodeApply`, `SnapshotLoopback`, `IterationOrderIsDeterministic`, `IndexTracksAttachDetach`.

## 5. Verification

- [x] 5.1 Built `NetworkWorld` + `NetworkWorldTest`; confirmed the algorithm target (`NetworkReplication`) does not link `Framework`.
- [x] 5.2 `NetworkWorldTest` 4/4 incl. a `SnapshotCodec` loopback (`SnapshotLoopback`); framework/plugins unaffected.
- [x] 5.3 clang-format/clang-tidy.
