> **Current status - 2026-11-05: planned.** Depends on `add-particle-system`, `add-framegraph-gpu-driven-passes` and `add-scene-render-stages`.

## 1. Backend resources

- [ ] 1.1 Implement per-emitter persistent state A/B, free list, counters and indirect argument buffers
- [ ] 1.2 Validate compiled attribute layout against GPU state stride and shader declarations
- [ ] 1.3 Implement per-in-flight-frame state rotation or serialized GPU submission
- [ ] 1.4 Release persistent resources on retirement without dropping frames in flight

## 2. Compute passes

- [ ] 2.1 Implement reset and spawn compute passes
- [ ] 2.2 Implement simulate compute using double-buffered state
- [ ] 2.3 Implement compact and indirect-argument generation
- [ ] 2.4 Add optional sort pass for alpha blending

## 3. Rendering

- [ ] 3.1 Implement indirect sprite rendering from the state buffer
- [ ] 3.2 Add texture and sub-UV sampling to the GPU sprite path
- [ ] 3.3 Add capability detection and CPU/non-indirect fallback
- [ ] 3.4 Report GPU fallbacks and budget state through stats

## 4. Validation

- [ ] 4.1 Add GPU readback tests for spawn/simulate/compact correctness
- [ ] 4.2 Add indirect argument readback tests
- [ ] 4.3 Add multi-frame-in-flight tests
- [ ] 4.4 Run Vulkan/Metal/DX12 validation on available hosts and record unavailable checks
