## ADDED Requirements

### Requirement: Engine particle subsystem ownership

The particle subsystem SHALL be an engine-owned subsystem under `engine/particle` with `core`, `assets`, `adaptor`, `builder` and `editor` targets, and SHALL NOT require a `plugins/particle` implementation target.

#### Scenario: Core has no runtime or render dependency

- **WHEN** the `Particle` core target is built
- **THEN** it SHALL link only `Core` and SHALL NOT include Framework, Aurora, editor or builder headers

#### Scenario: Adaptor boundary

- **WHEN** the particle World or render adaptor is built
- **THEN** it SHALL depend on the core/assets targets and SHALL NOT be depended on by the core target

#### Scenario: Builder boundary

- **WHEN** the particle builder is built
- **THEN** it SHALL depend on the core/assets targets and SHALL NOT depend on runtime, adaptor or editor targets

#### Scenario: Editor boundary

- **WHEN** the particle editor module is built
- **THEN** it SHALL use the Sandbox editor framework and SHALL NOT be depended on by runtime or core targets

### Requirement: Effect authoring model

The subsystem SHALL model an effect as user parameters, system modules, scalability settings and one or more emitters; each emitter SHALL declare a simulation target, capacity, spawn settings, simulation space, an ordered module stack and one or more renderers.

#### Scenario: CPU and GPU emitters

- **WHEN** an effect is authored with multiple emitters
- **THEN** each emitter SHALL independently declare CPU or GPU simulation and its own capacity

#### Scenario: Ordered module stack

- **WHEN** an emitter declares spawn and update modules
- **THEN** the modules SHALL execute in the authored order with stable semantics on the CPU path and a validated equivalent on the GPU path

#### Scenario: Renderer list

- **WHEN** an emitter declares one or more renderers
- **THEN** the compiled program SHALL contain each renderer's type, parameters and required variant keys

#### Scenario: Unsupported module on GPU

- **WHEN** a GPU emitter uses a module that has no GPU implementation
- **THEN** compilation SHALL fail with a diagnostic instead of silently dropping the module

### Requirement: Compiled effect program

The subsystem SHALL compile an effect into a deterministic program containing a fixed attribute layout, module operations, GPU feature flags, renderer descriptors and a stable hash.

#### Scenario: Stable compilation

- **WHEN** the same source effect is compiled twice
- **THEN** the program hash and compiled layout SHALL be identical

#### Scenario: Layout validation

- **WHEN** a GPU renderer or compute shader declares a particle layout
- **THEN** the compiled layout SHALL be validated against the shader declaration before the effect can run

#### Scenario: Invalid effect

- **WHEN** an effect has invalid capacity, bounds, parameter types or module combinations
- **THEN** compilation SHALL return diagnostics and SHALL NOT produce a partial runnable program

### Requirement: CPU particle simulation

The CPU path SHALL simulate particles using fixed-capacity SoA storage, module evaluation across attribute columns and bounded parallel chunks.

#### Scenario: Capacity is fixed

- **WHEN** an emitter reaches its capacity
- **THEN** new spawns SHALL be rejected or recycled by policy without growing the storage

#### Scenario: Chunk parallelism

- **WHEN** a CPU emitter simulates more than one chunk
- **THEN** chunks SHALL be dispatched through the engine task dispatcher and merged deterministically

#### Scenario: No per-frame allocation

- **WHEN** a CPU emitter simulates after warmup
- **THEN** it SHALL NOT allocate per-frame storage for particle state or module evaluation

#### Scenario: Lifetime and death

- **WHEN** a particle exceeds its lifetime or enters a kill volume
- **THEN** it SHALL be removed from the active set and its slot SHALL become reusable

### Requirement: GPU particle simulation

The GPU path SHALL simulate fixed-capacity emitters using persistent device state, spawn/simulate/compact passes and GPU-produced draw arguments.

#### Scenario: Persistent state

- **WHEN** a GPU emitter has simulated a frame
- **THEN** its particle state SHALL remain in persistent device buffers for the next frame

#### Scenario: Double-buffered simulation

- **WHEN** a GPU simulate pass updates particles
- **THEN** it SHALL read from one state buffer and write to the other without read/write aliasing

#### Scenario: Spawn from free list

- **WHEN** a GPU emitter spawns particles
- **THEN** spawn SHALL allocate reusable slots from the free list and initialize their attributes on the GPU

#### Scenario: Compaction and indirect arguments

- **WHEN** simulation completes
- **THEN** a compaction stage SHALL produce the alive count and indirect draw arguments for rendering

#### Scenario: No CPU readback for rendering

- **WHEN** a GPU emitter renders
- **THEN** particle state and draw counts SHALL be consumed on the GPU without a CPU readback

#### Scenario: Capability fallback

- **WHEN** required GPU features are unavailable
- **THEN** the emitter SHALL fall back to CPU simulation or an explicitly supported non-indirect path and SHALL report the fallback

### Requirement: Particle rendering integration

The render adaptor SHALL render particles in a scene extension stage after opaque geometry and before tonemap, sharing scene color and depth.

#### Scenario: Correct pass position

- **WHEN** a particle effect is visible
- **THEN** its transparent work SHALL execute after the scene opaque pass and before tonemap

#### Scenario: Depth sharing

- **WHEN** a particle renderer needs the scene depth
- **THEN** it SHALL use the scene depth provided by the scene extension stage

#### Scenario: CPU instanced rendering

- **WHEN** a CPU emitter renders sprites
- **THEN** the adaptor SHALL upload per-particle instance data and issue an instanced draw

#### Scenario: GPU indirect rendering

- **WHEN** a GPU emitter renders sprites
- **THEN** the adaptor SHALL read particle state from imported persistent buffers and issue an indirect draw

#### Scenario: Missing renderer prerequisite

- **WHEN** the required graph or scene extension capability is unavailable
- **THEN** the adaptor SHALL report an explicit error and SHALL NOT render incorrect output

### Requirement: Sprite renderer

The sprite renderer SHALL support billboard modes, sub-UV/flipbook animation, per-particle color/size/rotation and additive, alpha and premultiplied blend modes.

#### Scenario: Billboard orientation

- **WHEN** a sprite emitter uses camera-facing billboards
- **THEN** each sprite SHALL face the current view camera

#### Scenario: Flipbook

- **WHEN** a sprite emitter uses sub-UV animation
- **THEN** the renderer SHALL select the sub-UV frame from particle age or a compiled parameter

#### Scenario: Blend modes

- **WHEN** a sprite emitter selects a blend mode
- **THEN** the renderer SHALL use the corresponding blend state for that emitter's draws

### Requirement: World integration and snapshots

The World subsystem SHALL own effect instances, advance emitter state, apply budgets/LOD and publish immutable render snapshots without exposing live ECS state to the renderer.

#### Scenario: Component lifecycle

- **WHEN** a particle component is created, destroyed or has its effect changed
- **THEN** the World subsystem SHALL create, retire or replace the corresponding effect instance

#### Scenario: Snapshot handoff

- **WHEN** a frame is rendered
- **THEN** the renderer SHALL consume an immutable particle snapshot and SHALL NOT read mutable simulation state

#### Scenario: Renderer not attached

- **WHEN** no particle render sink is registered
- **THEN** the World subsystem SHALL still support CPU simulation or report the missing sink without crashing

#### Scenario: Nonblocking effect replacement

- **WHEN** a component changes effect or a new product version is published
- **THEN** the World SHALL request the product without blocking its tick and replace complete solver state only after successful validation
- **AND** unrelated publications with unchanged program hashes SHALL preserve the current simulation

#### Scenario: Simulation-space transforms

- **WHEN** an actor transform changes
- **THEN** WORLD particles SHALL retain their spawn transform, including warmup particles
- **AND** LOCAL particles SHALL use the current actor transform during snapshot extraction

#### Scenario: World-scoped frozen packet

- **WHEN** multiple World instances are rendered or a newer snapshot or texture revision is published
- **THEN** each accepted packet SHALL retain its own instances, output routing and exact texture resource keys
- **AND** stopping or detaching one World SHALL withdraw only that World's snapshots

### Requirement: Effect asset and cook

The subsystem SHALL provide a reflected `.particle` source asset, a cooked product containing the compiled program and variants, and runtime loading through the asset pipeline.

#### Scenario: Source roundtrip

- **WHEN** a `.particle` source asset is saved and loaded
- **THEN** its system, emitter, module, parameter and renderer data SHALL roundtrip without loss

#### Scenario: Cook output

- **WHEN** a `.particle` asset is cooked
- **THEN** the product SHALL contain the compiled program, renderer descriptors, variant keys and source hash

#### Scenario: Invalid source

- **WHEN** a `.particle` asset fails validation
- **THEN** cook SHALL fail with diagnostics and SHALL NOT emit a partial product

### Requirement: Effect editor

The editor module SHALL provide effect authoring, preview and diagnostics using the shared reflection form framework and editor chrome.

#### Scenario: Module stack editing

- **WHEN** a user adds, removes or reorders modules in the effect editor
- **THEN** the editor SHALL update the source asset through undoable commands

#### Scenario: Parameter editing

- **WHEN** a user edits a parameter, curve or gradient
- **THEN** the editor SHALL use the reflected form framework rather than a per-field hand-written form

#### Scenario: Preview and statistics

- **WHEN** an effect is being edited
- **THEN** the editor SHALL provide a preview and report particle count, simulation time and budget state

### Requirement: Budgets, significance and LOD

The subsystem SHALL enforce global and per-effect particle budgets and SHALL reduce or cull emitters by significance without unbounded growth.

#### Scenario: Global budget

- **WHEN** active particle demand exceeds the global budget
- **THEN** the subsystem SHALL reduce or drop the least significant emitters and SHALL record the event

#### Scenario: Distance LOD

- **WHEN** an emitter is far from the view
- **THEN** its spawn rate, update frequency or capacity SHALL be reduced according to the configured LOD

#### Scenario: Culled emitter

- **WHEN** an emitter is outside the view or below the significance threshold
- **THEN** it SHALL not submit render work while retaining valid state according to its policy

### Requirement: Determinism and failure behavior

CPU deterministic mode SHALL be reproducible for the same seed and update sequence; GPU mode SHALL explicitly document its lack of cross-vendor determinism. All fallible entry points SHALL return explicit errors without exceptions.

#### Scenario: CPU replay

- **WHEN** the same effect, seed and update sequence are simulated twice in deterministic mode
- **THEN** the resulting particle state SHALL be identical

#### Scenario: GPU determinism disclosure

- **WHEN** an effect uses GPU simulation
- **THEN** the system SHALL NOT claim cross-vendor deterministic results

#### Scenario: Explicit failure

- **WHEN** an invalid descriptor, asset or GPU capability is encountered
- **THEN** the subsystem SHALL return an explicit error or fallback and SHALL NOT throw


#### Scenario: Stable structural undo

- **WHEN** a reflected property edit is followed by emitter or module insertion, removal or reordering
- **THEN** undo and redo SHALL apply through document-owned commands without retaining pointers into reallocated vectors or clearing prior history
- **AND** invalid draft data SHALL remain editable without overwriting the source on save
