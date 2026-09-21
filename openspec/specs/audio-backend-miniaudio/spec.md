# audio-backend-miniaudio Specification

## Purpose
TBD - created by archiving change add-audio-subsystem. Update Purpose after archive.
## Requirements
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

