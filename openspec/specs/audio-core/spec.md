# audio-core Specification

## Purpose
TBD - created by archiving change add-audio-subsystem. Update Purpose after archive.
## Requirements
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

