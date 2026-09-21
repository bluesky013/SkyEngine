## Why

Animation events (`animation-events`) should drive audio, e.g. footsteps, weapon sounds, or state-transition
stingers. `Animation` (depends on `Core`) and `Audio` (depends on `Framework`) must stay mutually independent, so
the mapping belongs in a new bridge layer, mirroring how `render/adaptor` links `Animation` + `Framework`.

## What Changes

- Add a bridge layer that links `Animation` + `Audio` (neither module gains a dependency on the other).
- Add a component mapping animation event names to audio clips, bus, volume, pitch, and spatial parameters.
- Subscribe to the animation event dispatch; on a matching event, play through `AudioSystem` / `AudioSource`.
- Support 3D positioning at a bone socket (e.g. foot/weapon), which needs a bone world-transform query.

## Capabilities

### New Capabilities
- `audio-animation-bridge`: event-name to audio-clip mapping component and playback wiring.

## Related backlog (to split into its own change when expanded)

- `aurora-animation-bridge`: map `animation::AnimPose` to `aurora::Skin::boneMatrices` in a bridge layer (aurora
  must not depend on `Animation`). Bone-socket audio positioning depends on this; the legacy bridge lives in
  `engine/render/adaptor`.
- Bone socket query: expose `AnimPose::GetBoneTransform(Name)`-style access (currently only `ToSkinRenderData`).

## Impact

- New bridge target linking `Animation` + `Audio`; a reflected component (registered in the audio component group).
- No change to the `Animation` or `Audio` module dependencies.

## Open Questions (expand later)

- One-to-one event/clip mapping vs weighted variants (randomized footsteps).
- Whether the mapping is per-actor authored data or an asset (event table).
