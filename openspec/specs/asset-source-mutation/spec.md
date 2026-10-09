# asset-source-mutation Specification

## Purpose
TBD - created by archiving change asset-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Move and rename preserve identity

Moving or renaming a source asset SHALL keep its UUID unchanged by relocating the corresponding manifest entry to the destination directory, and MUST NOT modify any asset references.

#### Scenario: Rename in the same directory

- **WHEN** a source asset is renamed in place
- **THEN** its manifest entry SHALL be rewritten with the new filename and the same UUID

#### Scenario: Move to another directory

- **WHEN** a source asset is moved to a different directory within the writable mount
- **THEN** the old directory manifest entry SHALL be removed, the new directory manifest SHALL record the same UUID, and existing references SHALL continue to resolve

### Requirement: Delete removes identity

Deleting a source asset SHALL remove its manifest entry and its identity-map record; a manifest with no remaining entries SHALL be removed. Products generated from the deleted source are not removed by this operation and are reclaimed by a later build.

#### Scenario: Delete cleans identity

- **WHEN** a source asset is deleted
- **THEN** its file, manifest entry, and identity-map record SHALL all be removed

### Requirement: Duplicate assigns a new identity

Duplicating a source asset SHALL create the new file with a newly generated UUID and leave the original asset and its references unchanged. Two different source paths MUST NOT share a UUID in the manifests.

#### Scenario: Duplicate does not alias identity

- **WHEN** a source asset is duplicated
- **THEN** the duplicate SHALL have a distinct UUID and the original's UUID SHALL be unchanged

