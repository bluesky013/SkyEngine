## ADDED Requirements

### Requirement: Inputs are sequenced

Each input command for the locally controlled entity SHALL carry a monotonically increasing sequence number.

#### Scenario: Sequence increases per input

- **WHEN** the client produces input commands for a tick
- **THEN** each command SHALL carry a sequence number greater than the previous one

### Requirement: Inputs tolerate loss by redundancy

Input messages SHALL repeat the most recent unacknowledged inputs so a single lost message does not stall server application.

#### Scenario: One lost input message recovers

- **WHEN** an input message is lost
- **THEN** a subsequent message carrying the repeated inputs SHALL allow the server to apply the missing inputs

### Requirement: The server applies inputs in order and de-duplicates

The server SHALL apply inputs in sequence order, SHALL ignore inputs at or below the last applied sequence, and SHALL validate each input before applying it.

#### Scenario: Duplicate input ignored

- **WHEN** the server receives an input whose sequence was already applied
- **THEN** the server SHALL NOT apply it again

### Requirement: The authoritative snapshot reports the last processed input

Authoritative snapshots for a predicted entity SHALL include the highest input sequence the server has applied.

#### Scenario: Client learns which inputs are pending

- **WHEN** a client receives an authoritative snapshot for its predicted entity
- **THEN** it SHALL determine from the reported sequence which of its inputs remain unacknowledged
