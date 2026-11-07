## Why

The particle subsystem currently simulates and renders on the CPU only. GPU emitters need persistent device state, compute spawn/simulate/compact passes and GPU-produced indirect draw arguments. The core API already reserves `ParticleSimTarget::GPU`, GPU feature flags and a data-only GPU contract, but no backend implementation exists.

## What Changes

- Add a GPU particle backend under `engine/particle/adaptor/render` using the persistent-buffer and indirect capabilities from `add-framegraph-gpu-driven-passes`.
- Implement per-emitter state buffers, free list, spawn/simulate/compact passes and indirect sprite rendering.
- Add capability detection and CPU fallback for missing compute/indirect/storage support.
- Add per-in-flight-frame state rotation or serialized submission.
- Add GPU readback correctness tests for test-only validation.
- Keep CPU chunk simulation and the CPU sprite path as the reference/fallback.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `particle`: Add GPU simulation, GPU state ownership, indirect rendering, capability fallback and GPU validation requirements.

## Impact

- `engine/particle/adaptor/render` backend implementation, shaders, tests and module wiring.
- Depends on `add-particle-system`, `add-framegraph-gpu-driven-passes` and `add-scene-render-stages`.
- No changes to the CPU authoring/product format beyond what `add-particle-system` already defines.
