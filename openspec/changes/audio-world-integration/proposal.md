## Why

The audio subsystem (archived change `add-audio-subsystem`) shipped the interface module, miniaudio backend,
assets and headless tests, but `AudioSystem` is never added to a `World`. This is the deferred attach task, so the
game runtime currently produces no audio. This change closes that gap.

## What Changes

- Attach `AudioSystem` (`NAME = "Audio"`) to the world in the aurora scene/runtime path and in the editor document.
- Reflect `AudioSourceComponent` / `AudioListenerComponent` into the aurora scene components and the editor.
- Drive the `AudioListener` from the active camera each frame; update source transforms from actor transforms.
- Keep the no-device path safe (the null-backend contract already supports it).

## Capabilities

### New Capabilities
- `audio-world-integration`: world/scene attachment, component registration, camera-driven listener.

### Modified Capabilities
<!-- to be expanded -->

## Impact

- `engine/aurora/adaptor` (component reflection), aurora scene/runtime world setup, `engine/editor` document path,
  launcher/sample scene.
- No new third-party dependencies.

## Open Questions (expand later)

- Where the game-world instance is created and owns the subsystem (GameApplication vs the aurora render module).
- Listener source: explicit `AudioListenerComponent` vs implicit active-camera binding.
