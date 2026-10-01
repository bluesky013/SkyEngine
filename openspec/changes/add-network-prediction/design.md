## Context

`add-network-replication` defines an authoritative server with snapshot/delta state and interpolated remote entities. It deliberately excludes client prediction. This change layers prediction and reconciliation over that bridge, limited to the locally controlled entity, so input feels instant while the server stays authoritative. It relies on the fixed network tick and the differential state machinery already defined by replication.

## Goals / Non-Goals

**Goals:**

- Instant local response for the locally controlled entity under normal latency.
- Correct convergence to authoritative state without visible snapping.
- Tolerate isolated input/state packet loss without stalling the entity.

**Non-Goals:**

- Rollback replay of the full simulation (`add-network-lockstep`).
- Deterministic simulation; prediction here is best-effort and self-correcting, not bit-exact.
- Predicting remote entities or whole-world physics.

## Decisions

### D1. Predict only the locally controlled entity

Prediction applies to the entity the local client controls; all other entities remain interpolated from snapshots.

**Why:** predicting divergent remote entities without determinism produces fighting corrections; scope-limited prediction is the standard, stable choice (Unreal character movement, Unity Netcode). **Alternative:** predict all entities — needs determinism and belongs to lockstep/rollback.

### D2. Inputs are sequenced and redundantly sent

Each input carries a monotonically increasing sequence; messages repeat the last N unacknowledged inputs.

**Why:** unreliable channels lose packets; repeating recent inputs lets the server recover without a retransmit round trip. This is the same trick rollback netcode uses. **Trade-off:** a small bandwidth overhead proportional to N.

### D3. The snapshot carries the last processed input sequence

Authoritative snapshots include the highest input sequence the server has applied for that entity.

**Why:** the client needs to know exactly which inputs are still pending so it replays only those. **Alternative:** acknowledge inputs out of band — more messages, no benefit.

### D4. Reconciliation is reset-and-replay

On an authoritative snapshot, the client sets the predicted entity to the authoritative state and replays all inputs after the acknowledged sequence.

**Why:** guarantees convergence and is simple to reason about. **Trade-off:** replay cost proportional to RTT / tick; bounded because unacknowledged input count is small.

### D5. Residual error is smoothed visually, not logically

The logical predicted state is corrected exactly; a separate render offset blends the previous visual position toward the new one.

**Why:** exact correction without a visual blend produces jitter; smoothing hides small mispredictions without affecting gameplay truth.

### D6. Prediction is opt-in per controlled component

Only components that implement the prediction interface participate; others replicate normally.

**Why:** projectiles, AI, and scripted objects should not be predicted by the client unless intended. **Alternative:** predict everything the server does — rejected, it invites divergence.

### D7. Prediction is a separate sub-target over replication

`NetworkPrediction` lives in `engine/network/prediction/` and links `NetworkReplication`; it modifies neither `engine/network` core nor the replication algorithm.

**Why:** prediction needs the replication baselines and source seam but is a distinct concern, so a separate target keeps non-predicting games from linking it and preserves the refactor seams defined in `add-network-core` D1.

## Risks / Trade-offs

- **Misprediction under packet loss or high latency** -> redundancy (D2) plus error smoothing (D5); bounded replay.
- **Replay cost grows with RTT** -> cap replayed inputs and treat excessive divergence as a full correction.
- **Cheating via malicious inputs** -> the server validates and clamps inputs; prediction never grants authority.
- **Interaction with interpolation** -> predicted entity bypasses interpolation; remote entities keep it, so the two paths must not double-apply.
- **Determinism mismatch between client and server replay** -> reconciliation is self-correcting, so exact bit-equality is not required here.

## Follow-up Changes

- `add-network-lockstep`: deterministic lockstep and rollback for RTS/fighting.

## Open Questions

- Input redundancy count N and input send cadence defaults.
- Which components are predict-eligible in the first version (movement/controller only?).
- Smoothing time constant and its interaction with high-latency teleports.
- Whether inputs are compressed/quantized and the encoded input layout.
