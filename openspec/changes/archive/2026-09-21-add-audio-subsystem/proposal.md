## Why

SkyEngine has no audio subsystem at all: the engine can render a world but cannot play a sound. Games and tools
need at minimum one-shot SFX, looping ambient/music, 3D positional audio, and bus-level volume control (master /
music / SFX). The repository already has a proven "leaf interface module + backend plugin" seam (Physics/Bullet,
Navigation/Recast) and an SDL-backed platform layer, so now is the right time to add audio the same way.

`miniaudio` is chosen as the first backend because it is a single-header, cross-platform runtime audio library
with built-in decoders and a mixer/node graph. It does **not** replace a full middleware suite such as Wwise
(authoring tools, Event/Switch/State, SoundBanks, RTPC, interactive music, profiling); the AudioRegistry seam is
kept abstract so a Wwise/FMOD middleware backend plugin can be added later without changing gameplay code.

## What Changes

- Add a new leaf module `engine/audio` exposing the audio interface surface: `AudioRegistry` (singleton +
  abstract `Impl` factory, mirroring `PhysicsRegistry`), `AudioDevice`/`AudioEngine`, `AudioClip`, `AudioSource`,
  `AudioListener`, `AudioBus`, `AudioSystem` (an `IWorldSubSystem` with `NAME = "Audio"`), plus `AudioSourceComponent`
  and `AudioListenerComponent` reflected into the `World` component system.
- Add `plugins/audio`, a SHARED `AudioModule` (miniaudio backend) implementing `AudioRegistry::Impl` and registered
  via `REGISTER_MODULE`, following the `BulletPhysicsModule` pattern and toggled by
  `plugins/plugins.json` (`SKY_BUILD_AUDIO`) plus `configs/modules_game.json` / `configs/modules_editor.json`.
- Register `miniaudio` in `cmake/thirdparty.json` (header-only) with `cmake/thirdparty/Findminiaudio.cmake`, and
  link it only from the audio plugin.
- Add an audio asset pipeline path: `.audio` asset import/conversion of WAV/MP3/FLAC/OGG sources into a runtime
  clip asset, loaded through the existing asset system, with streaming (music/ambient) and in-memory (SFX) modes.
- Add bus/mixer volume control (master + named buses) and 3D spatialization (attenuation, panning, doppler), and
  device lifecycle (init/shutdown/device-loss handling) owned by the audio module.
- Add tests for the interface seam (null-backend safety, bus volume, asset metadata) following the module-local
  `AnimationTest` gtest pattern, and wire audio into a sample/launcher config.

**Non-goals**: authoring middleware features (Event/Switch/State, SoundBanks, RTPC, interactive music, profiling
UI), Wwise/FMOD integration, occlusion/raycast audio, HRTF/binaural plugins, and a dedicated audio editor panel.
These may be added later behind the same `AudioRegistry` seam.

## Capabilities

### New Capabilities
- `audio-core`: the engine-side audio interface surface (registry, device/engine, clips, sources, listener, buses,
  world subsystem and components), including the null-backend contract and bus/master volume behavior.
- `audio-3d`: 3D spatialization semantics — listener update, attenuation curves, distance/panning, doppler, and
  per-source spatial blend.
- `audio-backend-miniaudio`: the miniaudio backend plugin, third-party registration, plugin/module build switches,
  and runtime module configuration.
- `audio-asset`: audio clip assets — supported source formats (WAV/MP3/FLAC/OGG), import/convert into a runtime
  clip, memory vs streaming load modes, and the asset metadata contract.

### Modified Capabilities
<!-- none -->

## Impact

- New code: `engine/audio/**` (interface module), `plugins/audio/**` (miniaudio backend), `engine/test/audio/**`
  (tests).
- Build: `engine/CMakeLists.txt` (`add_subdirectory(audio)` and test wiring), `plugins/plugins.json`,
  `cmake/thirdparty.json`, `cmake/thirdparty/Findminiaudio.cmake`, `plugins/CMakeLists.txt` (unchanged logic).
- Runtime config: `configs/modules_game.json`, `configs/modules_editor.json`, and the builtin copies in
  `engine/configs/`.
- Asset builders: audio import/convert path alongside the existing render builder (`RenderBuilder.Static`) or a
  dedicated audio build step.
- Platform: relies on miniaudio's per-platform backends (WASAPI/DirectSound on Win32, CoreAudio on Apple, ALSA/
  PulseAudio on Linux, AAudio/OpenSL on Android). Existing macOS `AudioToolbox`/`CoreAudio` link flags in
  `cmake/configuration.cmake` remain valid.
- No breaking changes to existing modules; audio is opt-in via the plugin switch and module list.
