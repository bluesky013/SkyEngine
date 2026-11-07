## Context

`engine/particle/core` defines GPU simulation compatibility flags, `ParticleSimTarget::GPU`, the render snapshot and a data-only `ParticleGpu.h` contract. `add-framegraph-gpu-driven-passes` provides persistent buffer import and indirect draw/dispatch declarations; `add-scene-render-stages` provides the pre-tonemap scene extension point. This change implements the backend that uses those seams.

## Goals / Non-Goals

**Goals:**

- Fixed-capacity GPU emitters with persistent state across frames.
- Spawn, simulate, compact and indirect render in one graph frame.
- Capability detection and explicit CPU fallback.
- Correct multi-frame ownership and testable readback.

**Non-Goals:**

- GPU determinism across vendors.
- GPU event readback or GPU-to-CPU gameplay events.
- Ribbon/light/mesh GPU renderers in the first GPU increment.
- GPU sorting and soft particles (separate follow-up if needed).

## Decisions

### State layout

Per emitter:

```text
stateA / stateB            structured particle state, ping-pong
freeList / aliveIndices    uint
spawnRequests / counters
indirectArgs
emitterParams
curve/gradient LUTs
```

The compiled `ParticleAttributeLayout` defines the state stride; shader declarations are validated against it. Capacities are fixed at cook time.

### Frame passes

```text
ResetCounters -> Spawn -> Simulate(A->B) -> Compact/BuildIndirect -> [Sort] -> Render
```

Spawn allocates from the free list. Simulate reads one buffer and writes the other. Compact writes the alive count and indirect arguments. Render reads the state buffer by instance ID. No CPU readback is required for rendering.

### Ownership and frames in flight

`Particle.Render` owns persistent buffers through the feature lifetime. The graph imports them per frame and retains them until submission completes. State is rotated per in-flight frame or submission is serialized for GPU emitters; the graph does not invent cross-frame synchronization.

### Capability fallback

If compute, storage buffers, indirect draw or indirect dispatch are unavailable, the emitter falls back to CPU simulation or a non-indirect fixed-count draw as supported. Fallbacks are explicit and counted; no silent incorrect output.

### Events

GPU collision/death events are not part of the first backend. CPU events remain the authoritative gameplay path.

## Risks / Trade-offs

- In-flight state races -> per-frame rotation or serialized submission; multi-frame tests.
- Mobile compute/indirect gaps -> capability tiers and CPU fallback.
- Shader/state layout drift -> builder validation plus reflection tests.
- VRAM growth -> fixed budgets and per-emitter capacity limits.

## Migration Plan

1. Add backend buffers/passes behind existing GPU emitter flags.
2. Add capability detection and fallback.
3. Add readback correctness tests.
4. Enable GPU emitters in the editor/runtime behind the option.
