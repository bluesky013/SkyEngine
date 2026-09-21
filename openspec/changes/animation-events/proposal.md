## Why

The animation subsystem has an `AnimNode` graph and `AnimStateMachine`, but no way to emit gameplay events:
`animation/event/AnimationEvent.h` is an empty stub, `AnimationClip` has no markers, and
`AnimationSequencePlayer` does not report playhead marker crossings. `AnimationAsyncContext::PreTick` even contains
an unimplemented `// fetch events` seam. Without this, audio (and VFX/gameplay) cannot react to animation.

## What Changes

- Define animation event types and a time-keyed marker track on `AnimationClip`.
- Make `AnimationSequencePlayer` report markers crossed between the previous and current time, including loop wrap.
- Collect fired events in `AnimationClipNode` and dispatch them once per frame through `AnimationAsyncContext`.
- Emit state enter/exit events from `AnimStateMachine::SetState` / `Transition`.
- Provide a subscriber interface in the `Animation` module (no audio types).

## Capabilities

### New Capabilities
- `animation-events`: marker track, playhead crossing detection, per-frame dispatch, state enter/exit events.

## Related backlog (to split into its own change when expanded)

- `animation-graph-assets`: complete the started-but-unused graph asset schema (`AnimationAssetData` currently only
  carries `version`; `AnimationStateData` / conditions are defined but unused) and add an event-node authoring UI.

## Impact

- `engine/animation` (`core/AnimationClip`, `core/AnimationPlayer`, `graph/AnimationClipNode`,
  `graph/AnimationState`, `event/AnimationEvent`), asset `.clip` data, animation tests.
- No new dependencies (`Animation` stays dependent on `Core` only).

## Open Questions (expand later)

- Marker authoring format (clip asset vs json sidecar) and loop-wrap event ordering.
- Whether state events reuse the same dispatcher or a separate channel.
