> **Current status — 2026-11-01: blocked-on-prereq.** 等待事件分发与 World 音频接入；复用既有动画 world/palette 对接。
> Owner: `audio-animation-bridge`. Prerequisites: `animation-events`, `audio-world-integration`.

## 1. Bridge layer

- [ ] 1.1 Add a bridge target linking `Animation` + `Audio` (keep both modules independent)
- [ ] 1.2 Subscribe to the animation event dispatch from `animation-events`

## 2. Mapping component

- [ ] 2.1 Add a reflected component mapping event name to clip/bus/volume/pitch/spatial parameters
- [ ] 2.2 Play through `AudioSystem` / `AudioSource` on a matching event
- [ ] 2.3 Route through named buses and apply spatial parameters

## 3. 3D positioning

- [ ] 3.1 Add a bone world-transform query to the pose/animation side
- [ ] 3.2 Position 3D event sounds at the bone socket

## 4. Tests

- [ ] 4.1 Headless test: a synthetic event triggers a clip on the expected bus
- [ ] 4.2 Verify mapping/unmapping is safe when the audio backend is absent

## Related backlog

- Existing owner: `add-skeletal-animation-workflow` supplies the animation world/palette bridge. This change only adds event mapping and bone positioning; it does not create a second skin bridge.
