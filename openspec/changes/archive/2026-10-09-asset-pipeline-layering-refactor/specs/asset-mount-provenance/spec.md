## ADDED Requirements

### Requirement: Ordered mount list

The asset database SHALL expose its source mounts as an ordered list, each carrying a stable id, a
display name, and a writability flag, in the same order used to resolve logical paths (earlier mounts
shadow later ones).

#### Scenario: Mounts exposed in order
- **WHEN** the database's mounts are queried
- **THEN** it SHALL return the mounts in resolution order with id, display name, and writable flag

#### Scenario: Single source of order
- **WHEN** a mount is added or removed
- **THEN** both path resolution and the exposed mount list SHALL reflect the same order

### Requirement: Per-source owning mount

The asset database SHALL record each source's owning mount id at registration and SHALL expose it, so
consumers do not re-derive provenance by probing filesystems.

#### Scenario: Owning mount reported
- **WHEN** a source is registered from a given mount
- **THEN** its owning mount id SHALL be reported and SHALL match the mount that contains the file

#### Scenario: No filesystem probing by consumers
- **WHEN** a consumer needs a source's provenance
- **THEN** it SHALL obtain it from the recorded owning mount, not by testing each filesystem

### Requirement: Single-pass all-mount scan

The source rebuild scan SHALL iterate the mount list and perform one recursive discovery pass per
mount, testing builder-known extensions during the walk (not one full walk per extension).

#### Scenario: One walk per mount
- **WHEN** the source cache is rebuilt
- **THEN** each mount SHALL be walked once and files of builder-known extensions SHALL be registered

#### Scenario: Workspace shadowing
- **WHEN** the same logical path exists in the writable mount and a later read-only mount
- **THEN** the writable mount's source SHALL win

### Requirement: Read-only identity without writes

Sources discovered in a read-only mount SHALL receive stable identity derived from their logical path
and SHALL NOT cause any manifest write; writable mounts SHALL read and write their directory manifests
as before.

#### Scenario: Read-only not written
- **WHEN** a source is registered from a read-only mount
- **THEN** no manifest file SHALL be created or modified and its identity SHALL be stable across scans

#### Scenario: Writable keeps manifest identity
- **WHEN** a source is registered in the writable mount
- **THEN** its identity SHALL come from (and be persisted to) the directory `assets.jsonl`
