## Context

SkyEngine has no audio subsystem; a repo-wide search finds only the macOS `AudioToolbox`/`CoreAudio` link flags
in `cmake/configuration.cmake` (inherited from SDL). Everything else is greenfield.

The engine already has a repeated, proven seam for optional subsystems:

- A **leaf interface module** (`engine/physics`, `engine/navigation`) owns abstract types plus a `Singleton`
  registry with a nested `Impl` factory (`PhysicsRegistry`, `NaviMeshFactory`) and null-safe delegation.
- A **backend plugin** under `plugins/` (`BulletPhysicsModule`, `RecastModule`) implements the factory and exposes
  `REGISTER_MODULE`, giving a `StartModule(Environment*)` C entry point (`engine/framework/include/framework/interface/IModule.h`).
- `ModuleManager` topologically loads shared libraries named after the module from `configs/modules_*.json`.
- Runtime world state is attached to `World` as an `IWorldSubSystem` (`World::AddSubSystem` / `Tick`).
- Third-party libraries are declared in `cmake/thirdparty.json`, discovered via `cmake/thirdparty/Find<lib>.cmake`
  and linked as `3rdParty::<lib>`.

Audio must follow these seams so it stays opt-in, swappable, and consistent with the rest of the engine. The
backend libraries differ in scope: `miniaudio` is a runtime playback engine (decode + mix + node graph + basic 3D),
whereas Wwise/FMOD are full middleware suites. The design therefore hides the backend entirely behind
`AudioRegistry` so a middleware backend can be added later without gameplay changes.

## Goals / Non-Goals

**Goals**

- Add an `engine/audio` interface module with a backend-agnostic API: device/engine lifecycle, clips, sources,
  listener, buses, world subsystem, and reflected components.
- Ship `miniaudio` as the first backend plugin (`plugins/audio`, `AudioModule`) with WAV/MP3/FLAC/OGG decode,
  memory and streaming playback, bus mixing, and 3D spatialization.
- Integrate with the existing build/runtime configuration: `plugins.json` switch, `modules_game.json` /
  `modules_editor.json` entries, and `cmake/thirdparty.json` + `Findminiaudio.cmake`.
- Add an audio asset path so clips are authorable assets, not hardcoded file paths.
- Make the null-backend case safe: with no audio plugin loaded, all `AudioRegistry` calls must behave sanely
  (no crash, `nullptr`/no-op) exactly like `PhysicsRegistry` without Bullet.
- Provide gtest coverage for the interface seam and asset metadata.

**Non-Goals**

- Event/Switch/State, SoundBanks, RTPC, interactive music, profiling/capture UI, or an audio editor panel.
- Wwise/FMOD integration in this change (only the seam that enables it later).
- Occlusion/raycast audio, HRTF/binaural plugins, convolution reverb, or a full DSP effect authoring graph.
- Changing existing render/physics/navigation behavior.

## Decisions

### D1. Leaf interface module + SHARED backend plugin (mirror Physics/Bullet)

`engine/audio` is a STATIC library (`sky_add_library(TARGET Audio STATIC ... LINK_LIBS Framework)`, mirroring
`engine/physics/CMakeLists.txt`) with `include/audio/**` public headers and `src/**`. The concrete backend lives in
`plugins/audio` as a SHARED target named `AudioModule`, gated by `SKY_BUILD_AUDIO` in `plugins/plugins.json` and
listed in the runtime module JSONs.

Rejected alternative: a single built-in module that compiles the backend in directly. It is simpler now but makes
the backend non-swappable and forces miniaudio (and any future middleware) into every binary, including servers
and tools that do not need audio.

### D2. Backend hidden behind `AudioRegistry`; miniaudio is just the first `Impl`

`AudioRegistry` is a `Singleton` with a nested abstract `Impl` that creates the backend runtime objects. All public
engine types (`AudioEngine`, `AudioClip`, `AudioSource`, `AudioBus`, ...) are abstract bases with `*Impl` hooks, as
with `PhysicsWorld`. **No miniaudio type appears in a public `engine/audio` header.**

This is what makes "can miniaudio replace Wwise?" a non-question architecturally: miniaudio covers the runtime
output/decode/mix/3D layer today, and a Wwise/FMOD plugin can be registered later that implements the same factory
for Event/SoundBank/RTPC-driven behavior, while gameplay uses identical `AudioSource`/`AudioBus` calls.

Rejected alternative: expose miniaudio's `ma_engine` types directly. Fastest to write, but hard-couples the engine
to miniaudio and blocks any future middleware backend.

### D3. Object model

- `AudioEngine` (owned by the module / `AudioRegistry`): owns the device, the master bus, and the clip cache;
  `Init`/`Shutdown`/device-loss handling.
- `AudioClip`: decoded or streamable audio resource, loaded through the asset system.
- `AudioSource`: a playing (or playable) instance referencing a clip with volume, pitch, loop, spatial blend, and
  playback state (`Play`/`Pause`/`Stop`/`IsPlaying`).
- `AudioListener`: transform-driven listener (usually the camera); updates position/orientation.
- `AudioBus`: named mixer node (e.g. `master`, `music`, `sfx`) with a volume and parent bus.
- `AudioSystem : IWorldSubSystem` with `static constexpr std::string_view NAME = "Audio"`: ticks sources/listener
  each frame and is attached via `World::AddSubSystem(Name(AudioSystem::NAME.data()), ...)`, mirroring
  `PhysicsWorld`/`NavigationSystem`.
- `AudioSourceComponent` and `AudioListenerComponent`: reflected components registered in the module `Start()` via
  `ComponentFactory`, so sources are authorable in the world and pick up their actor transform.

### D4. Bus/mixer graph: master + named buses

Buses are a shallow tree rooted at `master` (default children `music`, `sfx`). Bus volume multiplies down the
chain; a bus volume of 0 mutes its subtree. The backend maps buses onto miniaudio sound groups (`ma_sound_group`),
so mixing/volume is applied on the audio thread rather than per-frame on the main thread.

Rejected alternative: per-source volume only, no buses. It cannot express "master/music/sfx" sliders, which is the
single most common runtime audio need and is hard to retrofit into an asset/event model later.

### D5. Decoding and formats via miniaudio built-in decoders

WAV, MP3, FLAC, and OGG Vorbis decode through miniaudio's built-in decoders, so no extra third-party libraries
(libsndfile/dr_libs) are needed. The engine exposes the supported-format set and the importer rejects unsupported
sources at build time rather than failing at runtime.

Rejected alternative: WAV-only v1. It leaves music/ambient requiring huge uncompressed assets; compressed support
is essentially free with miniaudio.

### D6. Memory vs streaming load modes

`AudioClip` declares a load mode: `InMemory` (fully decoded, low-latency, best for short SFX) or `Streaming`
(decoded on demand from disk, best for music/ambient). Both modes are exposed in the asset metadata and honored by
the backend (`ma_sound` with `MA_SOUND_FLAG_STREAM` vs decoded in memory).

### D7. Asset pipeline

Audio source files (`.wav`/`.mp3`/`.flac`/`.ogg`) are imported into an `AudioClip` asset that records metadata
(duration, channels, sample rate, load mode, default bus, loop) **and the encoded source bytes** (`AssetRawData`,
binary `BinLoad`/`BinSave`). Embedding the bytes keeps packaged builds self-contained: the runtime decodes from
memory (`ma_decoder_init_memory`) with no loose-file dependency.

**Resolved**: the builder is a dedicated `AudioBuilder` (target `AudioBuilder.Static` under
`engine/audio/builder`), registered by the active `Aurora.Cook` module's `Init`; the legacy `SkyRender.Builder`
path is not touched. This was chosen over extending `RenderBuilder.Static` to keep the audio build step
independent and avoid the migrating legacy render-builder path.

### D8. 3D spatialization

`AudioSource` exposes `spatialBlend` (0 = 2D, 1 = 3D), a min/max distance and attenuation curve, and optional
doppler. `AudioListener` holds the world transform (typically the active camera). Position is taken from the
owning actor transform each frame via `AudioSystem::Tick`. Backend maps these onto miniaudio's spatialization
(`ma_sound_set_position`/`set_attenuation_model`/`set_doppler_factor`).

### D9a. Headless verification

`MinAudioEngine` accepts a `useNullBackend` flag that forces miniaudio's null device, so a gtest suite can
exercise device init, bus volume, embedded-clip decode, playback state, and 3D spatialization without hardware or
a window. `AudioRegistry` is exercised both directly and through `ModuleManager` (which loads `AudioModule` with
no dependencies), covering the runtime module-loading path without the renderer.

### D9. Threading model

The device runs its own audio thread (miniaudio owns it). Main-thread gameplay calls (`Play`, volume, position)
are forwarded to the backend, whose public sound/group/node operations are thread-safe; clip loading/decoding is
done during asset load, not from the audio callback. No engine taskflow job is used for audio output.

Rejected alternative: driving mixing from the engine frame/taskflow loop. It adds jitter and couples audio
liveness to frame rate.

### D10. Null-backend contract

With no audio plugin loaded, `AudioRegistry::Get()` remains constructible and every `Create*` returns `nullptr`
(exactly like `PhysicsRegistry`), and `AudioEngine`-dependent calls are no-ops. Games therefore run silently
instead of crashing, and CI/headless builds can build audio-disabled.

### D11. Build and runtime switches

- `plugins/plugins.json`: `{ "name": "audio", "dir": "audio", "cmake_var": "SKY_BUILD_AUDIO", "enabled": true }`.
- `plugins/audio/plugin.json`: `targets: [{ name: "AudioModule", dependencies: ["Launcher", "Editor"] }]`.
- `cmake/thirdparty.json`: `miniaudio` entry, `header_only: true`.
- `cmake/thirdparty/Findminiaudio.cmake`: `sky_3rd_header_only(miniaudio)`.
- `configs/modules_game.json` / `configs/modules_editor.json` and the builtin `engine/configs/` copies: add
  `AudioModule` (game runtime; editor only when the plugin is built).

## Risks / Trade-offs

- [miniaudio is a large single-header; including it widely slows builds] -> include/compile it in exactly one `.cpp`
  inside `plugins/audio` and keep it out of public headers (guaranteed by D2).
- [Audio thread vs main thread data races on sources/buses] -> restrict public API to operations miniaudio
  documents as thread-safe; create/destroy sounds only on the main thread; no locking in the audio callback.
- [Device loss / format unsupported on a platform] -> `AudioEngine::Init` returns a status; on failure the module
  stays loaded but audio degrades to the null behavior (silent), never crashes.
- [Android/iOS backend availability or permissions] -> miniaudio uses AAudio/OpenSL ES and CoreAudio; verify on
  device; first iteration may validate desktop fully and flag mobile verification as follow-up.
- [Asset-builder integration is the least-defined part] -> keep the runtime asset contract in the spec and resolve
  the builder target during implementation (Open Question 1); a minimal `AudioBuilder` fallback can convert
  sources without touching the render builder.
- [Future Wwise/FMOD backend may need concepts (events, banks) not in this model] -> the interface intentionally
  covers low-level playback/bus/3D only; middleware-specific behavior can arrive via backend-specific extension
  interfaces or ADDED capabilities without breaking the base API.
- [Bus count/name conventions could ossify] -> start with `master`/`music`/`sfx` defaults but allow arbitrary named
  buses created by the game.

## Migration Plan

1. Land the interface module and tests with the null backend first (audio silent, nothing breaks).
2. Add the miniaudio third-party entry + Find module, then the `plugins/audio` backend behind `SKY_BUILD_AUDIO`
   (default in `plugins.json`, but the module is only loaded when listed in `modules_*.json`).
3. Add `AudioModule` to game/editor module configs and validate on desktop.
4. Add the asset import path and wire a sample clip for manual verification.

Rollback: set `SKY_BUILD_AUDIO=OFF` and remove `AudioModule` from the module JSONs; the interface module compiles
independently and the null-backend contract keeps the engine functional.

## Open Questions

- ~~Asset-builder integration~~ **Resolved**: dedicated `AudioBuilder` + `AudioBuilder.Static`, registered by
  `Aurora.Cook`. Audio source formats are imported as-is (no transcoding); load mode defaults to `InMemory`.
- Whether compressed runtime assets should stay as original MP3/OGG/FLAC or be transcoded to one canonical format,
  and whether a decode-cache is needed.
- Bus persistence/authoring: are buses defined in a config asset, in code, or both?
- Editor support depth in this change (inspector for `AudioSourceComponent` only, vs an audio panel).
- Mobile verification scope (which of Android/iOS must be validated before archive).
- Whether an optional software fallback (null device) is needed for headless CI.
