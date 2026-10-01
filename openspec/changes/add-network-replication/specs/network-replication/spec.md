## ADDED Requirements

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
