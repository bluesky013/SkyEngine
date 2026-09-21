# Tasks

All tasks are confined to `engine/animation` (which links only `Core`) and its tests.

## 1. Pose blending primitives

- [x] 1.1 Fix `AnimPose::BlendTransform` in `engine/animation/src/core/AnimationPose.cpp` to interpolate `dst` toward `src` by weight (translation/scale lerp, rotation slerp via `AnimSphericalLinear`)
- [x] 1.2 Fix `AnimPose::BlendTransformAdditive` so translation/scale accumulate weighted deltas and rotation uses a proper weighted rotation delta, keeping rotations unit-length
- [x] 1.3 Confirm `AnimPose::BlendPose` override/additive paths iterate the full transform range safely and keep the existing size-mismatch guard

## 2. Pose blend nodes

- [x] 2.1 Add `SetBlend(bool)` / `SetBlendTime(float)` to `PoseBlend2Node` and declare them in `engine/animation/include/animation/graph/PoseBlendNode.h`
- [x] 2.2 Rework `PoseBlend2Node::EvalAny` in `engine/animation/src/graph/PoseBlendNode.cpp` to evaluate A and B into scratch `AnimationEval` copies and blend into a reference-reset destination at `blendedAlpha`
- [x] 2.3 Replace the pure-virtual `AnimNode::InitAny(context)` call in `PoseBlend2Node::InitAny` with correct base handling
- [x] 2.4 Fix `AnimFadeInOut` construction in `engine/animation/src/core/AnimationInput.cpp` so `fadeInTime`/`fadeOutTime` are set from the constructor argument
- [x] 2.5 Add a builder (`AddPose`) to `PoseBlendNodeList`, populate `poses`/`desiredWeights`, and rewrite `PoseBlendNodeList::EvalAny` to track source indices instead of indexing the compacted scratch vector

## 3. Channel sampling

- [x] 3.1 Update `AnimationNodeChannel::Sample` in `engine/animation/src/core/AnimationNodeChannel.cpp` to sample position, rotation, and scale only when their `times` are non-empty, leaving reference values otherwise (remove the commented-out scale line)

## 4. Root motion

- [x] 4.1 Update `AnimationClipNode::EvalAny` in `engine/animation/src/graph/AnimationClipNode.cpp` to stop mutating `context.pose.boneMask`
- [x] 4.2 Resolve the root bone from the skeleton (`GetRoots()` / parent `INVALID_BONE_ID`) instead of hardcoded index `0`, and reset the root transform to the reference pose when root motion is disabled
- [x] 4.3 Record the consumed root delta on `AnimationEval` (data only; no actor application in this change)

## 5. State machine bounds

- [x] 5.1 Clamp state handles to `[0, states.size() - 1]` in `AnimStateMachine::SetState` (`engine/animation/src/graph/AnimationState.cpp`)
- [x] 5.2 Validate state/condition handles in `AddTransition`, `Transition`, and `FindTransition`; remove or use the unused `visited` parameter
- [x] 5.3 Resolve `ANIM_MAX_TRANSITION_PER_FRAME` (use it or remove it) in `engine/animation/include/animation/graph/AnimationState.h`

## 6. Clip duration in the core

- [x] 6.1 Make `AnimationClip` derive its duration/frame count from its channels' last key time (`maxKeyTime + 1` over all components) in `engine/animation`, removing any dependence on a frame count supplied by an asset/adaptor layer
- [x] 6.2 Keep `SetFrameRate` and the existing `SetNumFrame` surface usable for explicit authoring, but ensure `GetDuration()` is correct when only channels and frame rate are set
- [x] 6.3 Verify `AnimationSequencePlayer` stop/wrap behavior against the derived duration

## 7. Tests

Each test maps to a scenario in `specs/animation/spec.md`.

Clip channel data:

- [x] 7.1 Compression drops redundant keys and preserves first/last (float and vector)
- [x] 7.2 Keyframe span lookup: between keys, exact key time, before first, after last
- [x] 7.3 Single-key component returns that key at any sample time
- [x] 7.4 Resize allocates matching times/keys storage

Channel sampling:

- [x] 7.5 Step interpolation holds the previous key
- [x] 7.6 Linear interpolation returns the midpoint between two keys
- [x] 7.7 Rotation sampling returns a unit quaternion along the shortest arc
- [x] 7.8 Scale is applied when the channel provides scale keys
- [x] 7.9 An absent component preserves the incoming reference value
- [x] 7.10 Bone mask excludes a channel whose bone bit is cleared

Pose:

- [x] 7.11 Reset to reference pose; reset without a skeleton is a no-op
- [x] 7.12 `NormalizeRotation` yields unit-length rotations
- [x] 7.13 Skin matrices: one per bone, root transform applied, and parents affect children

Pose blending:

- [x] 7.14 Override blending at weight 0 / 0.5 / 1
- [x] 7.15 Additive blending at weight 0 and partial; result remains a valid unit rotation
- [x] 7.16 Mismatched pose sizes leave the destination pose unmodified

Blend nodes:

- [x] 7.17 Two-pose blend at alpha 0.5 equals the interpolation of the two source poses
- [x] 7.18 Zero-weight list entries are skipped without out-of-range access
- [x] 7.19 Blend alpha reaches the target after the blend time; disabling blend returns to the first pose

Playback and duration:

- [x] 7.20 Play rate scales advance; reverse playback clamps at zero
- [x] 7.21 Non-looping stops at duration; looping wraps and remains playing
- [x] 7.22 Duration derived from channels only: sparse clip matches `(lastKey + 1) / frameRate`; zero frame rate yields zero

State machine:

- [x] 7.23 Default entry is the first state when none is configured
- [x] 7.24 Valid transition is taken; a false condition keeps the current state
- [x] 7.25 At most one transition per tick; a transition from a non-current state is ignored
- [x] 7.26 Re-selecting the current state does not re-initialize it
- [x] 7.27 An out-of-range state handle does not access an out-of-range state

Root motion, parameters, lifecycle:

- [x] 7.28 Root-motion-disabled evaluation leaves the bone mask unchanged across evaluations
- [x] 7.29 The consumed root delta is available on the evaluation context
- [x] 7.30 Evaluation does not advance time: repeated evaluation yields the same pose
- [x] 7.31 Bool/float parameter read-back and an unavailable-parameter condition evaluates false

## 8. Verification

- [x] 8.1 Build the `Animation` target with CMake using the repository build workflow; confirm it still links only `Core` and has no renderer dependency
- [x] 8.2 Run `ctest` / the `AnimationTest` target with `SKY_BUILD_TEST=ON` and confirm all tests pass
- [x] 8.3 Confirm the core changes do not touch `SkyRender`/`RenderAdaptor` sources

## Future increments (out of scope for this change)

Spec-defined but not scheduled: the builder contract lives in `specs/animation/spec.md` (Animation asset builder, Cook-time curve compression, Runtime decompression, ACL2 third-party dependency, Animation asset versioning and compression metadata). Tracked here as TODOs for a later increment; no implementation in this change.

Builder + ACL2 compression (TODO, tracked):

- TODO: add `acl` (ACL v2) to `python/third_party.py` (static, all platforms) and expose it as a CMake target; no vendored copy under `engine/`.
- TODO: extend the animation asset builder to compress clip tracks with ACL at cook time, with per-clip error threshold and compression level defaults.
- TODO: add the runtime ACL-decompression path behind the existing channel-sampling interface, with the uncompressed fallback retained.
- TODO: preserve the last key time through compression so key-derived duration stays correct.
- TODO: version the clip asset, record compression settings, and reject unsupported versions at load.
- TODO: ensure only ACL decompression is linked into the animation runtime (no compression/optimizer).

Other deferred work:

- Renderer integration migration from `SkyRender` to `aurora` (`Aurora.Adaptor`), deferred:
  - Relocate clip/skeleton/graph asset data (`AnimationClipAssetData`, `SkeletonAssetData`, `AnimationAssetData`) and the `.clip`/`.graph` builder/creator to the aurora adaptor.
  - Port consumers: preview component, character locomotion, skeletal mesh, skeleton debug render.
  - Absorb the known `SkyRender`-side defects (wrong-case include, missing skeleton-asset null check) during that migration.
- Animation events (`engine/animation/include/animation/event/AnimationEvent.h`)
- Motion matching (`MotionDatabase`, `MotionMatcher`)
- Animation controller
- Cubic-spline interpolation
- State-machine cross-fade / transition blending
- Data-oriented evaluation plan (flat op records + pose pool, no per-frame allocation, no virtual dispatch in the data plane) and GPU-driven evaluation - see follow-up change `animation-data-oriented-graph`
- Asset-format `numFrame` and asset re-cook, if key-derived duration proves insufficient
