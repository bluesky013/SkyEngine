## 1. Input stream

- [ ] 1.1 Define the input command payload (sequence + input data) in the replication bridge
- [ ] 1.2 Send inputs from the client on the unreliable channel with the last N unacknowledged inputs repeated
- [ ] 1.3 Apply inputs on the server in sequence order with de-duplication and validation
- [ ] 1.4 Include the last processed input sequence in authoritative snapshots for predicted entities
- [ ] 1.5 Add tests for ordering, de-duplication, and recovery from one lost input message

## 2. Prediction and reconciliation

- [ ] 2.1 Add a prediction interface so controlled components opt in
- [ ] 2.2 Simulate local inputs immediately on the client for the predicted entity
- [ ] 2.3 On authoritative snapshot, reset the predicted entity and replay unacknowledged inputs
- [ ] 2.4 Exclude predicted entities from the interpolation path to avoid double application
- [ ] 2.5 Cap replayed inputs and fall back to full correction when divergence is excessive
- [ ] 2.6 Add a visual error-smoothing offset that does not modify logical state

## 3. Validation

- [ ] 3.1 Build and run prediction tests under simulated latency and loss
- [ ] 3.2 End-to-end check: local input feels instant and converges to authoritative state without visible snapping
- [ ] 3.3 Confirm `engine/network` core is unchanged
- [ ] 3.4 Document deferred items: lockstep/rollback, input quantization, prediction of non-movement components
