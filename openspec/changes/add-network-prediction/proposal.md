## Why

`add-network-replication` makes state authoritative but leaves the locally controlled entity a round-trip behind: the player presses a key and sees the result only after the server confirms it. For action-oriented genres this latency is unacceptable. This change adds client-side prediction for the locally controlled entity plus server reconciliation, on top of the existing replication bridge.

## What Changes

- **Client input stream.** Inputs for the locally controlled entity are sent to the server with sequence numbers on an unreliable channel, with the last N inputs repeated in each message so a single loss does not stall the entity (rollback-style redundancy).
- **Server application and validation.** The server applies inputs in sequence order, de-duplicates repeats, validates them, and includes the last processed input sequence in the authoritative snapshot.
- **Local prediction.** The client applies its own input immediately to the locally controlled entity and simulates it forward.
- **Server reconciliation.** When an authoritative snapshot arrives, the client resets the predicted entity to the authoritative state and replays unacknowledged inputs to reproduce the current frame.
- **Error smoothing.** Residual correction is blended into a visual offset so reconciliation does not snap.
- **Scope.** Only locally controlled entities are predicted; remote entities remain interpolated as defined by `add-network-replication`.
- **Non-goals**: rollback replay of the whole simulation (`add-network-lockstep`), deterministic simulation guarantees, and prediction of server-only entities.

## Capabilities

### New Capabilities

- `network-input-stream`: client-to-server input commands with sequencing, ordering, de-duplication, and loss-tolerant redundancy.
- `network-prediction`: local prediction of the locally controlled entity, server reconciliation by replay, and visual error smoothing.

### Modified Capabilities

<!-- None: this adds prediction on top of the replication bridge; replication requirements are unchanged. -->

## Impact

- **Engine**: adds `NetworkPrediction` (`engine/network/prediction/`, links `NetworkReplication`); `engine/network` core and the replication algorithm are unchanged.
- **World/ECS**: predicted components (movement/controller) opt in to prediction; a per-entity prediction scope is required.
- **Consumers**: games mark which entity/controller is locally predicted and handle the input send/receive path.
- **Compatibility**: additive; depends on `add-network-core` and `add-network-replication`.
