## Why

SkyEngine has no particle system. Effects currently require ad-hoc rendering code, and there is no shared CPU/GPU simulation model, authoring asset, or editor workflow. A data-oriented engine subsystem is needed so gameplay, rendering and tooling share one compiled effect contract.

## What Changes

- Introduce an engine-owned `engine/particle` subsystem with `core`, `assets`, `adaptor`, `builder` and `editor` targets, following the existing core/adaptor/builder/editor mechanism.
- Add a Niagara-style system/emitter/module-stack effect model with fixed attribute layout, user parameters, curves, gradients and renderer descriptors.
- Add a pure compiler that lowers effect assets into a CPU module program and a GPU feature mask plus renderer variant keys.
- Add a World sub-system and component for effect instances, CPU simulation, budgets, significance/LOD and render snapshots.
- Add an Aurora render adaptor: CPU instanced sprite rendering and GPU spawn/simulate/compact/render using persistent buffers and indirect draws.
- Add particle effect asset schema, serialization, reflection, cook and hot reload.
- Add an effect editor with module stack, parameter/curve/gradient editing, preview and statistics.
- Add `SKY_BUILD_PARTICLE`, module manifests and build integration.

## Capabilities

### New Capabilities

- `particle`: Engine-owned particle subsystem covering effect authoring model, compiled programs, CPU/GPU simulation, rendering, budgets/LOD, editor and failure behavior. The subsystem has an independent runtime and asset boundary and is not a renderer-only feature, so it needs a new canonical domain.

### Modified Capabilities

- `modules`: Particle is an engine subsystem like Animation, with interfaces and implementations under `engine/particle`, not `plugins/particle`.
- `build`: Add the `SKY_BUILD_PARTICLE` option, target layout and module packaging for the engine subsystem.
- `assets`: Add the particle effect source/product asset type, reflection, cook and runtime loading contract.

## Impact

- New `engine/particle/{core,assets,adaptor,builder,editor}` targets, modules, shaders and tests.
- `engine/CMakeLists.txt`, `cmake/options.cmake`, `configs/modules_*.json`, `openspec/README.md` and `capability-map.json`.
- Depends on `add-framegraph-gpu-driven-passes` (persistent buffers and indirect declarations) and `add-scene-render-stages` (post-opaque/pre-tonemap sharing of scene color and depth).
- CPU sprite path can land first; GPU path is gated on the two prerequisite changes.
- No new third-party dependencies.
