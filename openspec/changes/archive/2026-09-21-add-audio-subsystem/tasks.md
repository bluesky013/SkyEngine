## 1. Audio interface module scaffold

- [x] 1.1 Create `engine/audio/CMakeLists.txt` as a STATIC `Audio` target (`sky_add_library`, `PRIVATE_INC src`, `PUBLIC_INC include`, `LINK_LIBS Framework`) and add `add_subdirectory(audio)` to `engine/CMakeLists.txt`
- [x] 1.2 Add `engine/audio/include/audio/AudioRegistry.h` + `src/AudioRegistry.cpp`: `Singleton`, nested abstract `Impl` factory, null-safe delegation mirroring `PhysicsRegistry`
- [x] 1.3 Define public abstract types `AudioEngine`, `AudioClip`, `AudioSource`, `AudioListener`, `AudioBus` with `*Impl` hooks and no backend types in headers
- [x] 1.4 Add audio enums/structs (`AudioLoadMode`, `AttenuationModel`, known bus names `master`/`music`/`sfx`) and value types for clip metadata

## 2. World subsystem and components

- [x] 2.1 Implement `AudioSystem : IWorldSubSystem` with `static constexpr std::string_view NAME = "Audio"` and per-frame source/listener update from actor transforms
- [x] 2.2 Add `AudioSourceComponent` and `AudioListenerComponent` and reflect them through `SerializationContext` in the module `Start()`
- [x] 2.3 Register audio components with `ComponentFactory` and wire source playback params (clip, volume, pitch, loop, bus, spatial blend)
- [ ] 2.4 Extend the editor/world document path (`Document.cpp` equivalent) to attach `AudioSystem` when the audio backend is available

## 3. Bus graph and null-backend behavior

- [x] 3.1 Implement `AudioBus` tree rooted at `master` with volume propagation to descendants and routing of sources
- [x] 3.2 Verify the null-backend contract: `Create*` returns `nullptr`, dependent ops are no-ops, `UnRegister` restores null behavior
- [x] 3.3 Confirm the `Audio` target compiles and links with no backend present

## 4. miniaudio third-party integration

- [x] 4.1 Add a `miniaudio` header-only package entry to `cmake/thirdparty.json` with the pinned version/tag and supported platforms
- [x] 4.2 Add `cmake/thirdparty/Findminiaudio.cmake` using `sky_3rd_header_only(miniaudio)` creating `3rdParty::miniaudio`
- [x] 4.3 Run the third-party bootstrap for the target platforms and verify the package installs under `3RD_PATH/miniaudio`

## 5. Backend plugin (miniaudio)

- [x] 5.1 Create `plugins/audio/CMakeLists.txt` defining SHARED `AudioModule` linking `Audio`, `Framework`, `3rdParty::miniaudio`, plus `plugin.json` with dependencies `["Launcher", "Editor"]`
- [x] 5.2 Add the `audio` entry to `plugins/plugins.json` with `cmake_var: SKY_BUILD_AUDIO`
- [x] 5.3 Implement the miniaudio `Impl` factory and `AudioModule` (`REGISTER_MODULE`) with one translation unit defining `MINIAUDIO_IMPLEMENTATION`
- [x] 5.4 Implement device init/shutdown/status, decoding for WAV/MP3/FLAC/OGG, in-memory vs streaming playback, and bus→`ma_sound_group` mapping
- [x] 5.5 Implement 3D spatialization: listener transform, per-source attenuation/distance model, panning, optional doppler
- [x] 5.6 Implement graceful device-loss/failure handling and unregister on `Shutdown`

## 6. Audio asset pipeline

- [x] 6.1 Define the audio clip asset type and metadata (duration, channels, sample rate, load mode, default bus, loop) in the serialization/asset system
- [x] 6.2 Implement import/validation for WAV/MP3/FLAC/OGG with explicit rejection of unsupported formats
- [x] 6.3 Implement runtime loading honoring `InMemory` vs `Streaming` load modes through the asset system
- [x] 6.4 Resolve the asset-builder target decision from design Open Question 1 (extend `RenderBuilder.Static` vs dedicated audio build step) and implement it
- [x] 6.5 Verify loose-file (development) and packaged-bundle runtime resolution

## 7. Runtime configuration

- [x] 7.1 Add `AudioModule` to `configs/modules_game.json` and the builtin `engine/configs/modules_game.json`
- [x] 7.2 Add `AudioModule` to `configs/modules_editor.json` and the builtin `engine/configs/modules_editor.json` (gated by the plugin being built)
- [x] 7.3 Verify module dependency ordering does not break existing modules when audio is enabled or disabled

## 8. Tests and verification

- [x] 8.1 Add `engine/audio/test/` (or `engine/test/audio/`) gtest target covering registry null-backend behavior, bus volume propagation, and clip metadata
- [x] 8.2 Add a test verifying the public headers compile without miniaudio present
- [x] 8.3 Build with `SKY_BUILD_AUDIO=ON` and `OFF` and confirm both configurations succeed
- [x] 8.4 Manual verification: play a one-shot SFX, a looping ambient track, and a 3D positional source with bus volume changes on desktop
- [x] 8.5 Run `openspec validate add-audio-subsystem --strict` and confirm style checks (ASCII-only, no new warnings)
