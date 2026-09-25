## Context

`engine/*` currently has no networking code, and `core/async` provides only computation primitives (`ThreadPool`, `Task`, `Semaphore`, `LockFreeQueue`) with no socket or event-loop layer. The repository enforces a strict layout: `engine/<feature>` holds interfaces + data + backend-agnostic logic (see `engine/physics`, `engine/navigation`), and implementations live in `plugins/<feature>` (see `plugins/bullet`, `plugins/recast`). `engine/navigation/terrain` (`TerrainNavigationBridge`) is the established pattern for a world-coupled bridge placed in a sub-target, separate from the core.

Constraints that shape this design:

- **Cross-platform**: desktop, Android/iOS clients, Win/Linux dedicated servers, and wasm reserved. wasm cannot assume threads or raw sockets.
- **Boost is isolated** (`openspec/specs/boost-isolation/spec.md`): Boost is header-only and restricted to legacy shader/render. Boost.Asio is therefore not a candidate; standalone Asio only.
- **Dedicated servers must not require Aurora/render**, so the core host must be drivable from a plain `IModule` loop with no window or world.
- The consumer (World/ECS replication) is explicitly out of scope and expected to change, so the core seam must be frozen while the bridge stays mutable.

## Goals / Non-Goals

**Goals:**

- A backend-swappable transport + session contract in `engine/network` that depends on `Core` only.
- A single, backend-implementable threading primitive that serves both a deterministic single-thread mode and a low-latency threaded mode.
- Session identity that survives reconnects and can be re-homed for graceful scaling, without requiring cluster infrastructure.
- Enough observability and lifecycle control for an external orchestrator to scale a server process up and down safely.

**Non-Goals:**

- World/ECS replication, snapshot/delta, interest management (deferred to `engine/network/world`).
- A concrete `Cluster` or `Control` backend (enum + composition only).
- Matchmaking, service discovery, membership/consensus, container orchestration.
- Game-message serialization (payload stays opaque bytes; `BinaryArchive` binding is a later layer).
- Runtime, in-process elastic thread scaling with live-connection migration.

## Decisions

### D1. Core module is Core-only; the rest is a family of sub-targets

`engine/network` builds target `Network` and links `Core` only; it never includes `framework/world`. Higher layers are separate sub-targets under `engine/network/` (following `engine/navigation/{builder,terrain}`), so a concern can change without rebuilding or re-coupling the others:

| target | directory | links | concern |
|---|---|---|---|
| `Network` | `engine/network/` | `Core` | transport, session, lifecycle (this change) |
| `NetworkReplication` | `engine/network/replication/` | `Network` | replication algorithm + source seam (no `Framework`) |
| `NetworkWorld` | `engine/network/world/` | `NetworkReplication` + `Framework` | `Actor`/`Component` source adapter + world subsystem |
| `NetworkEcs` | `engine/network/ecs/` | `NetworkReplication` + `Core` | `SparseSet`/`View` source adapter |
| `NetworkPrediction` | `engine/network/prediction/` | `NetworkReplication` | prediction + reconciliation |
| `NetworkLockstep` | `engine/network/lockstep/` | `NetworkReplication` + `Physics` | determinism + rollback |

Include paths stay `network/...` across all sub-targets.

**Why:** the simulation model (World/ECS) is expected to change; keeping the replication algorithm free of `Framework` and of any concrete model means swapping the source adapter or adding a genre layer never touches the frozen core nor each other, and a pure-ECS path can avoid `Framework` entirely. **Alternative:** one monolithic bridge target — rejected, it re-couples transport, replication, reflection, and the world model; **Alternative:** put `IWorldSubSystem` in the core (as `engine/physics` does via `IPhysicsSystem`) — rejected because networking must also run in a render-free dedicated server.

### D2. Seam sits at L4 (connection + channel), not at L2 (socket)

The backend contract exposes connections and channels with delivery modes; it does not expose sockets or datagrams.

```cpp
enum class DeliveryMode { ReliableOrdered, ReliableUnordered, UnreliableSequenced, Unreliable };

class INetConnection {
    virtual void Send(uint8_t channel, std::span<const uint8_t> payload, DeliveryMode mode) = 0;
    virtual void Close(DisconnectReason) = 0;
};

class INetBackend {
    virtual const NetworkBackendCaps &GetCaps() const = 0;
    virtual INetListener  *Listen(const ListenDesc &) = 0;
    virtual INetConnection *Connect(const ConnectDesc &) = 0;
};
```

**Why:** ENet/KCP/GameNetworkingSockets provide ACK/retransmit/fragmentation; sinking the seam to UDP would force reimplementing all of it per platform. Expressing the seam in terms of "channel + delivery mode" lets a future raw `asio` backend implement the same semantics internally and a WebSocket backend report only `ReliableOrdered`. **Alternative:** two-tier datagram + reliability seam — deferred as an internal detail of a future raw backend, since it is far more P1 cost.

### D3. Role-composed host, not a single active backend

`NetworkHost` composes backends per `NetworkRole { Client, Server, Cluster, Control }`. This intentionally diverges from `PhysicsBackendRegistry`, which allows exactly one active backend.

**Why:** a dedicated server concurrently runs client traffic (UDP, untrusted), cluster traffic (reliable mTLS), and control/metrics (HTTP). **Alternative:** one active backend — rejected, it cannot serve split roles; one backend supporting all roles is allowed but must not be required.

**Industry precedent:** Unreal hosts `IpNetDriver`, `WebNetDriver`, and `DemoNetDriver` side by side, and Unity's transport exposes several `NetworkInterface` implementations; role/transport composition rather than a single active transport mirrors how shipping engines are built.

### D4. Unified `Pump` primitive; host owns the threading policy

```cpp
// waitMs == 0 -> non-blocking (CallerPump). waitMs > 0 -> may block up to waitMs (OwnedThread).
virtual uint32_t Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t waitMs) = 0;
```

- **CallerPump** (reference path): `Update()` calls `Pump(..., 0)` on the caller thread.
- **OwnedThread**: host runs a single IO thread looping `Pump(..., waitMs)`; events cross via a bounded `LockFreeQueue`; `Update()` drains on the caller thread.

Every backend is written against `Pump` once. **Why:** ENet's `enet_host_service`, Asio's `poll`/`run_for`, and GNS's `RunCallbacks` all fit this shape, so backends never own threads and the policy is a host concern. **Alternative:** per-backend threading (each owns io_context/thread) — rejected, it fragments the contract and makes deterministic testing impossible.

### D5. Backend is single-threaded; host serializes access

In `OwnedThread`, outgoing sends are queued and executed on the IO thread, and incoming events are queued for the caller thread. The backend is only ever touched by one thread at a time. Backends with unavoidable internal threads (GNS, uWS) advertise `backendOwnsThreads` and rely on host-visible ordering guarantees.

**Why:** keeps the backend contract simple and race-free. **Trade-off:** outbound latency in `OwnedThread` is bounded by `waitMs` for backends without a wakeup primitive (`wakeupSupport=false`, e.g. ENet), so `waitMs` defaults to 1–2 ms.

### D6. Callbacks always run on the caller thread

Regardless of mode, user callbacks are invoked only during `Update()`'s drain, in queue order, at a fixed point in the frame. **Why:** game logic is thread-agnostic and lockstep determinism is preserved even under `OwnedThread`. The IO thread only enqueues bytes; it never touches simulation state.

### D7. Network time is real monotonic time, independent of the game clock

Heartbeat, timeout, reconnect backoff, and stats windows use `std::chrono::steady_clock`, not the `World` tick delta, and the host is driven from `IModule` rather than a world subsystem. **Why:** an editor play/pause must not drop connections, and lockstep must not let render frame time perturb timeouts.

### D8. Lanes are connection partitions fixed at startup

`NetworkHostConfig.laneCount` is fixed for the process lifetime. A lane is an independent object that partitions connections (each connection is pinned to one lane by hash for its lifetime) and owns that lane's connection set and counters. Per-connection ordering is guaranteed because events are delivered in arrival order from a single drain queue.

**Implementation note (deviation):** the backend contract advances *all* connections in one `Pump`, so a lane cannot currently own its own queue or I/O thread. Tasks 4.1/4.4 are adjusted accordingly; per-lane I/O would require backend-side lane partitioning and is deferred. The lane object still keeps its own connection set and counters, so per-lane load is reported and add/remove of lanes for new connections can be introduced later without live-connection migration.

**Why:** live socket migration between io_contexts is complex and backend-dependent, and runtime elasticity is better served at the process layer. **Alternatives:** (A) fixed-only — chosen for P1; (B) fully dynamic with live migration — rejected; (C) add/remove without migration — reserved as the growth path.

### D9. Session identity is separate from the physical connection; `ResumeToken` is stateless and signed

`SessionId` is stable across reconnects; `ConnectionId { index, generation }` is the physical handle (mirroring `PhysicsObjectId`). `ResumeToken` is an expiring HMAC-signed value any server can verify with a shared secret.

**Why:** decoupling is required for reconnect, re-homing during drain, and cross-server handoff; stateless signing avoids a session directory and therefore any `Cluster` dependency in P1. **Alternative:** directory-backed tokens — reserved as an enhancement once `Cluster` exists (enables active revocation).

### D10. Lifecycle is an explicit state machine with graceful shutdown ordering

`Accepting -> Draining -> Closed`, with `RedirectTo(address, token)` to move clients during drain. Shutdown order: stop producers (signal + join IO thread, or stop pump) -> discard/drain pending events -> release sockets.

**Why:** dynamic scale-down must not drop players; the ordering prevents use-after-free when the backend thread outlives the host.

### D11. Event payloads are copied into the queue; overflow closes the connection

Because queued events outlive the backend's receive buffer, payloads are copied into an owned buffer in P1. The event queue is a bounded `LockFreeQueue`; on overflow the affected connection is closed with a backpressure error rather than silently dropping events.

**Why:** silent loss of control/reliable events is worse than a controlled disconnect. **Trade-off:** one copy per inbound message; a buffer pool or a `CallerPump` zero-copy fast path is a later optimization.

### D12. First backend is ENet plus a loopback test backend

ENet validates `Client`/`Server` roles against a real UDP transport. A deterministic in-process loopback backend backs the test suite.

**Why:** ENet covers desktop + mobile, gives reliable/unreliable channels, is C/MIT, and has no Boost dependency. **Alternative:** GameNetworkingSockets first — deferred to the P2 `Cluster`/P2P work where its extra capabilities matter.

**Industry precedent:** Godot ships `ENetMultiplayerPeer` as its default and Mirror defaults to KCP, showing that "use a library for L3" is a mature path for engines that do not need in-house tuning. Unreal and Unity instead build reliability in-house because they target very high tick rates and deep prediction/rollback, which is out of scope for an engine-plus-games middleware approach.

### D13. Deferred backend roadmap (recorded, not implemented here)

The reserved roles have concrete target technologies so the seams are not left ambiguous:

- `Cluster` / `Control`: standalone Asio (Boost.Asio is excluded by `openspec/specs/boost-isolation/spec.md`) plus OpenSSL 3.0.18, which is already a third-party dependency. Reliable-ordered TCP/TLS; Asio's `poll` / `run_for` map onto `Pump`, `strand` onto a lane, and `post()` provides `wakeupSupport`.
- wasm: a WebSocket backend under `CallerPump`; Asio does not support wasm and UDP reliability has no wasm path.
- P2P: GameNetworkingSockets or WebRTC; NAT traversal must come from a dedicated solution, not be reimplemented.

**Why:** Asio is a socket/event-loop substrate (L1/L2), not a game transport (L3/L4), so it cannot satisfy the `Client`/`Server` reliable-UDP requirement alone. It is nonetheless the natural base for the reliable-order stream roles.

## Risks / Trade-offs

- **Capability flags do not prevent semantic leakage across backends** -> add a backend conformance test suite (connect/send/delivery-mode/close/overflow) that every backend must pass, and keep the seam expressed in delivery modes rather than backend concepts.
- **Bounded queue overflow loses in-flight messages on close** -> surfaced as an explicit backpressure error; capacity is configurable; reliable events already delivered are not affected.
- **`OwnedThread` outbound latency for non-wakeup backends** -> default `waitMs` 1–2 ms; expose `wakeupSupport`.
- **`Pump` that blocks past `waitMs` stalls shutdown join** -> contract forbids blocking TLS handshakes inside `Pump`; backends must return within `waitMs`.
- **Per-message copy cost at high rates** -> acceptable at game message rates in P1; buffer pool / zero-copy reserved.
- **ENet is not thread-safe and cannot be woken** -> it runs only under `CallerPump` or host-owned IO thread with host-serialized access; no backend-internal threads.
- **wasm reserved** -> `CallerPump` is the reference path and must not depend on threads or raw file descriptors; the eventual wasm backend is WebSocket.
- **Session token revocation latency** (stateless tokens) -> use short TTL; directory-backed revocation is the documented future path.

## Migration Plan

Additive and greenfield: no existing engine code changes, no runtime data migration. Land in order: (1) `engine/network` core contract + loopback backend + tests; (2) ENet plugin backend; (3) session/resume and drain/redirect wiring. Rollback is removing the new target and plugin entries.

## Follow-up Changes (deferred)

- `add-network-replication`: adds `NetworkReplication` (algorithm + source seam), `NetworkWorld` (Actor/Component adapter), and `NetworkEcs` (ECS adapter) — component/property replication serialized into per-connection snapshot records, delta-compressed against acked baselines, with interest management and dormancy for large worlds, on a fixed network tick with render interpolation.
- `add-network-prediction`: adds `NetworkPrediction` — client-side prediction and server reconciliation.
- `add-network-lockstep`: adds `NetworkLockstep` — deterministic lockstep and rollback over the reliable-ordered channel.
- Dedicated-server build target that does not pull in Aurora.
- Deferred backends per D13: `Cluster`/`Control` (standalone Asio + OpenSSL), wasm (WebSocket), P2P (GameNetworkingSockets / WebRTC).

## Open Questions

- Default `ResumeToken` TTL and signing-key distribution mechanism (config vs environment).
- Confirm ENet as the first backend versus GameNetworkingSockets (final check against Android/iOS build cost).
- Defaults for `maxChannels`, `maxPayload`, and event-queue capacity.
- Whether `Control` (health/metrics) shares the client backend or gets a dedicated HTTP/WS backend in P1.
- Whether `INetEventSink` should be the host itself or a separate object, and the exact inbound event set (connect/disconnect/message/error).
