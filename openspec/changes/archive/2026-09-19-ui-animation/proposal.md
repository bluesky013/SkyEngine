## Why

The UI has no time-driven animation, so nothing can tween transform/opacity or transition state. This is
render-independent (it changes element properties over time).

## What Changes

- Add easing functions and a float keyframe track.
- Add `UIAnimation`: a duration-based tween with an update callback, optional loop, and completion callback.
- Let `UIContext` own animations and advance them through `Tick(delta)`.

**Non-goals**: a full timeline/sequencer, animation assets/curves serialization, and editor authoring.

## Capabilities

### New Capabilities
- `ui-animation`: easing, keyframe tracks, and time-driven tweens advanced by the context.

## Impact

- New `animation/UIAnimation.{h,cpp}`; `UIContext` gains `AddAnimation`/`Tick`; tests in `engine/test/ui`.
