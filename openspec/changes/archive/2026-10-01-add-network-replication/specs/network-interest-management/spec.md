## ADDED Requirements

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
