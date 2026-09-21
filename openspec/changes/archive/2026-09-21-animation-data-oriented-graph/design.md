## Context

The `animation-system` change establishes the `animation` main spec and fixes correctness bugs, but keeps the object-oriented graph: `AnimNode` subclasses with virtual `InitAny`/`TickAny`/`EvalAny`, nodes owned as `std::vector<std::unique_ptr<AnimNode>>`, and `AnimationEval` carrying a `Skeleton*` and copying `std::vector<Transform>`. Measured anti-patterns in the evaluation path:

- Per-evaluation heap allocation: `PoseBlendNodeList::EvalAny` builds a `std::vector<AnimationEval>`; `PoseBlend2Node::EvalAny` copies two `AnimationEval` (each copying the transform vector); `AnimStateMachine::TickAny` builds a `std::set<AnimHandle>`.
- Per-bone hash lookups: `AnimationClip::SamplePose` iterates `std::unordered_map<Name, channel>` and calls `Skeleton::GetBoneByName` (another hash lookup) for every channel, every sample.
- Pointer/mixed-ownership data (`Skeleton*`, `CounterPtr`, `unique_ptr` node storage) that cannot be uploaded to a GPU buffer.
- The `.graph` asset is a stub in the legacy `SkyRender` adaptor: `AnimationAssetData` holds only `version`, and `AnimationBuilder::RequestGraph` neither saves the asset nor sets a result code. Since the renderer side is being retargeted to aurora, graph persisting is not frozen yet, which leaves the authoring structure malleable.

Meanwhile `engine/aurora` already provides the compute-side infrastructure needed for a GPU path later: `ComputePipeline`, `Dispatch`/`DispatchIndirect`, RDG compute passes, and the slang shader backend. Aurora deliberately defines no skeleton type and maps animation into a self-contained `Skin` (`inverseBindMatrices`, `boneMatrices`, `boneMapping`), so GPU evaluation would produce that palette through the existing contract.

## Goals / Non-Goals

**Goals:**
- Define and implement a data-oriented evaluation plan for the animation graph.
- Separate the control plane (dynamic, CPU) from the data plane (pure pose computation).
- Remove per-frame allocation, virtual dispatch, and per-bone hash lookups from evaluation.
- Make the data plane layout pointer-free, index-based, and fixed-stride so it is uploadable to GPU memory.
- Compile the authoring graph into the plan, with validation.
- Keep `AnimNode` as the authoring/builder API.

**Non-Goals:**
- Implementing the GPU compute evaluation pass itself (layout and CPU evaluator only).
- Animation events, motion matching, animation controller, cubic-spline interpolation.
- State-machine cross-fade.
- Rendering the pose: skinning, debug drawing, and component/preview integration stay in the deferred aurora integration layer.
- Cook-time `.graph` persistence and the `.clip`/`.skel`/`.graph` builder/creator work (adaptor-owned, relocated with the aurora integration).

## Decisions

### D1: Flat op plan over a preallocated pose pool
Compile the graph into a fixed-size `AnimOpRecord` array evaluated in topological order:

```cpp
enum class AnimOp : uint8_t { ClipSample, Blend, AdditiveBlend, Layer, StateSelect, Output };

struct AnimOpRecord {          // trivially copyable, fixed stride
    AnimOp   op;
    uint16_t inputA;
    uint16_t inputB;
    uint16_t output;
    float    weight;
    uint32_t dataIndex;        // clip index / channel range / mask range / state table
};
```

Evaluation walks the array and switches on `op`; pose data lives in a pool of `numSlots x numBones` contiguous `Transform`s. A `Layer` op writes `weight` rather than threading a recursive `AnimLayerContext`.
- *Alternative considered*: keep virtual nodes and add a parallel GPU exporter. Rejected - two evaluation implementations to keep in sync, and no cache-friendly CPU path.

### D2: Control plane / data plane split
Control plane (CPU, may branch and allocate at frame boundaries): parameter updates, state-machine transitions and condition evaluation, clip time advance, root-motion decisions, event dispatch. Data plane (pure, allocation-free): clip sampling, blending, skin-matrix generation. The plan's op list is the boundary: the control plane writes a small "frame state" block (active state, clip times, weights), the data plane consumes it.
- *Alternative considered*: put the state machine in the plan. Rejected - transitions are data-dependent branching and would require GPU/CPU readback.

### D3: Index-based channel and bone resolution
Resolve `Name`-keyed channels and bones to indices during plan compilation, producing per-op channel ranges and a bone-to-channel mapping. `AnimationClip` keeps a name index for authoring, but the runtime sampling path uses indices only, removing `unordered_map`/`GetBoneByName` from the hot loop.

### D4: Allocation-free evaluation
The plan owns all scratch: pose pool, blend scratch slots, and transition work buffers. `EvalAny` performs no heap allocation. The transition search uses a preallocated visited bitset owned by the state machine instead of a per-tick `std::set`.

### D5: SoA keyframe storage
Clip keyframes are stored as separate fixed-stride blocks (`times[]`, `values[]`) per channel component, with an interpolation tag per component. This removes per-key `std::pair` layout and makes the buffer directly uploadable.
- *Alternative considered*: keep `AnimChannelData<T>` with `vector<T> keys`. Rejected - interleaved layout impedes coalesced GPU reads and mixes host/device concerns.

### D6: Pointer-free, serializable graph data
The compiled plan plus clip/SoA blocks, skeleton indices, and bone masks form a self-contained byte layout with no pointers, using 32/64-bit offsets. It is versioned and serializable, so a future compute pass can bind it as an SSBO and produce the `Skin` bone-matrix palette. The CPU evaluator and the future GPU evaluator read the same layout.

### D7: Compile authoring graph to plan
Add a compile step inside `engine/animation` (init/compile time for code-built graphs; later also cook time for `.graph` once the aurora integration lands) that lowers the `AnimNode` DAG and `AnimStateMachine` into the op list, assigns pose slots, and validates: no cycles, all references in range, single output op, bone indices within the skeleton. Invalid graphs fail with a diagnostic rather than evaluating undefined behavior. Cook-time persistence of the compiled plan is deferred with the aurora integration.

### D8: CPU-first, GPU as a later increment
Land the CPU data-oriented evaluator and serializable layout first; keep the GPU compute pass out of this change. This bounds risk and lets the layout be validated against the CPU path before it is frozen into shaders.
- *Alternative considered*: ship CPU and GPU together. Rejected - doubles the surface and blocks the CPU wins on GPU work.

### D9: Debuggability
Provide a plan dump (op-by-op with slot/data indices) and a compile-time validation report so the indirection stays inspectable, replacing the readability that virtual node names provided.

## Risks / Trade-offs

- [Indirection hurts debuggability] → D9 plan dump + validation; keep `AnimNode` authoring names mapped to ops in metadata.
- [Plan compilation bugs produce silent wrong poses] → validation pass (D7) plus golden-pose tests comparing the plan evaluator against reference poses.
- [Two data-plane representations (host SoA, device buffer) drift] → single fixed-stride layout (D6) shared by both; offsets defined once.
- [Larger refactor than the correctness fix] → sequenced after `animation-system`; correctness fixes land first and the plan evaluator is tested against those results.
- [`.graph`/`.clip` asset work is deferred] → the plan is produced in the core from code-built graphs now; cooked-asset reproduction and the builder/creator move with the aurora integration, so no source re-authoring is needed in between.

## Migration Plan

1. Land `animation-system` (correctness fixes + main spec) first and archive it so `openspec/specs/animation/` exists.
2. Introduce the plan types and CPU evaluator behind the existing `AnimNode` authoring API; keep behavior identical, verified by tests.
3. Migrate the core graph nodes (`AnimationClipNode`, `PoseBlend*`, `AnimStateMachine`) to lower to ops and stop using virtual evaluation.
4. Produce the pointer-free layout in the core and validate round-trip in memory.
5. Deferred with the aurora integration: cook-time `.graph` persistence and renderer consumers binding to the plan-produced pose/palette.
6. GPU evaluation lands as a separate change against the frozen layout.

Rollback: the plan evaluator is additive until the core nodes migrate; revert the change commit before the layout is persisted into cooked assets.

## Open Questions

- Is a single global pose pool with slots sized for the widest clip acceptable, or is per-instance paging needed for very large skeletons?
- Should plan compilation live in `engine/animation` (runtime init) only, or also be exposed for cook-time use by the aurora integration?
- What fixed bone-count/key-format limits are acceptable to keep the GPU layout simple?
- Does GPU evaluation need one buffer per instance or a batched multi-instance layout (compute dispatch over instances)?
