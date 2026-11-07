> **Current status - 2026-11-07: review corrections implemented and verified on macOS; remaining work is listed below.** CPU sprite path can land independently; GPU simulation/render requires `add-framegraph-gpu-driven-passes` and `add-scene-render-stages`.

## 1. Build, ownership and metadata

- [ ] 1.1 Confirm `add-framegraph-gpu-driven-passes` and `add-scene-render-stages` are complete before starting tasks 6.1+ (N/A while section 6 is deferred to `add-particle-gpu`)
- [x] 1.2 Add `SKY_BUILD_PARTICLE` to `cmake/options.cmake` and wire `engine/particle` from `engine/CMakeLists.txt`
- [x] 1.3 Create `engine/particle/{core,assets,adaptor,builder,editor}` targets with the designed dependency boundaries
- [x] 1.4 Create `Particle.Assets`, `Particle.World`, `Particle.Render`, `Particle.Builder` and `Particle.Editor` modules and register them in `configs/modules_*.json`
- [x] 1.5 Register the `particle` domain in `openspec/README.md` and `openspec/capability-map.json`
- [x] 1.6 Add `engine/particle/resources.json` and shader resource staging registration
- [x] 1.7 Add the particle engine-subsystem exception to `AGENTS.md` and the modules documentation

## 2. Core data model and compiler

- [x] 2.1 Define `ParticleAttribute`, attribute layout, fixed-capacity storage and parameter value types
- [x] 2.2 Define effect/system/emitter/module/renderer authoring descriptors with stable IDs
- [x] 2.3 Implement the pure compiler: validation, layout generation, CPU module lowering, GPU feature mask and stable hash
- [x] 2.4 Implement PCG-based deterministic random, curve and gradient evaluation
- [x] 2.5 Add core unit tests for layout, compiler stability, invalid combinations and parameter evaluation

## 3. Assets and cook

- [x] 3.1 Implement `ParticleEffectAssetData` serialization and reflection
- [x] 3.2 Register `AssetTraits<ParticleEffectAsset>` and the runtime asset handler
- [x] 3.3 Wire the source `.particle` schema and cooked product contract
- [x] 3.4 Implement the `Particle.Builder` cook step with diagnostics and hash generation
- [x] 3.5 Add asset roundtrip, cook success/failure and hot-reload hash tests

## 4. World runtime

- [x] 4.1 Implement `ParticleSystem : IWorldSubSystem` with the `"Particle"` name
- [x] 4.2 Implement `ParticleComponent` with reflection and required sub-system registration
- [x] 4.3 Implement effect instance lifecycle, spawn rate, bursts, warmup and event queue
- [x] 4.4 Implement fixed-capacity SoA storage and synchronous CPU simulation in the subsystem tick (chunked task-dispatcher execution is a follow-up)
- [x] 4.5 Implement budget management, significance/LOD and cull policy
- [x] 4.6 Implement immutable `ParticleRenderFrame` snapshots and the registered render sink
- [x] 4.7 Add World/component lifecycle, budget, determinism and snapshot ownership tests

## 5. CPU rendering

- [x] 5.1 Register the particle scene extension after opaque using the scene color/depth context
- [x] 5.2 Implement per-particle instance data packing and upload
- [x] 5.3 Implement instanced sprite draws with billboard modes, sub-UV and blend modes
- [x] 5.4 Add particle shader declarations, programs and variant keys
- [x] 5.5 Add CPU sprite packing, frozen packet, output isolation and texture-version tests

## 6. GPU runtime (deferred to `add-particle-gpu`)

This change ships the CPU sprite path. The GPU runtime tasks below move to `add-particle-gpu` together with
the `ParticleGpu.h` contract; they stay unchecked here only to preserve the original scope record.

- [ ] 6.1 Implement persistent state buffers, free list and per-emitter resource ownership
- [ ] 6.2 Implement spawn, simulate, compact and indirect-argument compute passes
- [ ] 6.3 Implement GPU indirect sprite rendering from the persistent state buffer
- [ ] 6.4 Implement per-in-flight-frame state rotation or serialized submission
- [ ] 6.5 Implement capability detection, CPU fallback and explicit unsupported errors
- [ ] 6.6 Add GPU readback correctness tests for spawn/simulate/compact and argument generation

## 7. Editor

- [x] 7.1 Implement the effect asset editor view with system/emitter tree and module stack editing
- [x] 7.2 Use reflected forms for parameters, curves and gradients with undoable commands
- [x] 7.3 Implement the preview backend seam and register the render-side implementation
- [x] 7.4 Add statistics and debug views for count, sim/render time, memory and budget state
- [x] 7.5 Add editor tests for stack editing, parameter commands and preview lifecycle
- [x] 7.6 Preview unsaved drafts through the real preview world using the world-scoped in-memory effect registry, with the CPU overlay only as fallback
- [x] 7.7 Select module parameters in the stack and edit their ParticleValue (kind/constant/vector/range) through the reflected form
- [ ] 7.8 Add key-list editors for curve/gradient parameter values (currently authored in JSON)

## 8. Validation and follow-up

- [x] 8.1 Build affected engine, plugin, module and test targets
- [x] 8.2 Run core, asset, World, render and editor tests; record unavailable platform checks explicitly
- [x] 8.3 Run CPU simulation and death-reclamation benchmarks; leave GPU budgets to the GPU follow-up
- [x] 8.4 Update design documentation and verification evidence without commit or archive

## 9. Review corrections

- [x] 9.1 Preserve source fields, reject malformed JSON and unsupported module/value semantics, and hash complete programs
- [x] 9.2 Use linear death reclamation, reusable event buffers, nonblocking asset rebinding and correct WORLD/LOCAL transforms
- [x] 9.3 Freeze per-World snapshots and exact texture versions in the existing packet payload; retire unused resources
- [x] 9.4 Use stable document commands for property/structural undo and reuse compiled preview programs
- [x] 9.5 Collect engine shader inventories for project cook and verify ParticleSprite requests
- [x] 9.6 Build affected production targets and run focused regression tests; record unavailable native platforms
