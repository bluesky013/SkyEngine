## Context

`engine/animation` is the runtime skeletal-animation module. It links only `Core`, so it is renderer-independent by construction: the core owns instances, the node graph, poses, clips, playback, and the state machine, and never references a renderer. Today the render/consumer side lives in the legacy `SkyRender` module (`engine/render/adaptor` and `engine/render/builder`: `AnimationPreviewComponent`, `CharacterLocomotion`, `SkeletonDebugRender`, `SkeletalMeshComponent`, `.clip`/`.graph` assets). The target is to move that side to `aurora` (`Aurora.Adaptor`, which links `Aurora` + `Framework` + `Core` and not `SkyRender`); the migration is **deferred** and no new `SkyRender` code is added here. This change is deliberately confined to the `Core`-only core.

There is no top-level `animation` spec today; behavior is defined implicitly by the code. This change bootstraps that main spec and implements its first increment - a correctness pass over the existing evaluation path.

### Current architecture (as-is)

- **Instance/parameters**: `Animation` (`engine/animation/src/core/Animation.cpp`) owns the node graph, named parameters, and an `AnimationAsyncContext`. `SkeletonAnimation` specializes it with a `Skeleton`.
- **Two-phase frame model**: `Animation::Tick` updates parameters, runs node `PreTick`, then node `TickAny`; `EvalAny` separately samples the graph into an `AnimPose`. Update and evaluation are separate so sampling can run independently of time advance.
- **Node graph**: `AnimNode` defines `PreTick` / `InitAny` / `TickAny` / `EvalAny`. Contexts are `AnimContext` (instance), `AnimLayerContext` (weighted layer), and `AnimationEval` (pose + skeleton). Nodes are owned by `Animation` via `NewAnimNode<T>`.
- **Data model**: `Skeleton` holds a bone tree and a shared reference pose; `AnimPose` is a flat `std::vector<Transform>` indexed by `BoneIndex` plus an `AnimationBoneMask` and a weak `Skeleton*`. Clip channels and parameters are keyed by interned `Name`.
- **Clip/playback**: `AnimationClip` owns `AnimationChannel`s keyed by bone name and samples keyframes; `AnimationSequencePlayer` advances time with play rate and loop/clamp behavior. `AnimationClipNode` binds a clip, loop flag, and root-motion flag into the graph.
- **State machine**: `AnimStateMachine` holds states, conditions, and transitions, evaluating conditions during `TickAny` and evaluating the active state's node.
- **Assets/integration (deferred, not part of this change)**: `AnimationClipAssetData` (`.clip`, binary), `SkeletonAssetData` (`.skel`, binary), and `AnimationAssetData` (`.graph`, JSON) currently live in the `SkyRender` adaptor layer (`engine/render/adaptor`, loaded/saved by `AnimationBuilder`). They will be relocated to the aurora integration layer; the core must not depend on them.
- **Threading seam**: `AnimationAsyncContext` and `UseAsync()` exist, but `UseAsync()` returns `false` and ticking is synchronous.

## Goals / Non-Goals

**Goals:**
- Define `animation` as the main capability spec for the runtime skeletal animation system.
- Land the first increment: correct blending, channel sampling (including scale), non-destructive root motion, bounds-safe state selection, and key-time-derived duration - all inside `engine/animation`.
- Keep the module's renderer independence (depends only on `Core`) and existing public surface intact.
- Cover the fixes with unit tests.

**Non-Goals:**
- Implementing `AnimationEvent`, `MotionDatabase`/`MotionMatcher`, `AnimationController`.
- `CUBIC_SPLINE` interpolation.
- State-machine cross-fade / transition blending.
- Changing the on-disk asset formats or re-cooking assets.
- Applying sampled root motion to actors (only record the consumed delta).
- Enabling the async evaluation path.
- Renderer integration: no new `SkyRender`/`RenderAdaptor` work, and no aurora integration code. The migration from `SkyRender` to `aurora` (asset relocation, preview/locomotion/skeletal-mesh/debug-render components) is deferred.
- Restructuring the node graph into a data-oriented evaluation plan (target architecture recorded in D9; implemented by the follow-up change `animation-data-oriented-graph`).

## Decisions

### D0: Bootstrap `animation` as the main spec
Write the full core capability description (renderer independence, instance/parameters, graph/layers, skeleton/pose, clip sampling, playback, blending, state machine, root motion) as the initial `openspec/specs/animation/spec.md`, with the first-increment correctness requirements included. Renderer/asset integration and deferred subsystems are left unspecified rather than specified as fiction.
- *Alternative considered*: keep the narrow `animation-core` capability. Rejected - the user wants a single main spec to evolve against.

### D1: Correct `AnimPose` blend math
`AnimPose::BlendTransform(src, dst, weight)` currently ignores `src` and multiplies `dst` (including its quaternion) by `weight`. Redefine:
- Override: lerp translation/scale `dst → src`, and `dst.rotation = AnimSphericalLinear(dst.rotation, src.rotation, weight)`.
- Additive: accumulate weighted translation/scale deltas, and apply a weighted rotation delta via `dst.rotation = dst.rotation * AnimSphericalLinear(identity, delta.rotation, weight)`.

Keep signatures unchanged. `AnimSphericalLinear` already handles shortest-path and normalization.
- *Alternative considered*: scalar-scaled quaternions (current). Rejected - not a unit rotation and produces wrong arcs.

### D2: Repair the pose-blend nodes
- `PoseBlend2Node`: evaluate A and B into scratch `AnimationEval` copies and produce `lerp(A, B, alpha)` via an override blend onto a reference-reset destination. Add `SetBlend(bool)` / `SetBlendTime(float)` (blending is currently never activated), and fix `AnimFadeInOut(float time)` to actually set the fade times (today they are forced to 0).
- `PoseBlendNodeList`: add `AddPose(node, weight)`, populate `poses`/`desiredWeights`, and track source indices instead of indexing a compacted scratch vector with the full-range loop index (current out-of-range read).
- Remove the call to the pure-virtual `AnimNode::InitAny` (undefined reference trap).

### D3: Conditional TRS channel sampling
`AnimationNodeChannel::Sample` assigns position/rotation/scale only when the component has keys; absent components keep the incoming reference-pose value. This re-enables the commented-out scale assignment and avoids the non-empty assertion on component-sparse clips.
- *Alternative considered*: require uniform clips. Rejected - imported clips are not guaranteed uniform and the loader allocates components independently.

### D4: Non-destructive root motion
`AnimationClipNode::EvalAny` stops clearing bit 0 of the eval pose bone mask. When root motion is disabled it samples normally and resets the root bone transform to the reference pose, resolving the root from `Skeleton::GetRoots()` instead of index `0`. The consumed delta is recorded on `AnimationEval`.
- *Alternative considered*: save/restore the mask bit. Rejected - skipping the root channel also drops root rotation and still mutates shared state.

### D5: Bounds-safe state machine
Clamp state handles to `[0, states.size() - 1]` (currently `states.size()`), validate state/condition handles in `AddTransition` / `Transition` / `FindTransition`, and remove or use the unused `visited` parameter. Transitions stay instant (`ANIM_MAX_TRANSITION_PER_FRAME` used or removed).
- *Alternative considered*: add cross-fade here. Deferred - it is a feature, not a correctness fix.

### D6: Duration derived in the core
`AnimationClip` derives its duration from the key times of its channels (`maxKeyTime + 1` over all components divided by frame rate) instead of relying on a frame count assigned by the asset loader. This keeps the fix inside `engine/animation` with no renderer or asset dependency, so any adaptor (current or aurora) gets correct playback timing for free.
- *Alternative considered*: fix `CreateAnimationClipFromAsset` in the `SkyRender` adaptor. Rejected - it is the wrong layer and would couple a core correctness fix to a renderer being migrated away.
- *Alternative considered*: add `numFrame` to `AnimationClipAssetData` + version bump. Rejected for now to avoid re-cooking; revisit if a clip needs trailing hold time.

### D7: Renderer-independent core, integration deferred
Do not add to or fix the `SkyRender` animation adaptor. The animation rendering/consumer side is retargeted to `aurora` (`Aurora.Adaptor`) and its wiring - asset relocation, `AnimationPreviewComponent`, `CharacterLocomotion`, `SkeletalMeshComponent`, `SkeletonDebugRender` - is deferred to a future integration increment. The known `SkyRender`-side defects (`AnimationPreviewComponent` wrong-case include, missing skeleton-asset null check) are therefore left to that migration rather than patched in place. The core contract only needs to expose the evaluated final pose in a renderer-neutral form; how a renderer consumes it is the integration's concern.
- *Alternative considered*: patch the `SkyRender` preview component now. Rejected - it is being replaced by the aurora integration; fixing it would be throwaway work.

### D8: Tests
Extend `engine/animation/test` with blend, channel/scale, root-motion-mask, state-bounds, and duration cases.

### D9: Target architecture - data-oriented evaluation plan (recorded, not implemented here)
The current graph is pointer-and-virtual based and does not lend itself to GPU evaluation; the graph asset is still a stub, so the structure can be reshaped cheaply before it is serialized. Record the target for the main spec and implement it in the follow-up change `animation-data-oriented-graph`:

- **Control plane (CPU)**: state-machine transitions, condition evaluation, parameters, clip time advance, root-motion decisions, events. Inherently dynamic branching.
- **Data plane (CPU now, GPU later)**: flat `AnimOpRecord` array (op tag, input/output pose slots, weight, data index) evaluated in topological order as `for (op) switch (op.op)` over a preallocated pose pool (`numSlots x numBones` contiguous `Transform`s). No virtual dispatch, no per-frame allocation.
- Layer weight becomes an op field instead of a recursively propagated `AnimLayerContext`.
- Clip keys move to SoA buffers so the same data can be uploaded.
- Graph data (op list, channels, skeleton, masks) becomes pointer-free and index-based, serializable to an SSBO; a compute pass produces the `aurora::Skin` bone-matrix palette.

Rationale: the current code allocates every frame and hash-looks-up every bone (`PoseBlendNode`, `AnimationClip::SamplePose`, `AnimStateMachine::TickAny`), which is a CPU cost problem now and a GPU-portability blocker later. Doing CPU-first keeps the risk bounded; GPU-driven evaluation is a further increment, not a prerequisite.
- *Alternative considered*: keep the OOP node graph and bolt on a GPU path. Rejected - two divergent evaluation implementations and no cache-friendly CPU path.

## Risks / Trade-offs

- [Blend behavior changes] → Blend nodes are currently unused by consumers; blast radius is the new unit tests, and any consumer effect is checked when the aurora integration lands.
- [Root motion now preserves root rotation] → Intended; verify against a clip with root rotation.
- [Core-only duration derivation can be short without a trailing key] → Compression preserves the last key; the asset-format alternative in D6 covers pathological clips later.
- [Main spec mixes shipped and fixed behavior] → Deferred subsystems are deliberately unspecified; each future increment adds its own delta rather than pre-declaring behavior.
- [Latent pure-virtual call hidden by static linking] → Removing it in D2 closes a trap that only surfaces when the node is referenced.
- [Data-oriented reshape (D9) is deferred] → Recording it in the main spec prevents the interim OOP structure from being frozen into the `.graph` asset format; the follow-up change must land before graph serialization is authored.
- [SkyRender animation adaptor is left unfixed] → Accepted; it is superseded by the deferred aurora integration, so no effort is spent on it. The core is unaffected.

## Migration Plan

Source-only change confined to `engine/animation` and its tests; no asset or data migration. Build the `Animation` target and run its tests. The renderer integration migration (SkyRender to aurora) and asset relocation are separate, deferred work. Rollback is a revert of the change commit.

## Open Questions

- Should the recorded root-motion delta be applied to actor transforms in the next increment, or remain consumers' responsibility?
- Should the next increment be state-machine cross-fade, animation events, or motion matching?
- When should the async evaluation path (`UseAsync`) be activated, and does it change the pose ownership rules in the spec?
- Does the data-oriented evaluation plan (D9) land before the aurora integration, so graph serialization/cooking is authored against the op plan rather than the interim OOP structure?
- Where exactly should the relocated clip/skeleton/graph asset data and builders live under `Aurora.Adaptor`, and does the `.clip`/`.skel`/`.graph` on-disk format need to change during relocation?
