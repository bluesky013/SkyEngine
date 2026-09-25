## 1. Module scaffold

- [x] 1.1 Create the `engine/network` tree (`CMakeLists.txt`, `include/network/`, `src/`) and register it in `engine/CMakeLists.txt`
- [x] 1.2 Configure the `Network` static target to link only `Core`, with `PUBLIC_INC include` and `PRIVATE_INC src`
- [x] 1.3 Add a `NetworkTest` googletest target behind `SKY_BUILD_TEST` and confirm it builds
- [x] 1.4 Verify `engine/network` headers include no `framework/world` or plugin headers, and the target links no plugin/`Framework`

## 2. Transport contract and data types

- [x] 2.1 Add `NetworkAddress` with parse/format for host, IPv4, and IPv6
- [x] 2.2 Add `ConnectionId { index, generation }` with `INVALID`, validity check, and hash, mirroring `PhysicsObjectId`
- [x] 2.3 Add `DeliveryMode`, `DisconnectReason`, and the transport error result type
- [x] 2.4 Add `NetworkBackendCaps` (delivery/ordering, encryption, client/server/peer-to-peer, threading behavior, default mode, channel/payload limits)
- [x] 2.5 Define `INetConnection`, `INetListener`, and `INetBackend` (`Pump`, `Listen`, `Connect`, `GetCaps`, `Init`/`Shutdown`)
- [x] 2.6 Add `INetEventSink` and the inbound event set (connect, disconnect, message, error)
- [x] 2.7 Implement `NetworkBackendRegistry` keyed by `NetworkRole`, allowing concurrent backends
- [x] 2.8 Expose the per-channel maximum payload size (MTU-derived) and reject oversized sends
- [x] 2.9 Expose the message sequence number on `UnreliableSequenced` delivery for application-level acknowledgement

## 3. Host, roles, and threading

- [x] 3.1 Add `NetworkHostConfig` (role, lane count, listeners, heartbeat/timeout/reconnect policy)
- [x] 3.2 Implement `NetworkHost` composition of one backend per role and its public send/connect/update surface
- [x] 3.3 Implement `CallerPump` mode: `Update` calls `Pump(..., 0)` on the caller thread and drains events
- [x] 3.4 Implement `OwnedThread` mode: host-owned I/O thread looping `Pump(..., waitMs)`, bounded inbound queue, semaphore wake for outbound
- [x] 3.5 Guarantee callbacks run only on the caller thread, in arrival order, in both modes
- [x] 3.6 Implement heartbeat, timeout, and backoff timers on `std::chrono::steady_clock`, driven by host update (not world delta)
- [x] 3.7 Add a dependency-injection seam so tests can drive the clock

## 4. Lane model

- [x] 4.1 Implement `Lane` as an independent object owning its connection set and counters (per-lane queue/IO thread deferred: the backend `Pump` advances all connections)
- [x] 4.2 Assign each connection to a lane at accept/connect for its lifetime
- [x] 4.3 Verify per-connection event ordering under a single lane
- [x] 4.4 Structure lane storage so add/remove of lanes for new connections can be added later without live-connection migration (documented, not enabled)

## 5. Session identity and resume

- [x] 5.1 Add `SessionId` and the connection-to-session binding table in the host
- [x] 5.2 Implement `ResumeToken` issue/verify with HMAC signing and expiration, requiring no directory service
- [x] 5.3 Implement client reconnect with bounded, increasing backoff
- [x] 5.4 Support holding old and new connections for one session during handover, with a timeout

## 6. Lifecycle, drain, and statistics

- [x] 6.1 Implement the `Accepting -> Draining -> Closed` state machine with new-connection rejection while draining
- [x] 6.2 Implement `RedirectTo(address, token)` to move a client during drain
- [x] 6.3 Implement shutdown ordering: stop/join producers, then stop event delivery, then release transport resources
- [x] 6.4 Implement `NetworkHostStats` (connections, RTT, bandwidth, loss, per-lane load) readable without stopping the host

## 7. Loopback backend and conformance tests

- [x] 7.1 Implement a deterministic in-process loopback backend for tests
- [x] 7.2 Build a shared backend conformance suite (connect, send, each supported delivery mode, close, overflow)
- [x] 7.3 Add deterministic `CallerPump` tests for delivery, ordering, and delivery-mode rejection
- [x] 7.4 Add an `OwnedThread` concurrency/stress test asserting callback thread and no lost events
- [x] 7.5 Add a queue-overflow test asserting controlled disconnect with backpressure reason
- [x] 7.6 Add a session test for reconnect, token expiry rejection, and multi-connection handover
- [x] 7.7 Add a drain/redirect test asserting new connections are rejected and existing sessions redirect

## 8. First backend plugin (ENet)

- [x] 8.1 Scaffold `plugins/<enet-backend>` (`plugin.json`, CMake, module) and register it in `plugins/plugins.json`
- [x] 8.2 Implement the backend over ENet with `Pump`, `Listen`, `Connect`, and a truthful `NetworkBackendCaps` (`wakeupSupport=false`, no backend-owned threads)
- [x] 8.3 Register the backend module and wire role-based resolution
- [x] 8.4 Run the shared conformance suite against the ENet backend
- [x] 8.5 Validate `Client`/`Server` roles end to end (connect, send on reliable and unreliable channels, close)

## 9. Validation and deferred scope

- [x] 9.1 Build the engine and run `NetworkTest` on the desktop configuration
- [x] 9.2 Confirm `engine/network` contains no World/ECS types and no concrete backend references
- [x] 9.3 Document deferred items: `engine/network/world` (`NetworkWorld` bridge), `Cluster` backend, `Control` backend, dedicated-server target, and WebSocket/wasm backend
