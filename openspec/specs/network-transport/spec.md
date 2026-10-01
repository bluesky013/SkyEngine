# network-transport Specification

## Purpose
TBD - created by archiving change add-network-core. Update Purpose after archive.
## Requirements
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

