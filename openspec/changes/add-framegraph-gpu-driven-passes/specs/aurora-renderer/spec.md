## ADDED Requirements

### Requirement: Feature persistent buffer creation

The feature preparation context SHALL allow a render feature to create device-owned persistent buffers from a checked buffer descriptor, without exposing the device or queue submission.

#### Scenario: Create in prepare

- **WHEN** a feature prepares a frame and requests a persistent buffer
- **THEN** the context SHALL create the buffer on the active device and return a `BufferPtr` owned by the feature

#### Scenario: Feature ownership and release

- **WHEN** the feature retires
- **THEN** it SHALL release its persistent buffers after every frame that imported them has completed

#### Scenario: No device escape

- **WHEN** a feature uses the preparation or build context
- **THEN** it SHALL NOT obtain a mutable device, create a queue submission, or retain a borrowed context past its callback

#### Scenario: Invalid descriptor

- **WHEN** a feature requests a persistent buffer with an unsupported usage or size
- **THEN** the context SHALL return an explicit frame error without creating a native resource

### Requirement: Feature external buffer import

The feature build context SHALL allow importing a feature-owned persistent buffer with explicit initial and final states and an initialization declaration.

#### Scenario: Import a persistent buffer

- **WHEN** a feature imports its persistent buffer into a frame graph
- **THEN** the graph SHALL reference-count the buffer, validate its state and contents, and release its reference after submission completion

#### Scenario: Use imported buffer in compute and raster

- **WHEN** compute writes an imported buffer and a later raster pass reads it
- **THEN** the graph SHALL synchronize the write before the read without exposing native barriers to the feature

#### Scenario: Import without preparation ownership

- **WHEN** a buffer is imported that the feature does not own or that belongs to another device
- **THEN** the graph SHALL reject the import with an explicit error
