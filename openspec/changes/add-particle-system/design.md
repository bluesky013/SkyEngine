## Context

SkyEngine has no particle subsystem. Terrain and vegetation own their core in `engine/*` and their implementation in `plugins/*`; Animation and Aurora are explicit engine-owned exceptions. Particle spans pure simulation, World integration, Aurora rendering, offline cooking and editor tooling, so it is treated as an engine subsystem with the same core/adaptor/builder/editor split rather than a business plugin.

The renderer prerequisites are not yet available:

- Persistent GPU buffers and indirect draw/dispatch declarations are owned by `add-framegraph-gpu-driven-passes`.
- Sharing scene color/depth after opaque and before tonemap is owned by `add-scene-render-stages`.

The CPU sprite path can be implemented without either prerequisite; the GPU path depends on both.

## Goals / Non-Goals

**Goals:**

- One compiled effect contract shared by CPU simulation, GPU simulation, rendering, cook and editor.
- Data-oriented CPU simulation with fixed-capacity pools and deterministic ordering.
- GPU simulation with persistent state, spawn/simulate/compact passes and indirect rendering.
- Niagara-like authoring model without committing to a full node graph in v1.
- Clear target boundaries so core stays free of Framework/Aurora/editor/builder dependencies.
- Graceful fallback when GPU capabilities are missing.

**Non-Goals:**

- No arbitrary shader code generation at runtime.
- No GPU determinism guarantee across vendors.
- No Niagara Data Channels equivalent in v1.
- No ribbon/light/shadow/soft-particle renderers in the first delivery.
- No OIT integration.
- No particle physics solver beyond simple planar/depth collision in v1.

## Decisions

### Engine subsystem ownership and target split

```text
engine/particle/
  core/        Particle (STATIC)              pure data, compiler, CPU solver
  assets/      Particle.Assets.Static + module schema, serialization, reflection
  adaptor/     Particle.Adaptor.Static + Particle.World module
               Particle.Render.Static + Particle.Render module
  builder/     Particle.Builder.Static + Particle.Builder module
  editor/      Particle.Editor.Static + Particle.Editor module
  resources.json
```

Dependencies:

- `Particle` links `Core` only.
- `Particle.Assets.Static` links `Particle`, `Framework`.
- `Particle.Adaptor.Static` links `Particle`, `Particle.Assets.Static`, `Framework` (no Aurora; the world layer is render-agnostic).
- `Particle.Render.Static` links `Particle`, `Particle.Assets.Static`, `Aurora.Core.Static`, `Aurora.Adaptor.Static`, `Framework`.
- `Particle.Builder.Static` links `Particle`, `Particle.Assets.Static`, `Framework` (framework `AssetBuilder` is the cook boundary).
- `Particle.Editor.Static` links `Particle`, `Particle.Assets.Static`, `Framework`, `EditorCore`, `EditorUi`, `EditorWidgets`.
- `AssetTool` loads `Particle.World` only for world-cook metadata (component persistence schema and the `Particle` subsystem), mirroring `Aurora.Cook` loading `Aurora.Adaptor.Static`.

Rationale: this mirrors Animation and keeps the render feature out of core. A `plugins/particle` split was considered but rejected because the subsystem is engine technology, not game content, and the user explicitly requested `engine/particle`.

The `modules` spec is amended to record Particle as the third engine subsystem exception after Aurora and Animation.

### Authoring model: stack, not graph

```text
ParticleEffect
  userParameters[]
  systemModules[]
  scalability / effectType
  emitter[]
    simTarget: CPU | GPU
    capacity / spawn / simulationSpace
    modules[]: Spawn | Update
    renderers[]: Sprite | Mesh | ...
```

Rationale:

- A linear module stack gives stable CPU/GPU semantics and a small compiler.
- Niagara's graph is more expressive but requires a graph IR, node compilation and a much larger editor; it can be added later as a front-end lowering into the same module stack.
- Unity VFX Graph and Godot GPU particles confirm that fixed-capacity compile-time layouts are the pragmatic first step.

### Attribute and parameter model

Core attributes are a fixed enum with a compiled layout:

```cpp
enum class ParticleAttribute : uint8_t {
    Position, Velocity, Age, Lifetime, Color,
    Size, Rotation, AngularVelocity, SpriteIndex, SubUV, Seed,
    Custom0, Custom1, Custom2, Custom3,
    COUNT
};
```

- CPU uses SoA arrays per attribute.
- GPU uses one std430-compatible interleaved state buffer.
- Layout is compiled once and validated against shader declarations.
- Parameters support constant, random range, curve, gradient, user-parameter reference and attribute reference.
- GPU bakes curves/gradients into LUT textures; it does not evaluate curve objects in shaders.

### Compiled program and GPU feature mask

The compiler lives in `Particle` core and is a pure function over the effect description. `Particle.Builder` is the offline wrapper.

```cpp
struct ParticleProgram {
    uint64_t hash;
    uint32_t flags;                 // gravity/drag/noise/collision/subuv/...
    ParticleAttributeLayout layout;
    std::vector<ParticleCompiledModule> spawnModules;
    std::vector<ParticleCompiledModule> updateModules;
    std::vector<ParticleCompiledRenderer> renderers;
    uint32_t capacity;
    ParticleBounds fixedBounds;
};
```

- CPU executes typed module operations per attribute column.
- GPU selects fixed compute kernels by feature mask. Unsupported module combinations fail compilation with diagnostics.
- Renderer variant keys are generated by the builder and consumed by shader cook.
- No runtime shader compilation or reflection.

### CPU simulation

`Particle.World` is a World sub-system named `"Particle"`.

- `ParticleComponent` references an effect asset and instance parameters.
- `ParticleSystem` owns effect instances, advances emitter time, accumulates spawn rate and bursts, applies significance/LOD and enforces budgets.
- Simulation advances synchronously in the subsystem tick over chunks; moving the chunk loop to `core/async/TaskDispatcher` is a follow-up (design target, not implemented yet).
- Modules execute across an attribute column for all particles in a chunk to preserve cache locality.
- Storage is a fixed-capacity pool; per-frame render frames are immutable snapshots (one allocation per published frame), while reusable payloads such as the default white texture are cached across frames.
- Spawn uses a deterministic PCG seed and stable chunk ordering when deterministic mode is enabled.
- Death/event output goes to a bounded event queue; no per-particle callbacks.

### GPU simulation

GPU emitter state is owned by `Particle.Render`, not by the World subsystem.

Per emitter persistent resources:

```text
stateA / stateB            structured buffers, ping-pong
freeList / aliveIndices
spawnRequests / counters
indirectArgs
emitterParams
curve/gradient LUTs
sort keys/indices (later)
```

Frame passes:

```text
ResetCounters -> Spawn -> Simulate(A->B) -> Compact/BuildIndirect -> [Sort] -> Render
```

- Spawn allocates from the free list and initializes attributes.
- Simulate uses double buffering so one dispatch does not read and write the same particle.
- Compact writes alive count and indirect arguments.
- Render reads the state buffer by instance ID; no per-particle vertex upload.
- GPU never requires CPU readback for rendering.
- Per-in-flight-frame state rotation or serialized submission is required; the design does not assume the graph provides cross-frame synchronization.
- CPU emitters fall back on GPU capability failure and record a counter/one-time warning.

### Rendering integration

`Particle.Render` registers a scene extension that runs in `POST_OPAQUE` before tonemap. This uses `add-scene-render-stages`, which supplies scene color and depth directly.

- CPU path: upload per-particle instance data and issue a declarative raster draw with `PER_INSTANCE` vertex streams.
- GPU path: import persistent state buffers and issue compute passes plus an indirect raster draw using `add-framegraph-gpu-driven-passes`.
- Sprite renderer is first: billboard modes, sub-UV/flipbook, additive/alpha/premultiplied blending. `lit`/`soft` are authored fields but are rejected by the compiler until implemented, so they never silently produce an unlit/non-soft draw.
- Mesh renderer follows; ribbon, light, soft particles and shadows are later.
- User-parameter and attribute bindings are rejected by the compiler until implemented; only constant, random, curve and gradient values lower today.
- Particle shaders live under `engine/particle/adaptor/shaders` and are registered through `engine/particle/resources.json` with owner `particle`.
- Binding layout follows Global set 0 / Pass set 1 / Material set 2 / Batch set 3.

### World-to-render handoff

The World subsystem publishes immutable `ParticleRenderFrame` values keyed by `World::GetRuntimeId()` through `IParticleRenderSink`.

- The main-thread `ParticleRenderSource` owns World snapshots, view feedback and resource publication state. Stop/detach removes only that World's snapshot.
- The host supplies all active World/output pairs, including editor preview sessions, when assembling its one owned packet.
- Source encoding freezes instance data and exact texture keys into the existing `FrameExtensionPayload` bytes/resources/outputs channel. Payload byte and resource budgets remain enforced by the renderer registry.
- `ParticleRenderFeature` only consumes the frozen payload and owns shader/sampler caches. It never reads a mutable latest snapshot or asset manager during graph construction.
- Texture resolution uses `ISceneResourceSource` with explicit material usage; version caching and published/used differences retain queued versions while releasing standalone ownership.

### Review corrections to CPU ownership and authoring

- World asset binding uses request/poll states and product change serials; Tick does not wait on asset loading. Effect identity changes replace the instance, and a valid reloaded product replaces the prior program after loading completes.
- WORLD particles apply the Actor transform at spawn, including warmup; LOCAL particles remain local until snapshot extraction. Component rate and distance LOD are applied without mutating shared compiled programs.
- Expired particles are removed by their known dense index. Event draining appends into caller-owned reusable storage; Sub-UV metadata is resolved once per emitter.
- Module tokens and parameter contracts have one small schema table in core. Assets owns authoring reflection; JSON schema checks remain in assets, and the compiler validates supported value kinds before lowering.
- Binary module serialization shares local helpers, while core hashing visits all semantic module fields explicitly. The product format carries simulation space and random initialization data; prior versions have explicit migration defaults.
- Editor property and structural edits use document-owned snapshots through the existing command factory. Forms never leave undo commands pointing into replaced emitter vectors. Preview consumes the document's compiled result and updates on revision changes.
- CPU sprites remain the implemented delivery. GPU/CUSTOM simulation and unsupported parameter bindings fail with diagnostics until their owning follow-up implements them.

### Assets and cook

- Source asset `.particle` is JSON/reflected and contains the stack description.
- Cooked product contains `ParticleProgram`, renderer descriptors, variant keys and the source hash.
- `Particle.Assets` registers the asset type, traits and reflection; runtime loads the cooked program through the existing asset pipeline.
- `Particle.Builder` registers the cook step and validates assets.
- Hot reload observes product publication and installs a fully loaded valid program at the next World reconciliation.

### Editor

- Effect editor edits the system/emitter tree and module stack.
- Parameters use the reflected form framework; no per-field hand-written config forms.
- Preview uses the asset viewer's generic left preview region (`IAssetViewDocument::CreatePreview`, the same seam as the animation graph/character viewers); the document owns a preview session that runs the real preview world through `AssetPreviewProviderRegistry` + `Particle.World` and injects the current working effect via `ParticlePreviewEffectRegistry` (scoped to the preview world). Unsaved drafts therefore preview through the same simulation/render path as runtime, without saving or cooking. The content widget only hosts the stack and reflected form.
- The CPU solver overlay is only a fallback when the preview host or `Particle.World` is unavailable, or while the render preview is not ready; the editor never links runtime internals.
- The tree exposes effect/emitter/module/renderer/parameter selection; module parameters come from `ParticleModuleSchemas()` and bind to `ParticleValue` (kind/constant/vector/range). Curve/gradient key-list editing is a follow-up.
- Statistics show particle count, CPU sim time, GPU time, CPU/GPU memory and budget state.
- Debug draws show bounds, axes, velocity and emitter state.

### Determinism, budgets and LOD

- CPU deterministic mode uses fixed RNG, stable chunk order and fixed float evaluation order.
- GPU mode is explicitly not cross-vendor deterministic.
- Global particle budget plus per-effect budget; overflow drops by significance and records counters.
- Significance uses bounds, distance, screen coverage and visibility; distant emitters reduce spawn rate and update frequency.
- Mobile defaults favor unlit sprite/additive, smaller capacities and no sort/soft particles.

### Failure handling

- No exceptions or `try/catch`; all fallible entry points return `Result`/`Status`/explicit errors.
- Compiler returns diagnostics; builder rejects invalid assets without partial products.
- Missing GPU features return `Unsupported` and fall back to CPU or a non-indirect path.
- Device loss releases persistent resources in retirement and recreates lazily.
- Budget exhaustion is bounded and observable, never unbounded growth.

## Risks / Trade-offs

- **Aurora prerequisites not delivered** -> CPU sprite path lands first; GPU path is explicitly gated.
- **Scene depth/color ordering** -> depends on `add-scene-render-stages`; without it particles would render after tonemap.
- **GPU state and in-flight frames** -> per-frame state rotation or serialized submission; tested with multiple frames in flight.
- **Shader permutation growth** -> fixed renderer/sim feature sets and builder-side combination reporting.
- **Mobile bandwidth and compute limits** -> capability tiers and CPU fallback.
- **Engine subsystem exception** -> modules spec and AGENTS must record Particle explicitly; otherwise ownership is ambiguous.
- **Snapshot lifetime between DLLs** -> registered interface with double-buffered immutable snapshots; no borrowed ECS state.

## Migration Plan

1. Land `add-framegraph-gpu-driven-passes` and `add-scene-render-stages`.
2. Add build options, target skeleton, module entries and the `particle` domain registration.
3. Implement core data model, compiler and CPU solver with tests.
4. Implement assets/cook and World integration.
5. Implement CPU sprite rendering through the scene extension.
6. Implement GPU spawn/simulate/compact and indirect sprite rendering with fallback.
7. Add editor tooling and the remaining renderers.

## Open Questions

- Should GPU emitter simulation run in the World subsystem for authority reasons, or remain render-owned as designed? Render-owned is assumed because state is frame-bound.
- Do gameplay systems need deterministic GPU collision events in v1, or can events stay CPU-only?
- Should the effect editor's preview use the full scene extension path or a standalone preview feature?


### Validated CPU parameter limits

- Capacity is at most 16,777,216 particles per emitter; warmup is at most 60 seconds.
- Sub-UV dimensions are integral values in `[1, 65535]`; source and compiled-product validation share this bound before extraction.
- The current scalar `KILL_VOLUME.min/max` contract checks a Y interval at its authored position in the update stack. A three-dimensional volume is not implied.
- Assets owns authoring reflection. Render source code does not link Particle.Assets and uses the existing Aurora adaptor resource interface.
