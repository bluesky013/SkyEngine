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

### Requirement: The locally controlled entity is predicted immediately

The client SHALL apply locally generated inputs to the locally controlled entity immediately, without waiting for an authoritative snapshot.

#### Scenario: Immediate local response

- **WHEN** the player provides input for the locally controlled entity
- **THEN** the client SHALL simulate that input locally before any server confirmation

### Requirement: Authoritative state triggers reconciliation by replay

On receiving an authoritative snapshot for the predicted entity, the client SHALL reset it to the authoritative state and replay every input after the acknowledged sequence.

#### Scenario: Correction replays pending inputs

- **WHEN** an authoritative snapshot arrives carrying the last processed input sequence
- **THEN** the client SHALL reset the predicted entity to that state and replay the still-unacknowledged inputs

### Requirement: Prediction is limited to locally controlled entities

Only entities controlled by the local client SHALL be predicted; all other entities SHALL remain interpolated from authoritative snapshots.

#### Scenario: Remote entities are not predicted

- **WHEN** a remote entity is replicated to the client
- **THEN** the client SHALL render it by interpolation and SHALL NOT apply local prediction to it

### Requirement: Residual correction is smoothed for display only

Any visual correction of a predicted entity SHALL be applied as a display offset and SHALL NOT alter the authoritative logical state.

#### Scenario: Correction does not snap

- **WHEN** reconciliation changes the predicted position
- **THEN** the client SHALL blend the displayed position toward the corrected one without changing the logical state
