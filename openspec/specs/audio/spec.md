# audio Specification

## Purpose

Define audio data, backend behavior, assets and spatial playback.

## Requirements

### Requirement: Listener represents the audio viewpoint

The audio API SHALL provide an `AudioListener` whose position and orientation are updated each frame from a world
transform (typically the active camera), and the backend SHALL spatialize 3D sources relative to that listener.

#### Scenario: Listener updates from camera transform

- **WHEN** the active camera transform changes during a frame
- **THEN** the listener position and orientation SHALL be updated before 3D sources are mixed

#### Scenario: No listener produces no spatialization error

- **WHEN** a `World` has 3D sources but no listener attached
- **THEN** playback SHALL continue without crashing and 3D sources SHALL be treated as non-spatialized

### Requirement: Per-source spatial blend

Each `AudioSource` SHALL expose a spatial blend in the range `[0, 1]`, where 0 is fully 2D (no spatialization) and
1 is fully 3D (attenuation, panning, and doppler apply).

#### Scenario: 2D source is unaffected by listener

- **WHEN** a source has spatial blend 0
- **THEN** its output level SHALL NOT depend on the listener position

#### Scenario: 3D source pans with listener

- **WHEN** a source has spatial blend 1 and the listener moves to one side of it
- **THEN** the source SHALL pan toward the listener and its level SHALL follow the attenuation curve

### Requirement: Distance attenuation

A 3D `AudioSource` SHALL expose a minimum distance, a maximum distance, and an attenuation curve, and the backend
SHALL apply the resulting gain based on listener-source distance.

#### Scenario: Inside minimum distance is full volume

- **WHEN** the listener is within the source's minimum distance
- **THEN** the source SHALL play at its full configured volume

#### Scenario: Beyond maximum distance is silent

- **WHEN** the listener is beyond the source's maximum distance
- **THEN** the source SHALL be inaudible

#### Scenario: Curve controls falloff between the bounds

- **WHEN** the listener is between the minimum and maximum distance
- **THEN** the gain SHALL follow the configured attenuation curve

### Requirement: Doppler effect

A 3D `AudioSource` SHALL support an optional doppler factor that shifts pitch based on relative velocity between
the source and the listener.

#### Scenario: Doppler disabled by default

- **WHEN** a source is created without enabling doppler
- **THEN** its pitch SHALL NOT change due to relative motion

#### Scenario: Approaching source rises in pitch

- **WHEN** doppler is enabled and a source moves toward a stationary listener
- **THEN** the perceived pitch SHALL rise relative to a stationary source

### Requirement: Audio clip asset

The asset system SHALL define an audio clip asset that references audio data and carries metadata: duration,
channel count, sample rate, load mode, default bus, and loop default. Audio clips SHALL load through the existing
asset loading path.

#### Scenario: Clip asset carries metadata

- **WHEN** an audio clip asset is loaded
- **THEN** its duration, channel count, sample rate, load mode, default bus, and loop default SHALL be available
  to the runtime

#### Scenario: Clip loads through the asset system

- **WHEN** an audio clip is requested by asset reference
- **THEN** it SHALL be resolved and loaded through the same asset loading mechanism as other engine assets

### Requirement: Supported source formats

The audio import path SHALL accept WAV, MP3, FLAC, and OGG Vorbis source files and SHALL reject unsupported formats
explicitly.

#### Scenario: Supported formats import

- **WHEN** a WAV, MP3, FLAC, or OGG Vorbis file is imported
- **THEN** the import SHALL produce a valid audio clip asset

#### Scenario: Unsupported format is rejected

- **WHEN** a file with an unsupported extension is imported as audio
- **THEN** the import SHALL fail with an explicit error rather than producing a broken asset

### Requirement: Memory and streaming load modes

An audio clip SHALL support an `InMemory` load mode (fully decoded for low-latency playback) and a `Streaming` load
mode (decoded on demand for long audio), selected through asset metadata and honored at playback.

#### Scenario: In-memory clip plays after load

- **WHEN** a clip with `InMemory` mode is loaded
- **THEN** it SHALL be immediately playable without further disk reads

#### Scenario: Streaming clip reads on demand

- **WHEN** a clip with `Streaming` mode is played
- **THEN** its audio data SHALL be read incrementally rather than fully resident in memory at load time

#### Scenario: Load mode is selectable per asset

- **WHEN** a clip asset declares a load mode
- **THEN** playback SHALL use that mode regardless of clip length

### Requirement: Runtime file resolution

Audio clip assets SHALL be resolvable both from loose files during development and from the packaged asset bundle in
a shipped build.

#### Scenario: Development loose file

- **WHEN** an audio clip asset references a source that exists on disk in development
- **THEN** it SHALL load successfully

#### Scenario: Packaged build

- **WHEN** the application runs from a packaged build
- **THEN** the audio clip asset SHALL resolve from the asset bundle

### Requirement: miniaudio is registered as a third-party dependency

The third-party bootstrap SHALL declare `miniaudio` as a header-only package in `cmake/thirdparty.json`, provide a
`cmake/thirdparty/Findminiaudio.cmake` module that creates the `3rdParty::miniaudio` interface target, and the audio
backend SHALL link that target.

#### Scenario: Find module resolves the dependency

- **WHEN** `sky_find_3rd(TARGET miniaudio DIR miniaudio)` is invoked during configuration
- **THEN** the `3rdParty::miniaudio` target SHALL exist with the package include directory

#### Scenario: Missing package fails clearly

- **WHEN** the miniaudio package is not present under `3RD_PATH`
- **THEN** configuration SHALL fail with a message naming the missing dependency

### Requirement: Audio backend plugin and module switch

The audio backend SHALL be a SHARED `AudioModule` target under `plugins/audio`, gated by the `SKY_BUILD_AUDIO`
switch declared in `plugins/plugins.json`, declaring its `plugin.json` dependencies, and registering its
`AudioRegistry::Impl` through `REGISTER_MODULE`.

#### Scenario: Plugin toggles with the build switch

- **WHEN** `SKY_BUILD_AUDIO` is ON
- **THEN** `AudioModule` SHALL be built and placed in the shared runtime output directory

#### Scenario: Plugin disabled

- **WHEN** `SKY_BUILD_AUDIO` is OFF
- **THEN** `plugins/audio` SHALL NOT be added to the build and the engine SHALL retain the null-backend behavior

#### Scenario: Module registers its backend

- **WHEN** `AudioModule` starts
- **THEN** it SHALL register its factory with `AudioRegistry` and reflect the audio components

#### Scenario: Module unregisters on shutdown

- **WHEN** `AudioModule` shuts down
- **THEN** it SHALL unregister its factory and release audio device resources

### Requirement: Runtime module configuration

`AudioModule` SHALL be listed in the runtime module configuration so it loads with the application, with a
dependency ordering that does not break existing modules.

#### Scenario: Game runtime loads audio

- **WHEN** the game application starts with `AudioModule` listed in `configs/modules_game.json`
- **THEN** the module SHALL be loaded and its backend registered before audio sources are played

#### Scenario: Editor runtime honors the switch

- **WHEN** the editor starts with the audio plugin built
- **THEN** `AudioModule` SHALL be listed in `configs/modules_editor.json` and load accordingly

### Requirement: miniaudio is confined to the backend plugin

The miniaudio header SHALL be included and compiled in the backend plugin only, and SHALL NOT be included from any
public `engine/audio` header or other engine module.

#### Scenario: Single translation unit compiles miniaudio

- **WHEN** the backend plugin is built
- **THEN** miniaudio implementation macros SHALL be defined in exactly one backend translation unit

#### Scenario: Dependents do not need miniaudio

- **WHEN** an engine module links the `Audio` target
- **THEN** it SHALL NOT require the miniaudio include directory to compile

### Requirement: Backend capabilities

The miniaudio backend SHALL implement the `AudioRegistry::Impl` factory, providing device output, clip decode for
the supported formats, bus mixing through sound groups, and 3D spatialization.

#### Scenario: Backend decodes supported formats

- **WHEN** a clip in WAV, MP3, FLAC, or OGG Vorbis is loaded through the backend
- **THEN** the backend SHALL decode it successfully

#### Scenario: Backend applies bus volume

- **WHEN** a bus volume is changed through the engine API
- **THEN** the backend SHALL apply the gain on the corresponding mixer group

#### Scenario: Backend spatializes 3D sources

- **WHEN** a 3D source position and listener are updated
- **THEN** the backend SHALL produce output with the corresponding attenuation and panning

### Requirement: Device loss and supported-platform backends

The backend SHALL select the platform audio API at runtime and SHALL handle device failure without aborting the
process.

#### Scenario: Platform backend selected

- **WHEN** the engine runs on a supported platform
- **THEN** the backend SHALL use that platform's API (WASAPI/DirectSound on Win32, CoreAudio on Apple, ALSA/
  PulseAudio on Linux, AAudio/OpenSL ES on Android)

#### Scenario: Device failure does not abort

- **WHEN** the audio device becomes unavailable
- **THEN** the backend SHALL report failure and the application SHALL continue running

### Requirement: Audio interface module and registry

`engine/audio` SHALL provide a STATIC `Audio` module exposing a backend-agnostic audio API, and an `AudioRegistry`
singleton with a nested abstract `Impl` factory that creates the backend runtime objects. No backend-specific type
SHALL appear in a public `engine/audio` header.

#### Scenario: Audio module builds independently of any backend

- **WHEN** the `Audio` target is built without any audio backend plugin
- **THEN** it SHALL compile and link successfully with only `Framework` as its dependency

#### Scenario: Backend registers its factory

- **WHEN** an audio backend module starts and calls `AudioRegistry::Register` with its `Impl`
- **THEN** subsequent `AudioRegistry` `Create*` calls SHALL be served by that backend

#### Scenario: No backend types leak into the public API

- **WHEN** a consumer includes only public `engine/audio` headers
- **THEN** the compilation SHALL NOT require any backend library header to be present

### Requirement: Null-backend contract

With no audio backend registered, the `AudioRegistry` SHALL remain usable and SHALL not crash: factory `Create*`
calls SHALL return `nullptr` and dependent operations SHALL be no-ops.

#### Scenario: Registry without a backend

- **WHEN** `AudioRegistry::CreateAudioEngine()` is called with no backend registered
- **THEN** it SHALL return `nullptr` instead of crashing

#### Scenario: Audio-disabled build stays functional

- **WHEN** the engine runs with no audio module loaded
- **THEN** the application SHALL run normally and produce no audio output

#### Scenario: Backend unregisters on shutdown

- **WHEN** the audio backend module shuts down and calls `UnRegister`
- **THEN** subsequent `Create*` calls SHALL fall back to the null behavior

### Requirement: Audio engine owns device lifecycle

The `AudioEngine` SHALL initialize and shut down the audio device, own the master bus and clip cache, and report an
initialization status so callers can detect device failure without crashing.

#### Scenario: Successful initialization

- **WHEN** `AudioEngine::Init` is called on a system with a working audio device
- **THEN** it SHALL return success and be ready to play clips

#### Scenario: Device failure degrades gracefully

- **WHEN** the audio device cannot be opened
- **THEN** `AudioEngine::Init` SHALL return a failure status and the engine SHALL remain usable without audio

#### Scenario: Shutdown releases the device

- **WHEN** `AudioEngine::Shutdown` is called
- **THEN** all playing sources SHALL be stopped and the device resources SHALL be released

### Requirement: Audio clips and sources

The audio API SHALL provide an `AudioClip` resource and an `AudioSource` playback instance with volume, pitch,
loop, bus assignment, `Play`/`Pause`/`Stop`, and `IsPlaying` state.

#### Scenario: Play a clip

- **WHEN** a source bound to a clip is started
- **THEN** `IsPlaying` SHALL report true and the clip SHALL be audible on its assigned bus

#### Scenario: Stop a playing source

- **WHEN** `AudioSource::Stop` is called on a playing source
- **THEN** playback SHALL cease and `IsPlaying` SHALL report false

#### Scenario: Looping source

- **WHEN** a source with loop enabled reaches the end of its clip
- **THEN** it SHALL continue playing from the beginning without external retriggering

#### Scenario: Volume and pitch are applied

- **WHEN** a source's volume or pitch is set while playing
- **THEN** the change SHALL affect the output without requiring the source to be recreated

### Requirement: Bus graph with master and named buses

The audio API SHALL provide named `AudioBus` mixer nodes with a volume and an optional parent, rooted at a master
bus, and bus volume SHALL multiply down the chain.

#### Scenario: Master volume scales children

- **WHEN** the master bus volume is reduced
- **THEN** the effective output of every descendant bus SHALL be scaled accordingly

#### Scenario: Bus volume zero mutes subtree

- **WHEN** a bus volume is set to zero
- **THEN** all sources routed to that bus and its descendants SHALL be silent

#### Scenario: Sources are routed to a named bus

- **WHEN** a source is assigned to the `music` bus
- **THEN** its output SHALL be affected by the `music` bus and master bus volumes

### Requirement: World subsystem and components

`AudioSystem` SHALL implement `IWorldSubSystem` with `NAME = "Audio"`, SHALL attach to a `World` via
`World::AddSubSystem`, and SHALL tick every frame to update sources and the listener from actor transforms.
`AudioSourceComponent` and `AudioListenerComponent` SHALL be reflected components.

#### Scenario: Subsystem attaches to the world

- **WHEN** an `AudioSystem` is added to a `World` and the world ticks
- **THEN** the audio subsystem SHALL be ticked as part of the world update order

#### Scenario: Source component follows its actor

- **WHEN** an actor carrying an `AudioSourceComponent` moves
- **THEN** the associated source position SHALL be updated from the actor transform during the audio tick

#### Scenario: Components are reflected

- **WHEN** the audio module starts and reflects its components
- **THEN** `AudioSourceComponent` and `AudioListenerComponent` SHALL be available to the serialization/component
  system

### Requirement: Thread-safe main-thread control

Public audio operations invoked from the main thread (play/stop, volume, pitch, position, bus volume) SHALL be safe
with respect to the backend audio thread, and clip loading SHALL NOT occur on the audio callback.

#### Scenario: Concurrent volume change while playing

- **WHEN** the main thread changes a source volume while the audio thread is mixing
- **THEN** the operation SHALL NOT corrupt playback state or crash

#### Scenario: Loading does not run on the audio thread

- **WHEN** a clip is loaded
- **THEN** decoding/asset loading SHALL happen off the audio callback thread
