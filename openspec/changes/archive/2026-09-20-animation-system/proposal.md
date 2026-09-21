## Why

`engine/animation` has grown feature-by-feature (clip node, blend nodes, state machine, character locomotion) without a top-level capability spec, so its contract is implicit and its defects are hard to reason about. This change establishes `animation` as the main runtime animation capability spec and lands the first increment against it: the correctness fixes for blending, channel sampling, root motion, state-machine bounds, and clip duration found during review of the current implementation. All of these live in the `Animation` module, which depends only on `Core`; the change deliberately stays inside that boundary and guarantees the core evaluation path works independently of any renderer.

## What Changes

- Establish the `animation` capability as the authoritative description of the runtime skeletal animation system: renderer independence, animation instances and parameters, the node graph and layers, skeleton/pose model, clip data and sampling, sequence playback, pose blending, blend nodes, and the animation state machine.
- First increment (implemented by this change):
  - Correct override and additive pose blending to consume the source pose and blend rotations as rotations.
  - Make the pose-blend nodes weight-driven, index-safe, and free of the pure-virtual base call; make fade times actually take effect.
  - Sample position, rotation, and scale channels, preserving reference values for absent components.
  - Make root-motion evaluation non-destructive to the pose bone mask and use the skeleton's real root bone.
  - Make state-machine state selection bounds-safe.
  - Derive clip duration from real key times inside `AnimationClip`, so it works without any asset/renderer layer.
  - Add regression tests for the above.
- Renderer integration target: the animation rendering/consumer side SHALL move from `SkyRender` (`engine/render` / `RenderAdaptor`) to `aurora` (`Aurora.Adaptor`), where clip/skeleton/graph asset data and the preview/locomotion/debug-render components will live. The actual wiring is **deferred**: this change neither adds to nor fixes the `SkyRender` adaptor, and does not create aurora integration code.
- No new runtime features are introduced beyond the module's existing surface. `AnimationEvent`, `MotionDatabase`/`MotionMatcher`, `AnimationController`, and `CUBIC_SPLINE` interpolation remain unimplemented and are explicitly deferred.

## Capabilities

### New Capabilities
- `animation`: the runtime skeletal animation system - renderer independence, animation instances, node graph and layers, skeleton and pose data, clip sampling, sequence playback, pose blending, blend nodes, the animation state machine, and root motion. Renderer/asset integration is out of scope for this capability and handled by a deferred aurora integration increment.

### Modified Capabilities
<!-- None. No existing spec in openspec/specs/ covers the runtime animation system; this change bootstraps it. -->

## Impact

- `engine/animation`: `AnimationPose`, `AnimationNodeChannel`, `AnimationClipNode`, `AnimationState`, `PoseBlendNode`, `AnimationClip`, `AnimationPlayer`, `AnimationInput`.
- `engine/animation/test`: extended coverage.
- No public API removals; behavior fixes intentionally change existing playback/blend results.
- The `Animation` module continues to link only `Core`; no renderer dependency is added. Asset data (`AnimationClipAssetData`, `SkeletonAssetData`) stays in the adaptor layer for now and is relocated with the aurora integration.
- Deferred (not in this change):
  - Renderer integration migration from `SkyRender` to `aurora` (`Aurora.Adaptor`): clip/skeleton/graph asset relocation, preview/locomotion/skeletal-mesh/debug-render components.
  - Animation events, motion matching, animation controller, cubic-spline interpolation, state-machine cross-fade.
  - Data-oriented / GPU-driven node graph (main-spec target, implemented by the follow-up change `animation-data-oriented-graph`).
