# network Specification

## Purpose

Define transport, sessions, tick synchronization, replication and supported simulation models.

## Requirements

### Requirement: Deterministic batch iteration in the source

The framework replication source SHALL iterate replicated components grouped by type, in an order that is deterministic and independent of actor/component container layout. The source SHALL build this view itself (`World` exposes only the actor list; no per-type index is added to the framework).

#### Scenario: Grouped by type

- **WHEN** the replication source builds a snapshot over the framework world
- **THEN** it SHALL iterate replicated components grouped by type, not per-actor

#### Scenario: Layout-independent order

- **WHEN** actors or components are added and removed so container order changes
- **THEN** the replication iteration order SHALL remain stable (ordered by stable actor id)

### Requirement: Framework implements the replication seam

The framework SHALL provide an `IReplicationSource` implementation over `Actor`/`ComponentBase` that the same algorithm can use as the ECS adapter, without the algorithm depending on `Framework`.

#### Scenario: Same algorithm, framework source

- **WHEN** a replication source is provided by the framework component model
- **THEN** the replication algorithm SHALL operate over it without source-specific branching

### Requirement: Field selection from metadata

The framework replication source SHALL select replicated fields from the component reflection metadata and encode/apply them through the existing reflection and binary serialization.

#### Scenario: Unmarked fields are not replicated

- **WHEN** a component member is not marked replicated
- **THEN** its value SHALL NOT be encoded

#### Scenario: Marked fields use reflection

- **WHEN** a replicated field is encoded or applied
- **THEN** it SHALL use the existing reflection member and binary archive for its type

### Requirement: Defined apply point and host-owned lifecycle

The network host SHALL resolve spawn/despawn outside component/actor iteration, on the same thread, after the world update. The host owns actor creation and destruction and SHALL transmit the actor's full identity so the source can locate it; the source SHALL only add/remove replica components on actors that already exist.

#### Scenario: Spawn applied after tick

- **WHEN** spawn messages are received
- **THEN** the host SHALL create the actor (with its full identity) after `World::Tick` returns, not during iteration

#### Scenario: Source attaches replica components

- **WHEN** a replica is created for an entity whose actor already exists
- **THEN** the source SHALL add the replicated component to that actor

#### Scenario: No structural change during tick

- **WHEN** the world is ticking
- **THEN** the host SHALL NOT create or destroy actors

### Requirement: Side-effect-free apply

Applying replicated values SHALL NOT trigger gameplay side effects (for example component reconstruction on a field setter).

#### Scenario: Apply does not reconstruct

- **WHEN** a replicated field whose setter would normally rebuild state is applied on a client replica
- **THEN** the apply SHALL suppress that side effect

### Requirement: Client replicas are read-only

Clients SHALL apply received state and SHALL NOT propagate applied state as an authoritative change; authority SHALL be owned by the network layer.

#### Scenario: Applied state does not echo upstream

- **WHEN** a client applies replicated state
- **THEN** the applied value SHALL NOT be sent upstream as an authoritative change

### Requirement: Components declare which fields replicate

A component SHALL be able to mark a subset of its serialized fields as replicated, and non-marked fields SHALL NOT be sent to clients.

#### Scenario: Unmarked fields stay server-only

- **WHEN** a component has a field not marked as replicated
- **THEN** that field's value SHALL NOT appear in any replication message

### Requirement: Replication reuses the existing reflection and binary serialization

Replicated-field metadata SHALL be built on the existing reflection system and component binary serialization rather than a separate serialization path.

#### Scenario: Replicated serialization uses existing types

- **WHEN** a replicated field of a registered type is encoded
- **THEN** it SHALL use the existing reflection and binary archive types for that type

### Requirement: Clients create and destroy replicas from replication messages

The client SHALL create a replica for an entity on spawn and remove it on despawn, driven by replication messages rather than by load-time world data.

#### Scenario: Replica created on spawn

- **WHEN** the client receives a spawn for a replicated entity
- **THEN** the client SHALL instantiate the entity and its replicated components

#### Scenario: Replica removed on despawn

- **WHEN** the client receives a despawn for a replicated entity
- **THEN** the client SHALL remove that entity and its replicated components

### Requirement: Delta state uses a replicated-field bitmask

Each replicated component SHALL encode its delta as a bitmask of changed replicated fields followed by the changed values, so unchanged fields are omitted.

#### Scenario: Only changed fields are encoded

- **WHEN** one replicated field of a component changes
- **THEN** the encoded delta SHALL contain that field's bit and value and SHALL omit the unchanged fields

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

### Requirement: Host serializes backend access in OwnedThread mode

When the host runs an owned I/O thread, it SHALL serialize all backend access so that a backend which is not thread-safe for concurrent send is only ever touched by the I/O thread. Outbound sends and closes SHALL be deferred to that thread in this mode.

#### Scenario: Non-thread-safe backend under OwnedThread

- **WHEN** a backend that does not support thread-safe send is used in `OwnedThread` mode
- **THEN** the host SHALL route outbound sends and closes through the I/O thread instead of the caller thread

### Requirement: Simulation uses a fixed deterministic step

The deterministic simulation SHALL advance on a fixed timestep with a bounded catch-up budget, independent of render frame rate and wall-clock time.

#### Scenario: Frame rate does not affect simulation

- **WHEN** the render frame rate varies
- **THEN** the simulation SHALL advance at the configured fixed rate with no wall-clock dependence

### Requirement: Deterministic iteration order

Simulation code SHALL iterate entities and components in a deterministic order stabilized by a stable key such as the entity id, and SHALL NOT depend on unordered container iteration or on the physical layout of dense arrays (for example `SparseSet` swap-remove reordering).

#### Scenario: Stable iteration across peers

- **WHEN** two peers simulate the same tick with the same inputs
- **THEN** they SHALL visit entities and components in the same order

#### Scenario: Layout change does not change order

- **WHEN** entities are removed and re-added so that underlying dense arrays reorder
- **THEN** the iteration order SHALL be unchanged for the same live entity set

### Requirement: Deterministic random number generation

Any randomness in the simulation SHALL come from a deterministic source seeded consistently across peers, and SHALL NOT use an unspecified global RNG.

#### Scenario: Same seed same result

- **WHEN** two peers simulate a tick with the same seed and inputs
- **THEN** they SHALL produce the same random draws

### Requirement: Deterministic math mode is required

The simulation SHALL run with a deterministic math mode and SHALL NOT enable fast-math or other floating-point behavior that varies across platforms.

#### Scenario: Non-deterministic mode rejected

- **WHEN** the simulation is configured with a non-deterministic math mode
- **THEN** the system SHALL reject the configuration

### Requirement: Divergence is detected

Peers SHALL exchange a simulation hash at a configured cadence, and a mismatch SHALL be reported.

#### Scenario: Mismatch detected

- **WHEN** two peers report different simulation hashes
- **THEN** the system SHALL report a desync and initiate recovery

### Requirement: Divergence triggers resynchronization

On a desync, the system SHALL resynchronize affected peers from authoritative state.

#### Scenario: Recovery from authoritative state

- **WHEN** a desync is confirmed
- **THEN** the affected peer SHALL be resynchronized from authoritative state before continuing

### Requirement: Replication is filtered by area of interest

The server SHALL determine which entities are relevant to each connection using a spatial structure, and SHALL NOT include entities outside that connection's area of interest.

#### Scenario: Distant entities are excluded

- **WHEN** an entity is outside a connection's area of interest
- **THEN** that entity SHALL NOT be included in that connection's state messages

### Requirement: Unchanged entities are dormant

An entity whose replicated state has not changed SHALL be omitted from steady-state snapshot updates until it changes again.

#### Scenario: Static entity sent once

- **WHEN** a replicated entity does not change between ticks
- **THEN** the server SHALL NOT resend its state until it changes

### Requirement: A per-connection budget bounds bandwidth

Each connection SHALL have a bandwidth budget, and when the relevant entity set exceeds it the server SHALL order sends by priority and defer lower-priority entities.

#### Scenario: Budget exceeded defers low-priority entities

- **WHEN** the relevant entities for a connection exceed its budget
- **THEN** the server SHALL send higher-priority entities first and defer the rest to a later tick

### Requirement: State messages respect the transport payload limit

The replication layer SHALL pack state into messages that respect the maximum payload size exposed by the transport, and SHALL split across messages when needed.

#### Scenario: Oversized snapshot split

- **WHEN** the relevant state for a connection exceeds the transport payload limit
- **THEN** the replication layer SHALL split it into multiple messages within the limit

### Requirement: Host exposes an explicit lifecycle state machine

The host SHALL expose lifecycle states `Accepting`, `Draining`, and `Closed`. While `Draining`, the host SHALL stop accepting new connections but SHALL keep existing sessions served until they end or a timeout elapses.

#### Scenario: Drain stops new connections

- **WHEN** the host enters `Draining`
- **THEN** it SHALL reject new incoming connections while continuing to serve existing ones

### Requirement: Redirect moves clients during drain

While draining, the host SHALL be able to ask connected clients to reconnect to another endpoint by sending a redirect carrying a resume token.

#### Scenario: Client is redirected

- **WHEN** the host calls redirect for a connection with a target address and resume token
- **THEN** the client SHALL receive the redirect and be able to reconnect to the target endpoint using the token

### Requirement: Shutdown ordering is defined and safe

Host shutdown SHALL stop producers before releasing resources: it SHALL first signal and join the I/O thread when running in `OwnedThread` mode, then stop delivering events, and only then release transport resources. The backend `Pump` SHALL return within its `waitMs` bound so shutdown cannot stall indefinitely.

#### Scenario: No callbacks after shutdown begins

- **WHEN** host shutdown is initiated
- **THEN** no user callback SHALL be invoked after shutdown completes

### Requirement: Host reports statistics for orchestration

The host SHALL report per-backend and per-lane statistics including active connections, round-trip time, bandwidth, and packet loss, intended as input for an external orchestrator.

#### Scenario: Stats are available while serving

- **WHEN** the host is serving connections
- **THEN** it SHALL expose current connection counts and network statistics that can be read without stopping the host

### Requirement: Peers exchange input frames and simulate deterministically

Peers SHALL exchange input frames for each tick on a reliable-ordered channel and SHALL advance a fixed simulation tick without transmitting steady-state entity state.

#### Scenario: Same inputs produce the same tick

- **WHEN** all peers have received the input frame for a tick
- **THEN** each peer SHALL advance the simulation for that tick using only those inputs

### Requirement: Input frames are ordered and complete

The lockstep channel SHALL guarantee that every peer applies the same set of inputs for a given tick in the same order.

#### Scenario: No peer advances early

- **WHEN** a peer has not received the input frame for a tick
- **THEN** it SHALL wait rather than advance the simulation for that tick

### Requirement: Input delay is configurable

The system SHALL support a configurable input delay so inputs are available before their tick, and SHALL allow latency-sensitive genres to enable input prediction with rollback instead.

#### Scenario: Delay applied before tick

- **WHEN** an input is produced locally for a future tick within the input delay
- **THEN** it SHALL be scheduled and available before that tick is simulated

### Requirement: A reconnecting peer resynchronizes

A peer that rejoins after missing input frames SHALL obtain authoritative state and resynchronize before participating in lockstep again.

#### Scenario: Missed inputs recovered

- **WHEN** a peer reconnects after missing inputs
- **THEN** it SHALL resynchronize from authoritative state rather than replaying gaps

### Requirement: Server is authoritative and clients hold read-only replicas

The server SHALL own the authoritative value of every replicated entity, and clients SHALL apply received state without sending authoritative changes back.

#### Scenario: Client receives state without owning it

- **WHEN** the server changes a replicated field
- **THEN** clients SHALL receive and apply the new value, and SHALL NOT be the source of authority for that field

### Requirement: Snapshots are delta-compressed against a per-connection baseline

The server SHALL track the last snapshot acknowledged by each connection and SHALL encode subsequent state relative to that baseline.

#### Scenario: Unchanged data is not resent

- **WHEN** an entity's replicated state is unchanged since the acknowledged baseline
- **THEN** the server SHALL NOT include that data in the next state message

### Requirement: Application-level acknowledgement is distinct from transport reliability

Clients SHALL acknowledge received snapshots using the delivery sequence number exposed by the core, and the server SHALL treat that acknowledgement as the new baseline independently of transport reliability.

#### Scenario: New baseline established

- **WHEN** a client acknowledges snapshot sequence N
- **THEN** the server SHALL use snapshot N as the baseline for the next delta to that connection

### Requirement: Discrete events use a reliable-ordered channel

Spawn, despawn, and discrete gameplay events SHALL be delivered on a reliable-ordered channel separate from the unreliable state channel.

#### Scenario: Lost snapshot does not block an event

- **WHEN** a state snapshot is lost while an event is pending on the reliable channel
- **THEN** the event SHALL still be delivered in order and SHALL NOT be blocked by the lost snapshot

### Requirement: Lost baselines are repaired

If a connection does not acknowledge a baseline within a configured window, the server SHALL send a full (non-delta) snapshot to repair the connection.

#### Scenario: Repair after sustained loss

- **WHEN** a connection fails to acknowledge state for longer than the configured window
- **THEN** the server SHALL send a full snapshot that does not depend on a prior baseline

### Requirement: Repair snapshots are marked full

A snapshot that does not depend on a prior baseline (a repair) SHALL be marked as full so a client accepts it regardless of sequence continuity.

#### Scenario: Full snapshot accepted regardless of sequence

- **WHEN** a client receives a snapshot marked full
- **THEN** it SHALL apply it and advance its expected sequence

### Requirement: Gapped deltas are ignored and not acknowledged

A client SHALL apply a delta only when its sequence is contiguous with the last applied snapshot. A gapped delta SHALL be ignored and SHALL NOT be acknowledged, so the server's repair path is triggered instead of leaving the client permanently desynchronized.

#### Scenario: Lost delta recovers through repair

- **WHEN** a delta is lost and the next received sequence has a gap
- **THEN** the client SHALL ignore it without acknowledging, and SHALL resume once a full snapshot arrives

### Requirement: Replication reads through a source seam

Replication SHALL read simulation state through an engine-side replication-source interface defined in the algorithm target (`NetworkReplication`), rather than depending on a concrete world model such as `Actor`/`ComponentBase`. The algorithm target SHALL NOT depend on `Framework` or on a concrete simulation model.

#### Scenario: Source supplied by the host

- **WHEN** a world is set up for replication
- **THEN** the replication layer SHALL obtain state through the replication-source interface and SHALL NOT reference a concrete world model type

#### Scenario: Algorithm target is Framework-free

- **WHEN** the algorithm target is built
- **THEN** it SHALL NOT link `Framework`, so a pure-ECS adapter can be used without it

### Requirement: Source is adaptable by both the component model and the ECS

The source seam SHALL be implementable by both the `Actor`/`ComponentBase` model (adapter target `NetworkWorld`, links `Framework`) and the data-oriented ECS (adapter target `NetworkEcs`, links `Core`), without changing the replication algorithm.

#### Scenario: Two adapters, one algorithm

- **WHEN** a replication source is provided by the component model and, separately, by the ECS
- **THEN** the same replication algorithm SHALL operate over both without source-specific branching

### Requirement: Iteration is batched and dense

The source SHALL expose replicated entities/components in batches by type, iterated densely, so encoding does not perform per-entity virtual dispatch.

#### Scenario: Batch per type

- **WHEN** the replication layer builds a snapshot
- **THEN** it SHALL iterate replicated components grouped by type in dense batches

### Requirement: Network type identity is stable across modules

Replicated types SHALL be identified by a cross-module, call-order-independent identifier (such as the ECS compile-time tag-hash `TypeId` or a component `Uuid`). Runtime reflection SHALL NOT be used as the network identity.

#### Scenario: Identity consistent across modules

- **WHEN** the same replicated type is referenced from two modules or plugins
- **THEN** its network identifier SHALL be identical regardless of call order or module

### Requirement: Replicated state is stored in field columns

The source SHALL expose replicated state as per-field columns (structure of arrays) so delta, quantization, and bandwidth budgeting operate on fields in batches.

#### Scenario: Field-wise delta

- **WHEN** a snapshot delta is computed for a type
- **THEN** it SHALL be computed per field column rather than per entity object

### Requirement: Iteration order is deterministic

The source SHALL iterate in a deterministic order independent of underlying container layout, so `SparseSet` swap-remove reordering cannot change snapshot content or delta baselines.

#### Scenario: Order independent of container layout

- **WHEN** entities are removed and re-added so the underlying dense array order changes
- **THEN** the replication iteration order SHALL remain stable

### Requirement: Simulation state can be captured and restored deterministically

The system SHALL be able to capture the deterministic simulation state at a tick and restore it exactly, over stable entity ids.

#### Scenario: Restore reproduces state

- **WHEN** state is captured at a tick and later restored
- **THEN** the simulation SHALL continue identically to the original as if it had never advanced past that tick

### Requirement: Rollback resimulates corrected frames

On receiving a corrected or predicted input for a past tick, the system SHALL roll back to that tick, apply the input, and resimulate the frames up to the present.

#### Scenario: Late input corrects the past

- **WHEN** a peer learns that a predicted remote input was wrong for a past tick
- **THEN** it SHALL roll back to that tick, apply the correct input, and resimulate to the present

### Requirement: Rollback depth is bounded

Rollback SHALL be limited to a configured maximum number of frames, and SHALL NOT grow unbounded.

#### Scenario: Excessive lag clamps rollback

- **WHEN** a correction would require rolling back further than the configured limit
- **THEN** the system SHALL resynchronize from authoritative state instead of rolling back

### Requirement: Rollback is used only in deterministic mode

Rollback SHALL only be available when the simulation is running in the deterministic mode; it SHALL NOT be used over the state-replication path.

#### Scenario: Rollback unavailable without determinism

- **WHEN** the simulation is not in deterministic mode
- **THEN** rollback SHALL be rejected

### Requirement: Session identity is separate from the physical connection

The host SHALL represent a logical session by a stable `SessionId` that is independent of the physical `ConnectionId`, so a session can survive a disconnect and be re-bound to a new connection.

#### Scenario: Session survives reconnect

- **WHEN** a client disconnects and reconnects with a valid resume token
- **THEN** the host SHALL re-bind the existing `SessionId` to the new connection

### Requirement: Resume tokens are stateless, signed, and expiring

A `ResumeToken` SHALL be verifiable by any server holding the shared signing key, SHALL carry an expiration, and SHALL require no cluster or directory service to validate. The signature SHALL be an HMAC-SHA256 over the token fields, and verification SHALL use a constant-time comparison.

#### Scenario: Any server can validate a token

- **WHEN** a server receives a resume token issued by another server that shares the signing key
- **THEN** it SHALL validate the token without contacting a directory service

#### Scenario: Expired token is rejected

- **WHEN** a resume token is presented after its expiration
- **THEN** the host SHALL reject the resume attempt

#### Scenario: Tampered token is rejected

- **WHEN** any signed field or signature byte of a token is modified
- **THEN** verification SHALL fail

### Requirement: Reconnection uses bounded backoff

A client SHALL be able to reconnect automatically using bounded, increasing backoff and a resume token.

#### Scenario: Backoff grows then stops

- **WHEN** repeated connection attempts fail
- **THEN** the host SHALL increase the retry interval up to a configured maximum and SHALL report attempts through its statistics

### Requirement: Handover may hold multiple connections

During a redirect or re-homing, the host SHALL allow more than one connection to be active for the same session until the handover completes.

#### Scenario: Overlap during handover

- **WHEN** a session is being moved to a new endpoint
- **THEN** the host SHALL keep the old connection usable until the new connection is established or a timeout elapses

### Requirement: Replication advances on a fixed network tick

Replication SHALL advance on a fixed network tick that is independent of the render frame rate, using an accumulator that is decoupled from frame delta.

#### Scenario: Frame rate does not change tick cadence

- **WHEN** the render frame rate varies
- **THEN** the number of network ticks per second SHALL remain at the configured fixed rate

### Requirement: Remote entities are rendered by interpolation

The client SHALL render remote replicated entities by interpolating between the two most recent received snapshots, rather than snapping to the latest value.

#### Scenario: Smooth remote motion

- **WHEN** the client has two consecutive snapshots for a remote entity
- **THEN** it SHALL interpolate between them for display

### Requirement: Network timing uses the real monotonic clock

The fixed network tick and its timing SHALL be driven by the real monotonic clock, not by the world simulation time, so that pausing or slowing the simulation does not corrupt network cadence.

#### Scenario: Simulation pause does not stall the network tick

- **WHEN** the world simulation is paused
- **THEN** the network tick SHALL continue to advance on real time

### Requirement: Catch-up is bounded

When the host falls behind, the accumulator SHALL bound catch-up work per frame to avoid an unbounded tick spiral.

#### Scenario: Backlog does not spiral

- **WHEN** the host has accumulated many pending network ticks
- **THEN** it SHALL process at most a configured maximum per frame and drop the excess backlog

### Requirement: Transport seam is expressed as connections and channels

The backend contract SHALL expose connection and listener objects with channel-based sending. It SHALL NOT expose raw sockets or datagrams to consumers. Payloads SHALL be opaque byte spans, and the transport layer SHALL NOT depend on any serialization format.

#### Scenario: Consumer sends bytes without serialization knowledge

- **WHEN** a consumer sends a payload on a channel
- **THEN** the transport SHALL treat the payload as opaque bytes and SHALL NOT interpret or serialize it

### Requirement: Backends self-describe capabilities

Each backend SHALL report a capability descriptor including reliable/unreliable delivery, ordering, encryption, client/server/peer-to-peer support, threading behavior (caller-pump, host-thread, backend-owned-threads, thread-safe send, wakeup support), and default delivery mode. Consumers SHALL select behavior from capabilities rather than from a concrete backend type.

#### Scenario: Consumer routes by capability

- **WHEN** a consumer requires reliable ordering
- **THEN** it SHALL determine availability from the backend capability descriptor and SHALL fail explicitly when the backend does not support it

### Requirement: Connection handles are stable and validated

Consumers SHALL reference connections by an opaque handle composed of an index and a generation. A default handle SHALL be invalid and SHALL NOT resolve to a live connection.

#### Scenario: Stale handle does not resolve

- **WHEN** a connection is closed and its handle is later used
- **THEN** the host SHALL reject the operation because the handle no longer resolves to a live connection

### Requirement: Delivery modes are defined by the seam

The seam SHALL define delivery modes `ReliableOrdered`, `ReliableUnordered`, `UnreliableSequenced`, and `Unreliable`. A send under a mode the backend does not support SHALL be reported as an error rather than silently downgraded.

#### Scenario: Unsupported delivery mode is rejected

- **WHEN** a send requests a delivery mode the backend does not support
- **THEN** the host SHALL report an error and SHALL NOT send the message under a different mode

### Requirement: Backend registration is by role

Backends SHALL be registered and resolved by `NetworkRole`, and no consumer SHALL reference a concrete backend type.

#### Scenario: Backend resolved through the registry

- **WHEN** a host resolves its `Client` role backend
- **THEN** it SHALL do so through the registry without naming a concrete backend class

### Requirement: Backends pass a conformance suite

Every backend SHALL pass a shared conformance suite covering connect, send, each supported delivery mode, close, and queue-overflow behavior before it is considered supported.

#### Scenario: New backend validated by conformance

- **WHEN** a new backend implementation is added
- **THEN** the conformance suite SHALL exercise connect, send, delivery modes, and close against it

### Requirement: Event queue overflow causes controlled disconnect

The host inbound event queue SHALL be bounded, and when it overflows the affected connection SHALL be closed with an explicit backpressure reason instead of silently dropping events.

#### Scenario: Overflow closes the connection

- **WHEN** the inbound event queue reaches capacity for a connection
- **THEN** the host SHALL close that connection with a backpressure reason and SHALL surface the event to the consumer

### Requirement: Message size limits are exposed

The transport SHALL expose the maximum payload size a single send may use, derived from the transport MTU, so higher layers can pack multiple messages per packet and avoid unreliable messages larger than the limit.

#### Scenario: Upper layer packs within the limit

- **WHEN** a higher layer queries the maximum payload size for a channel
- **THEN** the transport SHALL return the current limit and SHALL reject an oversized send explicitly

### Requirement: Sequenced delivery exposes peer sequence numbers

For `UnreliableSequenced` delivery, the host SHALL expose the sequence number associated with each delivered message, so an upper layer can implement application-level acknowledgement and baseline tracking (distinct from transport reliability). A backend that cannot carry the sender's sequence SHALL report `realSendSequence = false`; consumers SHALL NOT rely on a synthesized sequence for loss detection.

#### Scenario: Application acks a snapshot

- **WHEN** a message is delivered on an `UnreliableSequenced` channel
- **THEN** the host SHALL report the message sequence number to the consumer so it can acknowledge the corresponding baseline

#### Scenario: Synthesized sequence is not loss-detectable

- **WHEN** a backend reports `realSendSequence = false`
- **THEN** consumers SHALL use their own application sequence for gap detection rather than the reported sequence

### Requirement: IPv4 listen endpoints complete handshakes on macOS

The ENet transport backend SHALL complete client/server handshakes when listening on an IPv4 loopback or IPv4 listen address on macOS, without requiring `ENET_IPV4_ONLY`, and SHALL continue to support explicit IPv6 listen addresses. The requested port SHALL be preserved, and `INetListener::GetAddress()` SHALL report the address passed to `Listen()`.

#### Scenario: IPv4 loopback handshake

- **WHEN** a server listens on `127.0.0.1:<port>` and a client connects to the same endpoint
- **THEN** both sides SHALL report a connected peer within the conformance timeout

#### Scenario: Explicit IPv6 listen

- **WHEN** a server listens on `[::1]:<port>` and a client connects to the same endpoint
- **THEN** both sides SHALL report a connected peer within the conformance timeout

#### Scenario: Requested address is reported

- **WHEN** `Listen()` succeeds for an IPv4 address
- **THEN** `INetListener::GetAddress()` SHALL return the requested address, including its port

### Requirement: macOS IPv4 binding limitation is documented

The backend SHALL document that, on macOS with the dual-stack ENet socket, a specific IPv4 listen address binds to all IPv4 interfaces while the requested address is retained for reporting. It SHALL NOT silently drop the requested port or switch the whole build to IPv4-only.

#### Scenario: Port is honored

- **WHEN** the server is configured for port `39555`
- **THEN** clients connecting to port `39555` SHALL reach it, regardless of the macOS bind widening

#### Scenario: IPv6 remains available

- **WHEN** the project is configured with the ENet backend on macOS
- **THEN** explicit IPv6 listeners SHALL still work and the build SHALL NOT define `ENET_IPV4_ONLY`
