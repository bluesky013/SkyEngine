## Why

The animation node graph is pointer-and-virtual based: `AnimNode` subclasses are dispatched through virtual calls, `AnimPose`/`AnimationEval` carry `Skeleton*` and copy `std::vector<Transform>` every evaluation, channels are looked up by interned `Name` in an `unordered_map` per bone per frame, and `AnimStateMachine::TickAny` allocates a `std::set` every tick. This is a CPU hot-path cost today and blocks any future GPU evaluation. The `.graph` asset is still a stub (`AnimationAssetData` stores only a version and `RequestGraph` does not persist anything), so the cheapest moment to reshape the structure is now, before graph authoring freezes the current shape.

## What Changes

- Introduce a **data-oriented evaluation plan**: the graph compiles to a flat array of fixed-size op records (`AnimOpRecord`: op tag, input/output pose slots, weight, data index) evaluated in topological order over a preallocated pose pool, replacing virtual `EvalAny` dispatch in the data plane.
- Separate the **control plane** (CPU: state-machine transitions, conditions, parameters, clip time advance, root-motion decisions, events) from the **data plane** (pure pose computation: clip sampling, blending, skin-matrix generation).
- Eliminate per-frame allocation and per-bone hash lookups from evaluation: pose pool, scratch, and transition work buffers are owned by the plan; channels and bones resolve to indices at compile time.
- Move clip keyframes to **SoA buffers** (times and values in separate, fixed-stride blocks) instead of per-channel `std::vector` pairs.
- Make graph data **pointer-free and index-based** so the same layout is uploadable to a GPU buffer; a compute pass can later produce the `aurora::Skin` bone-matrix palette.
- **Compile the authoring graph** (node DAG / state machine) into the evaluation plan, with a validation pass that rejects cycles and out-of-range references.
- Keep the existing `AnimNode` types as the **authoring/builder** surface that lowers to the plan; the runtime no longer depends on virtual evaluation.
- Stay inside the `Core`-only `Animation` module. Cooking/asset persistence for `.graph` and the renderer consumers are the deferred aurora integration's responsibility, not this change's.

## Capabilities

### New Capabilities
<!-- None. This change reshapes the existing runtime animation system. -->

### Modified Capabilities
- `animation`: the node-graph and evaluation model changes from an object graph to a data-oriented evaluation plan, and adds requirements for pointer-free, index-based, allocation-free, serializable data-plane evaluation. Depends on `animation-system` establishing the `animation` capability; this change's delta SHALL be archived after `animation-system`.

## Impact

- `engine/animation`: new plan/op types and evaluator; `AnimNode` retained as authoring API; `Animation`, `AnimationClip`, `AnimationNodeChannel`, `AnimationPose`, `AnimationState`, `AnimationClipNode`, `PoseBlendNode` reworked to lower to and execute the plan.
- `engine/animation/test`: extended coverage.
- `engine/aurora`: no dependency added to the animation module; GPU evaluation, when it lands, consumes a flat pose/palette buffer through the existing `Skin` contract.
- Renderer/asset integration is out of scope and deferred: the `SkyRender` adaptor (`AnimationPreviewComponent`, `CharacterLocomotion`, `SkeletalMeshComponent`, `SkeletonDebugRender`) and the `.clip`/`.graph` builder/creator are neither modified nor extended here. When the aurora integration lands, those consumers bind to the plan-produced pose/palette.
- Non-goals: implementing GPU compute evaluation itself, animation events, motion matching, cubic-spline interpolation, and the SkyRender-to-aurora integration migration.
