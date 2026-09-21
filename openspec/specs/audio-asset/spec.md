# audio-asset Specification

## Purpose
TBD - created by archiving change add-audio-subsystem. Update Purpose after archive.
## Requirements
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

