## Why

SkyEngine has no networking layer at all: multiplayer servers and clients cannot be built on the engine today. Per the repository module rules, the engine side must be a backend-swappable contract (interfaces + data only) with implementations in plugins, exactly like `engine/physics` + `plugins/bullet`. Two properties are cheap to design in now and expensive to retrofit later: **session identity decoupled from physical connections** (needed for reconnect, re-homing, and elastic scaling) and a **role/lane model** that lets a server host client, cluster, and control traffic concurrently. Establishing the core contract before any gameplay or replication code depends on it avoids a painful re-design.

## What Changes

- **New `engine/network` interface-only module.** Depends on `Core` only; contains no World/ECS/Aurora coupling. The host is driven by `Update()`/`Pump`, so a dedicated server can run it without a window, render stack, or world.
- **Transport seam with capability descriptors.** `INetBackend` / `INetConnection` / `INetListener`, an address type, connection handles (`index + generation`, mirroring `PhysicsObjectId`), delivery modes (`ReliableOrdered` / `ReliableUnordered` / `UnreliableSequenced` / `Unreliable`), and a `NetworkBackendCaps` descriptor. Backends self-describe capabilities; consumers never name a concrete backend.
- **Role-composed host, not a single active backend.** `NetworkRole { Client, Server, Cluster, Control }`; the host composes one backend per role. `Cluster` and `Control` are **reserved as enum + composition only, no backend** in this change.
- **Unified threading contract.** Backends implement one primitive, `Pump(sink, maxEvents, waitMs)`; the host owns the policy: `CallerPump` (reference path, tests, wasm, lockstep determinism) and `OwnedThread` (client/server, keeps the render/main thread free). User callbacks are invoked on the caller thread in both modes.
- **Lane model fixed at startup, structured for later growth.** `laneCount` is fixed; each lane is an independent object (own queue + thread). The structure allows later add/remove of lanes for new connections **without live-connection migration**; runtime elasticity proper stays at the process/orchestration layer.
- **Session identity separate from connections.** `SessionId` is stable across reconnects; `ConnectionId` is the physical handle. `ResumeToken` is a **stateless, expiring signed token** in this change (no cluster infrastructure required).
- **Lifecycle and observability.** An `Accepting -> Draining -> Closed` state machine with `RedirectTo(address, token)`, graceful shutdown ordering (stop producers, then drain, then release sockets), and `NetworkHostStats` (connections, RTT, bandwidth, loss, per-lane load) intended as autoscaler input.
- **A first backend to validate the seam.** One concrete `INetBackend` plugin (ENet) proves the contract against a real UDP transport for `Client`/`Server` roles, plus a deterministic loopback test backend.

## Capabilities

### New Capabilities

- `network-core`: the `engine/network` module boundary (Core-only, World/ECS-free), the host update/pump model, role composition, and the startup-fixed lane model with a later add/remove path.
- `network-transport`: the backend contract (`INetBackend`/`INetConnection`/`INetListener`), address and connection-handle types, delivery modes, capability descriptor, and backend registration.
- `network-session`: session identity (`SessionId`) decoupled from `ConnectionId`, the stateless signed `ResumeToken`, reconnection with backoff, and multi-connection handover during redirect.
- `network-lifecycle`: the `Accepting -> Draining -> Closed` state machine, `RedirectTo`, graceful shutdown ordering, and `NetworkHostStats` reporting.

### Modified Capabilities

<!-- None: this introduces a new engine module; no existing spec requirements change. -->

## Impact

- **Engine**: new `engine/network` target (`sky_add_library(TARGET Network STATIC)`) linking `Core` only; no `framework/world`, no plugin, no render dependency. Added to `engine/CMakeLists.txt`.
- **Plugins**: new `plugins/<first-backend>` (ENet) registering its backend under the transport seam, plus a loopback backend for tests; `plugins/plugins.json` and plugin CMake wiring updated. No other plugin is required to change.
- **Build/config**: backend enable switch follows the existing plugin pattern; server builds must not pull in Aurora. A dedicated-server target is **not** introduced here (tracked separately).
- **Consumers**: none yet. The full `engine/network` target family is planned (see design D1) as `Network` (this change), `NetworkReplication`, `NetworkWorld`, `NetworkEcs`, `NetworkPrediction`, `NetworkLockstep`; only `Network` is built here and the rest are out of scope.
- **Compatibility**: additive; no existing runtime data or API is affected.
