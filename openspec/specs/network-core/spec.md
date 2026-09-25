# network-core Specification

## Purpose
TBD - created by archiving change add-network-core. Update Purpose after archive.
## Requirements
### Requirement: Network core is interface-, data-, and backend-agnostic logic only

`engine/network` SHALL contain interfaces, data types, and backend-agnostic host logic, and SHALL link `Core` only. It SHALL NOT include or link `Framework`, any plugin, or any render/Aurora target.

#### Scenario: Core builds without world or plugin dependencies

- **WHEN** the `Network` target is built
- **THEN** it SHALL link only `Core` and SHALL NOT include `framework/world` or any plugin header

### Requirement: Host is drivable without a World or render stack

The network host SHALL advance through an explicit update entry point that does not depend on a `World`, `IWorldSubSystem`, a window, or any render target, so a dedicated server can run it from a plain module loop.

#### Scenario: Dedicated server host without a world

- **WHEN** a process with no `World` and no render target creates a host and calls its update entry point each tick
- **THEN** the host SHALL process inbound and outbound traffic without error

### Requirement: Host composes one backend per role

The host SHALL support concurrent backends addressed by `NetworkRole` (`Client`, `Server`, `Cluster`, `Control`), allowing more than one backend to be active at a time. `Cluster` and `Control` SHALL be exposed as roles without requiring a backend implementation in this change.

#### Scenario: Server hosts two roles concurrently

- **WHEN** a host is configured with a backend for `Server` and a backend for `Cluster`
- **THEN** both backends SHALL be active simultaneously and addressable by their role

### Requirement: Backends implement a single pump primitive

Every backend SHALL implement `Pump(sink, maxEvents, waitMs)` as its only I/O advancement entry point, where `waitMs == 0` means non-blocking and `waitMs > 0` may block for at most `waitMs`.

#### Scenario: Pump does not block when waitMs is zero

- **WHEN** `Pump` is called with `waitMs == 0`
- **THEN** it SHALL return promptly after processing currently ready events

### Requirement: Host owns the threading policy

The host SHALL support a `CallerPump` mode that calls `Pump` on the caller thread and an `OwnedThread` mode that runs `Pump` on a host-owned I/O thread, without changing the backend contract.

#### Scenario: Same backend runs under either mode

- **WHEN** the same backend is used first in `CallerPump` mode and then in `OwnedThread` mode
- **THEN** both modes SHALL deliver events and accept sends without backend-specific code changes

### Requirement: User callbacks are invoked only on the caller thread

In all threading modes, user callbacks SHALL be invoked only during the host update on the calling thread, in event arrival order, and never on a backend or I/O thread.

#### Scenario: Callbacks under OwnedThread

- **WHEN** `OwnedThread` mode is active and the I/O thread receives messages
- **THEN** the user callbacks SHALL be invoked from the caller thread during the next update

### Requirement: Lane model is fixed at startup with per-connection ordering

The host SHALL assign each connection to one lane for its lifetime, with the lane count fixed at startup, so that events for a given connection are delivered in order. Each lane SHALL be an independent object so that add/remove of lanes for new connections can be introduced later without live-connection migration.

#### Scenario: Events for one connection stay ordered

- **WHEN** multiple messages arrive on one connection served by a single lane
- **THEN** the host SHALL deliver them to the user in arrival order

#### Scenario: Lane count is fixed for the process lifetime

- **WHEN** the host is created with a given lane count
- **THEN** connections SHALL be assigned only to lanes created at startup

### Requirement: Network time is independent of game time

The host SHALL use a real monotonic clock for timeouts, heartbeats, reconnect backoff, and statistics, and SHALL NOT derive them from the world tick delta.

#### Scenario: Paused world keeps the connection alive

- **WHEN** the world simulation is paused while the host update continues to be driven
- **THEN** heartbeat and timeout handling SHALL continue using real time and the connection SHALL NOT time out due to the pause

