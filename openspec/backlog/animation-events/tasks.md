> **Current status — 2026-11-01: planned.** 事件/marker/state 语义尚未实施；既有 GraphAssets 不再另起无名 change。
> Owner: `animation-events`.

## 1. Event types and clip markers

- [ ] 1.1 Fill `animation/event/AnimationEvent.h` with event/marker types
- [ ] 1.2 Add a time-sorted marker track to `AnimationClip` and its asset data
- [ ] 1.3 Extend `AnimationSequencePlayer` to detect crossed markers (loop-aware)

## 2. Dispatch

- [ ] 2.1 Collect fired events in `AnimationClipNode` (PreTick/TickAny)
- [ ] 2.2 Dispatch once per frame through the `AnimationAsyncContext::PreTick` seam
- [ ] 2.3 Define a subscriber interface in the `Animation` module
- [ ] 2.4 Emit state enter/exit events from `AnimStateMachine`

## 3. Tests

- [ ] 3.1 Unit test marker crossing, including loop wrap and zero-length clips
- [ ] 3.2 Unit test state enter/exit events

## Related backlog

- [ ] 4.1 Reuse the existing Animation.GraphAssets schema from `add-skeletal-animation-workflow` and add event-node authoring within this event workflow
