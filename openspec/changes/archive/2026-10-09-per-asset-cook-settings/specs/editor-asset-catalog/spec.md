## ADDED Requirements

### Requirement: Per-asset cook override read and write

Building on the catalog's effective cook configuration inspection, `EditorAssetCatalog` SHALL expose, for an
asset and target, the **effective** reflected settings object plus the global-preset baseline object and the
builder-declared settings type (`GetCookSettings`), and SHALL persist an edited reflected object by
computing the sparse override against the bundle preset and writing the manifest `cook.settings[target]`
block (`ApplyCookSettings`). Persistence SHALL occur only for assets on a writable mount, SHALL be a no-op
otherwise, and SHALL notify observers. The catalog SHALL NOT auto-trigger a cook on mutation.

#### Scenario: Provide effective settings plus baseline

- **WHEN** the catalog is asked for an asset's cook settings for a target
- **THEN** it returns a reflected effective object, a reflected preset baseline, and the settings type

#### Scenario: Persist an edit as a sparse override

- **WHEN** an edited reflected object that differs from the preset only in `maxSize` is applied
- **THEN** the manifest stores a sparse `maxSize` override for that target and observers are notified

#### Scenario: Reset removes the override

- **WHEN** an object equal to the preset is applied
- **THEN** the target's override key is removed (empty override clears the target block)

#### Scenario: Read-only asset not editable

- **WHEN** cook settings are applied to an asset on a read-only mount
- **THEN** the manifest is unchanged
