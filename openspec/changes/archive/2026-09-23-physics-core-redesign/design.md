## Context

`engine/physics` today mixes layers: it exposes shapes/queries/material interfaces (good) but also ships components, a factory whose `Impl` owns backend objects, raw-pointer ownership between components and backend objects, a variable-delta `Tick`, and an incomplete, untyped query/serialization surface. `plugins/bullet` implements the backend, but the runtime components live in the engine, so a second backend would still pull engine-side implementation along.

The repository has since converged on a layering rule (see `openspec/specs/engine-plugin-layering/spec.md`) proven by `engine/terrain` + `plugins/terrain` and `engine/navigation` + `plugins/recast`: `engine/<feature>` holds interfaces, plain data, and pure utilities; the feature is implemented in a plugin that registers the concrete interfaces. Physics must follow the same shape.

Constraints from the environment: 32-bit `float` engine positions; aurora is the RHI and the legacy render layer must not be reintroduced; runtime targets include PC, mobile (Android/iOS), and consoles; editor is the aurora sandbox (non-Qt) plus optional Qt paths; modules are selected at compile time (`plugins/plugins.json`) and at runtime (`configs/modules_game.json`, `configs/modules_editor.json`); `SKY_USE_TRACY` and `SKY_MATH_SIMD` exist as build switches; no production physics scene data is expected, so a data-format break is acceptable.

Stakeholders: gameplay/character code, terrain collision, navigation (consumes collision meshes), editor tooling, and a future networking/rollback path.

## Goals / Non-Goals

**Goals:**

- Make `engine/physics` interface/data/utility-only, with all implementation in plugins and a swappable backend contract (Bullet first, others addable).
- Provide a stable `PhysicsObjectId` handle model with world-owned object lifetime, eliminating component↔backend raw-pointer coupling.
- Make simulation fixed-step and decoupled from frame delta and wall clock, with transform interpolation.
- Expose `PhysicsMathMode { Fast, Exact }` and backend capability reporting; deliver `Fast` here and reserve `Exact` for a future backend.
- Define a job-friendly step seam with a single-threaded fallback.
- Provide snapshot/restore over stable ids for editor replay and rollback-by-restore.
- Define engine-side constraints, character controller, CCD, and contact/trigger event contracts and implement them in the first version.
- Correct queries (typed handles for all object kinds, skip-and-continue filtering, deterministic ordering) and complete serialization.
- Keep debug output render-free and give the editor a simulation-control contract.
- Keep the design mobile-first (sleep, budgets, heightfield over tri-mesh, opt-in CCD).

**Non-Goals:**

- Implementing or delivering cross-platform bit-exact determinism in this change. `Exact` is interface-only here; the implementation is a separate change built on a third-party deterministic backend.
- Modifying Bullet or writing an in-house deterministic/fixed-point solver.
- A second backend implementation (Jolt/PhysX) and cloth/ragdoll/vehicle/destruction. Only their seams are in scope.
- Reimplementing the render layer; the collision-mesh bridge stays a backend render submodule.
- Editor UI implementation beyond the simulation-control and debug-geometry contract.

## Decisions

### D1: Module boundary and plugin layout

**Decision**: `engine/physics` keeps only headers of interfaces, plain-data descriptors, and pure utilities (`PhysicsObjectId`, shape/body/material/filter/constraint/character/event/snapshot/step descriptors, `IPhysicsWorld`, `IPhysicsBackend`, `PhysicsBackendRegistry`, the shared `PHYSICS_SYSTEM_NAME`, `PhysicsDebugGeometry`, the mesh seam, `PhysicsMathMode`, and a pure `PhysicsStepper`). There is **one physics implementation plugin per backend**, and each backend plugin registers its **own world sub-system** under the engine-provided `PHYSICS_SYSTEM_NAME`, together with its components and serialization. The first is `plugins/bullet` (unmodified Bullet: `BulletBackend`, the `PhysicsSystem` sub-system, `PhysicsBodyComponent`, plus the `plugins/bullet/render` bridge). A future `plugins/physx` / `plugins/jolt` is a peer plugin registering the same sub-system name; there is **no separate backend-agnostic runtime plugin**.

**Rationale**: Matches the repository's one-plugin-per-feature pattern (`plugins/terrain`, `plugins/recast`) and removes the extra indirection of a thin runtime plugin. Pluggability comes from the engine-level `IPhysicsBackend` plus the shared `PHYSICS_SYSTEM_NAME`: adding or swapping a backend means adding a peer plugin that registers the same sub-system name, while consumers keep resolving physics through the engine interface. Components live with the backend that provides them.

**Alternatives considered**: Keep components in `engine/physics` (violates the layering spec and keeps runtime coupled to the backend). Merge runtime + backend into `plugins/bullet` (a second backend would then duplicate or lose the runtime). Put the world sub-system in `engine/physics` (implementation in engine; rejected).

### D2: Handle-based ownership

**Decision**: A world owns its physics objects. `AddBody`/`AddConstraint`/`AddCharacter` return a `PhysicsObjectId` (a `{ uint64_t index, uint32_t generation }`-style stable id scoped to a world). Consumers hold ids, never backend pointers. The engine world (the backend plugin's sub-system) exposes read/write accessors and a `Destroy(id)`. Ids are assigned in deterministic insertion order and are not silently reused within a world's lifetime (generation guards ABA). Snapshot restore preserves ids.

**Rationale**: Removes the current use-after-free class (component holds a pointer the world deleted on removal/detach), makes snapshot/rollback addressable, and gives the editor and gameplay a serializable reference. It also keeps the door open for the future `Exact` mode's stable-ordering requirement.

**Alternatives considered**: Keep raw pointers + strict detach discipline (the current bug source). `shared_ptr` per object (lifetime still ambiguous; heavier; not serializable). Slotmap without generation (ABA on removal).

### D3: Backend contract with capabilities

**Decision**: Replace `PhysicsRegistry::Impl` with `IPhysicsBackend`: a `PhysicsBackendCaps` descriptor (active `PhysicsMathMode` and `deterministic`, `jobStepping`, `supportsCCD`, `supportsConstraints`, `supportsCharacters`, `supportedShapes`), `Init`/`Shutdown`, `CreateWorld`/`DestroyWorld`, `CreateMaterial`, and shape creation that cooks inside the backend. `PhysicsRegistry` becomes a pure factory registry (register one active backend, query capabilities, create worlds). Backends self-register from their module `Start()`; the backend plugin's sub-system resolves a world through the registry and attaches itself to the `sky::World` under `PHYSICS_SYSTEM_NAME`. Hosts attach physics through `PhysicsBackendRegistry::SetWorldAttacher`/`AttachToWorld` (registered by the backend plugin), so launcher/editor stay backend-agnostic and link no plugin. The shipped backend reports `Fast` / `deterministic = false`; requesting `Exact` is rejected until a deterministic backend is registered.

**Rationale**: Capabilities let the engine adapt (skip job stepping, disable CCD, reject `Exact`) instead of assuming features; keeping the registry in engine is interface/data-level and keeps the decision "which backend" out of runtime and game code.

**Alternatives considered**: Feature-detect by probing calls (implicit, harder to reason about). A separate backend-agnostic runtime plugin (extra indirection with only one backend; rejected). Multiple simultaneous backends per world (unneeded complexity).

### D4: Fixed-step simulation; math mode reserved

**Decision**: The engine owns a pure `PhysicsStepper` implementing a fixed-timestep accumulator with a `maxSubSteps` cap and an integer frame counter; it calls the backend `Step(fixedDelta)` zero or more times per rendered frame. Body reads expose both the current (simulated) transform and an interpolated transform for rendering. No engine or backend step path may read wall-clock time or the frame delta; inputs are forces, transforms, and commands issued between steps. `PhysicsMathMode { Fast, Exact }` is exposed: this change delivers `Fast`; `Exact` (cross-platform bit-exact) is **reserved** and is rejected while unavailable.

**Deferred: Exact determinism (reserved)**: Cross-platform bit-exact simulation is required for rollback/lockstep but is out of scope for this change. It is planned on a third-party backend — **Jolt Physics** with its `CROSS_PLATFORM_DETERMINISTIC` CMake option (MIT, C++17, no RTTI/exceptions) — which the Jolt documentation states makes the simulation deterministic regardless of compiler, build configuration, OS, architecture, and word size (approx. +8% cost), provided all platforms use the same source and defines. Remaining engine-side obligations for that future change (recorded here so the current interfaces do not block them):

1. Deterministic input ordering and stable ids (D2 already provides ordering).
2. Fixed step with no wall-clock/frame-delta inputs (D4 provides this).
3. Sorting/re-validating Jolt's non-deterministic results (broadphase queries, listener callback order, `GetActiveBodies`) and using a narrowphase/custom-collector path for deterministic queries.
4. A cross-platform determinism trace test.
5. Console targets are not in Jolt's tested matrix, so console determinism needs separate validation.

Host-`float` transforms remain the engine-facing boundary.

**Rationale**: Fixed-step + stable ordering + a backend-capability seam deliver immediate value and cost nothing that a later deterministic backend cannot build on, while avoiding a large fixed-point rewrite now. Jolt is the pragmatic route to `Exact` without modifying Bullet or writing a solver.

**Alternatives considered**: Implement `Exact` now with fixed-point (large rewrite; rejected). Modify Bullet or write a deterministic solver (rejected by direction). Always-on determinism (penalizes single-player/editor). Never deterministic (loses future rollback/lockstep).

### D5: Job-friendly step seam with single-threaded fallback

**Decision**: The step is expressed as ordered phases (integrate, broadphase, narrowphase, solve, commit/events) with a declared read/write contract, and the backend advertises whether it can run them on the engine job system. The engine exposes a job-dispatch seam; when `jobStepping` is false or the job system is unavailable, stepping runs single-threaded. Where phases run in parallel, accumulation/reduction order SHALL be ordered by stable id (not completion order). Query access from worker threads is documented as read-only during the commit phase and otherwise forbidden.

**Rationale**: Gives a forward path to parallelism and preserves determinism for the future `Exact` backend without forcing either on the current `Fast` backend.

**Alternatives considered**: Design around a specific backend's threading. Require jobs unconditionally (breaks Bullet/mobile). Defer threading entirely (loses the architecture seam).

### D6: Snapshot/restore for editor replay and rollback

**Decision**: `IPhysicsWorld` can `CaptureState(PhysicsWorldState&)` and `RestoreState(const PhysicsWorldState&)` over stable ids. The state holds a frame/step counter and per-body kinematic state (transform, linear/angular velocity, sleep flag), plus optional per-constraint state; defaults capture dynamic/kinematic bodies only, with a "full" option including static/sleep state. The engine buffers snapshots for editor replay. Rollback is **snapshot restore**; re-simulation from a confirmed snapshot is reserved for the future `Exact` mode.

**Rationale**: Addressable rollback over ids supports editor single-step/rewind and a future network rollback path; without a deterministic backend, restore (not re-simulation) is the correct mechanism. Snapshot state is backend-neutral and addressable.

**Alternatives considered**: Re-simulate from input logs (needs `Exact`; deferred). Serialize the whole backend world blob (opaque, hard to diff, backend-specific).

### D7: Queries

**Decision**: `Raycast`, `Sweep`, and `Overlap` return `PhysicsQueryHit`/`PhysicsQueryOverlap` carrying a `PhysicsObjectId` (resolvable for every object kind) and backend-neutral geometry, ordered deterministically (by hit distance, then id). A `PhysicsQueryFilter` (`mask`, optional ignore id, optional layer matrix) causes rejected candidates to be skipped and the search to continue to the next acceptable candidate. No backend type appears in the result.

**Rationale**: Fixes the "null object for rigid bodies" and "reject-closest returns no hit" defects and makes queries usable by gameplay, AI, and interaction systems. Deterministic ordering also supports the future `Exact` mode.

**Alternatives considered**: Return raw backend objects (couples consumers). Keep "closest only, no skip" (incorrect filtering semantics).

### D7b: Navigation consumes collision geometry through an engine seam

**Decision**: Hosts and other subsystems reach physics through a single engine interface `phy::IPhysicsSystem : public IWorldSubSystem` (in `engine/physics`). It exposes `GetWorld()`, simulation control (`SetSimulating`/`IsSimulating`/`SingleStep`), and `GatherCollisionMeshes(std::vector<phy::CollisionMeshInstance>&)` (local `TriangleMesh` + world `Transform`). The physics runtime (`PhysicsSystem`) implements it and registers under `PHYSICS_SYSTEM_NAME`; consumers resolve the sub-system by name and down-cast. Providers populate collision mesh data via the render-agnostic `PhysicsMesh` seam (`CreatePhysicsMesh(Uuid)`), which the render bridge (`plugins/bullet/render`, linking Aurora) implements.

**Rationale**: One engine interface (rather than several) avoids a multiple-inheritance diamond on `IWorldSubSystem` and lets both navigation (collision geometry) and the editor (simulation control, `GetWorld()` for debug geometry/snapshots) stay free of backend and render types while keeping the down-cast static (no RTTI/`dynamic_cast`). Aurora stays confined to the render bridge.

**Alternatives considered**: several interfaces each deriving `IWorldSubSystem` (diamond across the concrete sub-system). recast reading the physics backend component (plugin→plugin coupling). recast linking Aurora and reading render assets directly (render coupling in navigation). A non-subsystem interface requiring `dynamic_cast` (RTTI, prohibited).

### D8: Material, filter, events, constraints, character, CCD

**Decision**: Material data gains friction/restitution combine modes and (optional) density, with per-body overrides. Filtering is a documented group/mask contract plus named interaction layers registered engine-side; the backend plugin maps it to backend group/mask. Contacts/triggers are emitted as an ordered event stream drained after the step. Constraints (fixed, hinge, slider, distance, generic 6-DOF), continuous collision detection, and the capsule character controller are engine-side descriptors/interfaces **implemented by the backend in the first version**.

**Rationale**: These are the gameplay-facing contracts the current implementation lacks; defining them in the engine keeps consumers backend-agnostic, and they are all backend features Bullet already provides.

**Alternatives considered**: Expose backend-native joints/characters (couples consumers). Per-object callbacks (harder to order/snapshot). Defer events/constraints to a later change (blocks gameplay and character work).

### D9: Serialization and editor contract

**Decision**: Physics descriptors have stable, documented field names and versioning; reflection registration and asset/component round-trip live in the backend plugin (implementation), while the descriptor structs live in the engine. The old typo'd/partial shape serialization is dropped (no production data). The editor gets play/pause/single-step/replay controls over the stepper + snapshots, and debug geometry is requested per category (`Shapes`, `Contacts`, `AABBs`, `Constraints`, `Character`) and consumed by the editor/render layer; the physics core and debug geometry remain render-free.

**Rationale**: Keeps serialization implementation in the plugin per the layering spec while giving the editor a clean, gated contract.

**Alternatives considered**: Serialize in engine (implementation in engine; rejected). Keep the current behavior-flag debug flag for all categories (no editor filtering).

### D10: Mobile-first budgets

**Decision**: Defaults favor mobile, exposed as runtime-configurable `PhysicsOptions` rather than hard validation: sleeping enabled, heightfield collision for terrain (never a triangle mesh for open worlds), dynamic triangle-mesh collision off by default but switchable (`allowDynamicTriangleMesh`), a body/active-body budget (`maxBodies`, `maxActiveBodies`) with stats (`PhysicsWorldStats`), CCD gated by a global option plus per-body opt-in, and `SKY_PROFILE`-instrumented step phases. Spatial structure and shape count are backend concerns, but the engine exposes stats, options, and caps so projects/targets can tune per world.

**Rationale**: The engine targets Android/iOS/consoles; these defaults prevent the common open-world/mobile pitfalls without forcing a solver design.

**Alternatives considered**: Tri-mesh terrain (memory/CPU heavy). Always-on CCD (cost). No stats (no way to budget).

## Risks / Trade-offs

- **[`Exact` deferred, networking depends on it]** → The capability seam and stable ordering are in place now; the future change adds a Jolt backend + trace test without changing consumers. Until then, rollback is restore-only and lockstep is unavailable.
- **[Console determinism is unproven]** → Jolt's tested matrix excludes consoles; treat console `Exact` as a separate validation task in the future change.
- **[Large migration touches terrain, navigation, character, editor]** → Stage it: land the contract + Bullet backend behind the existing component names, then migrate consumers, then delete the old engine implementation. Keep compile green at each stage.
- **[Snapshot memory/CPU cost at scale]** → Default to dynamic/kinematic bodies only, fixed-size state records, delta snapshots as a follow-up; expose snapshot size in stats.
- **[Components live in the backend plugin]** → Accepted for the single-backend case; if a second backend ships, shared components are factored out then. The engine interface and shared `PHYSICS_SYSTEM_NAME` keep consumers backend-agnostic meanwhile.
- **[Component/serialization break]** → No production physics data expected; document the new format and re-author scenes.

## Migration Plan

1. **Contract**: Add the new `engine/physics` headers (ids, descriptors, `IPhysicsWorld`, `IPhysicsBackend`, `PhysicsBackendRegistry`, `PHYSICS_SYSTEM_NAME`, stepper, snapshot, events, constraints, character, query, filter, material, debug categories, `PhysicsMathMode`). Keep old headers temporarily.
2. **Backend plugin**: Implement `plugins/bullet` (unmodified Bullet) against `IPhysicsBackend` (world-owned objects, capabilities, fixed `Step`, constraints, character, CCD, snapshot, typed queries, events, material, shape cooking) and register through the new registry. Port the render bridge unchanged in role.
3. **Runtime in the backend plugin**: place the world sub-system wrapper (`PhysicsSystem`), body/character/constraint components, serialization registration, CCD, and mobile budgets inside `plugins/bullet`; the module registers the sub-system under `PHYSICS_SYSTEM_NAME`, drives stepping via `PhysicsStepper`, and exposes interpolated transforms.
4. **Migrate consumers**: terrain collision layer, navigation collision-mesh consumption, character/animation paths onto `PhysicsObjectId` + descriptors + typed queries/events.
5. **Editor**: simulation controls (play/pause/step/replay) and per-category debug geometry exposed via `phy::IPhysicsSystem`; wired when the Sandbox editor (launched through `Launcher`) is ready. The legacy Qt `engine/editor` is retired and is not a dependency.
6. **Remove old implementation**: delete the engine-side components/factory implementation and the old shape/serialization surface; update `plugins/plugins.json`, `configs/modules_game.json`, `configs/modules_editor.json`, and CMake.
7. **Rollback strategy**: because this is a structural refactor, rollback is via version control and by keeping the old backend registered until step 6; the new format has no data migration to reverse.
8. **Future `Exact` change (not this one)**: add `plugins/jolt` with `CROSS_PLATFORM_DETERMINISTIC`, same source/defines across all platforms, sort/re-validate non-deterministic Jolt outputs, and add the cross-platform determinism trace test.

## Open Questions

- Does editor replay need full rewind/scrub (buffered snapshots) now, or just play/pause/single-step?
- Who owns the previous-frame transform for interpolation: the engine-side object record or the render feature processor?
- Should filters be extended from bitmask layers to a callback/filter-shader contract in the engine surface, or stay mask-based with backend-specific callbacks?
- For the future `Exact` change: does `Exact` need to run alongside `Fast` in one process (both backends compiled), or is it a per-build/per-target choice?
