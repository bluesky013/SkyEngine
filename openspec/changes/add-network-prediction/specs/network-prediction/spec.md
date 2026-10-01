## ADDED Requirements

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
