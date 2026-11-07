## ADDED Requirements

### Requirement: GPU particle simulation

The subsystem SHALL simulate GPU emitters with fixed-capacity persistent state, spawn/simulate/compact passes and GPU-produced indirect arguments.

#### Scenario: Persistent state

- **WHEN** a GPU emitter completes a frame
- **THEN** its particle state SHALL remain in persistent device buffers for the next frame

#### Scenario: Double-buffered simulation

- **WHEN** a simulate pass updates particles
- **THEN** it SHALL read one state buffer and write another without aliasing

#### Scenario: Compaction

- **WHEN** simulation completes
- **THEN** compaction SHALL produce the alive count and indirect draw arguments on the GPU

#### Scenario: No render readback

- **WHEN** a GPU emitter renders
- **THEN** rendering SHALL NOT require a CPU readback of particle state

### Requirement: GPU capability fallback

The subsystem SHALL detect missing GPU capabilities and fall back explicitly.

#### Scenario: Missing compute or indirect support

- **WHEN** required compute, storage or indirect features are unavailable
- **THEN** the emitter SHALL fall back to CPU simulation or an explicit supported path and report the fallback

#### Scenario: Unsafe multi-frame state

- **WHEN** multiple frames are in flight and state rotation is unavailable
- **THEN** GPU submissions SHALL be serialized rather than aliasing live state

### Requirement: GPU validation

GPU particle behavior SHALL be validated with readback tests on supported backends.

#### Scenario: Spawn and simulate readback

- **WHEN** a test dispatches spawn and simulate for a known emitter
- **THEN** readback SHALL confirm the expected count and attributes

#### Scenario: Indirect arguments

- **WHEN** compaction produces indirect arguments
- **THEN** readback SHALL confirm the expected draw count and shape

#### Scenario: Unavailable platform

- **WHEN** a backend cannot execute GPU particles
- **THEN** the validation record SHALL mark it unavailable rather than passed
