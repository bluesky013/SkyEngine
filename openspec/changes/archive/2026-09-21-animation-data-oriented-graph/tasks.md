# Tasks

## 1. Plan and pose pool

- [x] 1.1 Define `AnimOp` and `AnimOpRecord` (trivially copyable, fixed stride) in `engine/animation/include/animation/graph/`
- [x] 1.2 Add a plan type owning the op array, pose pool (`numSlots x numBones` contiguous `Transform`s), and scratch slots
- [x] 1.3 Assign pose slots (inputs/outputs) during plan construction and expose the output slot

## 2. Data-plane evaluator

- [x] 2.1 Implement the plan evaluator: topological walk with `switch (op.op)` over `ClipSample` / `Blend` / `AdditiveBlend` / `Layer` / `StateSelect` / `Output`
- [x] 2.2 Implement clip sampling ops against index-based channel ranges
- [x] 2.3 Implement blend ops against the pose pool without intermediate `AnimationEval` copies
- [x] 2.4 Ensure the evaluator performs no heap allocation (bounds-check with a test-only allocation counter)

## 3. Index resolution

- [x] 3.1 Precompute bone-to-channel index mapping during head/plan compilation, removing `GetBoneByName` from the sampling loop
- [x] 3.2 Keep a name index on `AnimationClip` for authoring only; runtime sampling uses indices

## 4. SoA keyframe layout

- [x] 4.1 Replace interleaved per-channel key storage with separate fixed-stride time and value blocks per component
- [x] 4.2 Provide a core-side conversion from the source channel data to the SoA layout (in `engine/animation`); `.clip` builder emission moves with the deferred aurora integration
- [x] 4.3 Update `AnimationNodeChannel` sampling to read the SoA layout

## 5. Control-plane separation

- [x] 5.1 Define a frame-state block (active state, clip times, weights) produced by the control plane and consumed by the data plane
- [x] 5.2 Move state-machine transitions, condition evaluation, parameter updates, and root-motion decisions to the control plane
- [x] 5.3 Replace the per-tick `std::set<AnimHandle>` transition search with a plan-owned visited bitset

## 6. Graph compilation and validation

- [x] 6.1 Add a compile step in `engine/animation` lowering the `AnimNode` DAG and `AnimStateMachine` to the op plan
- [x] 6.2 Validate cycles, out-of-range state/condition/bone references, and a single output op; emit diagnostics on failure

## 7. Core node migration

- [x] 7.1 Lower `AnimationClipNode` to ops and keep loop/play/root-motion control on the control plane
- [x] 7.2 Lower `PoseBlend2Node` / `PoseBlendNodeList` to blend ops
- [x] 7.3 Lower `AnimStateMachine` state evaluation to ops

## 8. Serializable layout

- [x] 8.1 Define the versioned, pointer-free compiled-plan byte layout (offsets only) covering ops, clips, skeleton indices, and masks
- [x] 8.2 Implement serialization and deserialization with a round-trip test
- [x] 8.3 Document the layout so a future GPU compute pass binds it directly

## 9. Debuggability

- [x] 9.1 Add a plan dump (op kind, slots, data index) and a compile validation report

## 10. Tests

- [x] 10.1 Golden-pose test: plan evaluator matches reference poses for clip / blend / state-machine graphs
- [x] 10.2 Allocation test: evaluation performs no heap allocation across frames
- [x] 10.3 Validation test: cyclic and out-of-range graphs are rejected with diagnostics
- [x] 10.4 Round-trip test: serialized and deserialized plan produces identical poses

## 11. Verification

- [x] 11.1 Build the `Animation` target; confirm it still links only `Core` with no renderer dependency
- [x] 11.2 Run the animation test target with `SKY_BUILD_TEST=ON` and confirm all tests pass
- [x] 11.3 Profile the evaluation hot path before/after to confirm allocation and lookup removal

## Dependencies

- Depends on `animation-system` landing and archiving first so `openspec/specs/animation/` exists; this change's delta MUST be archived after it.

## Future increments (out of scope)

- SkyRender-to-aurora integration: relocate clip/skeleton/graph assets and the `.clip`/`.graph` builder/creator, and port the preview/locomotion/skeletal-mesh/debug-render components to bind the plan-produced pose/palette
- GPU compute evaluation producing the `aurora::Skin` bone-matrix palette from the frozen layout
- Batched multi-instance GPU dispatch layout
- Cubic-spline interpolation and state-machine cross-fade on the op plan
- SoA SIMD (evaluated, deferred, not scheduled): upgrade pose pool and keyframe blocks to true SoA (component-split arrays, 16B aligned) and vectorize Blend/AdditiveBlend/Layer with SSE/NEON. Hand-written sampling/slerp SIMD is deprioritised in favour of ACL2 decompression (which is already SIMD). Precondition: `SKY_MATH_SIMD` is currently OFF and `Quaternion`/`Vector3` are scalar.

## 12. Plan parity supplements

- [x] 12.1 Add plan-level bone masks (`AnimOpRecord.boneMaskIndex`, mask table, mask-aware `AnimationTrackData::SamplePose`) and `AnimationClipNode::SetBoneMask/GetBoneMask`
- [x] 12.2 Expose the consumed root-motion delta from `AnimationPlan::Evaluate` / `AnimationPlanRuntime::Evaluate`
- [x] 12.3 Drive blend weights from the frame state (weight slots) and normalize multi-pose list weights at runtime with `PoseBlendNodeList::SetPoseWeight`
- [x] 12.4 Emit additive blend ops via `PoseBlend2Node::SetAdditive`
- [x] 12.5 Implement layer weight semantics (blend the layer input over the bind pose by weight)
- [x] 12.6 Replace compiler `dynamic_cast` dispatch with the `AnimNode::LowerToPlan` virtual extension point
- [x] 12.7 Gate inactive state-machine operations via the frame-state op-enable table
- [x] 12.8 Tests for bone mask, root-motion delta, dynamic list weights, additive emission, layer weight, and inactive-state gating
- [x] 12.9 Performance benchmark comparing virtual vs flat evaluation (allocations per eval + ns/eval sweep over 16/64/128 bones) on an equivalent blend graph
