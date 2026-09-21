## ADDED Requirements

### Requirement: ACL2 third-party packaging
ACL v2 SHALL be provided as a header-only third-party package built by `python/third_party.py`, exposing a CMake target that makes both the `acl` and bundled `rtm` headers available, without vendoring ACL sources under `engine/`. The package build SHALL NOT require ACL's unit tests, benchmark, or compressor tools.

#### Scenario: Package builds header-only
- **WHEN** the third-party bootstrap builds the `acl` package for a supported platform
- **THEN** it SHALL install the `acl` and `rtm` headers and expose them through one CMake target, without compiling ACL tests or tools

#### Scenario: No vendored copy
- **WHEN** the repository is inspected
- **THEN** no ACL sources SHALL exist under `engine/`

### Requirement: Optional ACL dependency
The ACL integration SHALL be gated by a build option that is enabled by default. When disabled, the animation module SHALL still build, compression entry points SHALL report failure, and decompression SHALL be a no-op.

#### Scenario: Disabled build
- **WHEN** the ACL option is off
- **THEN** the `Animation` module SHALL build without an ACL dependency and compression SHALL report failure

#### Scenario: Enabled build
- **WHEN** the ACL option is on and the package is present
- **THEN** the animation module SHALL link the ACL target and compression SHALL succeed

### Requirement: Cook-time clip compression API
The animation module SHALL expose a cook-time compressor that converts sparse clip tracks into a compressed clip at a given frame rate, bounded by a configurable per-track error threshold, and SHALL preserve the clip duration. The compressor SHALL NOT be required at runtime.

#### Scenario: Compress sparse tracks
- **WHEN** sparse clip tracks with a known last key time are compressed at a frame rate
- **THEN** the compressor SHALL produce a valid compressed clip whose duration equals the frame count divided by the frame rate

#### Scenario: Bounded error
- **WHEN** a compressed clip is decompressed at each authored frame
- **THEN** the reconstructed translation SHALL stay within the configured error tolerance of the source and the rotation SHALL stay aligned

#### Scenario: Deterministic output
- **WHEN** the same tracks and settings are compressed twice
- **THEN** the produced blobs SHALL be identical

### Requirement: Runtime decompression path
The runtime clip SHALL decompress the stored blob through ACL decompression only, write the result into the engine transform layout, and SHALL be transferable as a pointer-free blob with its metadata.

#### Scenario: Sample decoded pose
- **WHEN** a compressed clip is sampled at a time within its duration
- **THEN** it SHALL write one transform per bone and return success

#### Scenario: Time is clamped
- **WHEN** a sample time outside `[0, duration]` is requested
- **THEN** the sample time SHALL be clamped into range

#### Scenario: Blob transfer
- **WHEN** a compressed clip's blob and metadata are copied into another clip
- **THEN** sampling both SHALL produce the same pose
