## ADDED Requirements

### Requirement: Shared source document checkpoint

Source asset documents SHALL share a canonical saved-checkpoint lifecycle while retaining their domain codecs and validation. Successful reload SHALL clear old history and pending saves. Failed load/save SHALL preserve valid current data and the last successful checkpoint. Save SHALL require a successful load, and successful save SHALL clear stale errors.

#### Scenario: Undo to loaded source

- **WHEN** a source with non-canonical whitespace or omitted defaults is loaded, edited and undone
- **THEN** the restored semantic value is clean

#### Scenario: Failed save and retry

- **WHEN** writing fails and a later explicit retry succeeds
- **THEN** the failed write preserves dirty work, the retry updates the checkpoint and clears the error, and only the successful write requests cook

### Requirement: Shared asset source storage

Asset document storage SHALL resolve source paths through AssetDataBase, bound reads before allocation and replace saved files atomically. Errors SHALL use FileOperationStatus; a failed read SHALL preserve the output and a failed replacement SHALL preserve the previous source and remove owned staging files.

#### Scenario: Source exceeds its domain limit

- **WHEN** source storage encounters a file larger than the supplied limit
- **THEN** it fails before allocating the file content and leaves the output unchanged
