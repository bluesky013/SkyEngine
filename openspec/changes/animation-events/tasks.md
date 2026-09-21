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

- [ ] 4.1 `animation-graph-assets`: complete graph asset schema + event-node editor (split into its own change)
