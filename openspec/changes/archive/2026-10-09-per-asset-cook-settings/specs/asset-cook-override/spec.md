## ADDED Requirements

### Requirement: Manifest per-asset cook override schema

The source manifest (`assets.jsonl`) entry SHALL support an optional `cook` object with a `targets` array
(listing the product bundles to cook for the asset) and a `settings` object keyed by **target/bundle**,
where each value is a **flat**, possibly **sparse** object of scalar settings (string/number/bool). A key
absent from `settings[target]` SHALL fall back to the global per-bundle preset. Non-scalar values and
`settings` keys that are not cooked targets SHALL be ignored.

#### Scenario: Parse targets and per-target settings

- **WHEN** an entry's `cook` block is `{"targets":["common","tex_mobile"],"settings":{"tex_mobile":{"maxSize":512,"generateMip":false}}}`
- **THEN** the target set is `["common","tex_mobile"]` and the `tex_mobile` override is `{maxSize:512, generateMip:false}`

#### Scenario: Sparse override falls back to the global preset

- **WHEN** a target override sets only `encode`
- **THEN** every other key (srgb/quality/block/maxSize/generateMip) is taken from the global preset for that bundle

#### Scenario: Ignore invalid entries

- **WHEN** a `settings` value is an object/array, or is keyed by a target absent from the cooked target set
- **THEN** the value is ignored and does not affect resolution

### Requirement: Effective cook settings resolution

The effective settings for an asset and target SHALL be the global builder preset for the target's bundle
overlaid key-by-key with `cook.settings[target]`. The effective target set SHALL remain: asset `targets` if
present, else project targets, else the active platform's preset bundles. The framework SHALL resolve this
and pass the flattened per-target override to the builder through the build request; builders SHALL NOT read
the manifest.

#### Scenario: Override wins over the global preset

- **WHEN** the `tex_mobile` global preset is ASTC 4x4 with mips and the asset override sets `generateMip=false`
- **THEN** the effective `tex_mobile` config has `generateMip=false` and the global ASTC 4x4 encode

#### Scenario: Unset key uses the global preset

- **WHEN** an asset has a `tex_pc` override that sets only `quality`
- **THEN** the effective `tex_pc` `encode`/`srgb`/`block`/`maxSize`/`generateMip` equal the global `tex_pc` preset values

#### Scenario: Asset targets override project targets

- **WHEN** the manifest lists `targets:["tex_mobile"]` and the project declares other targets
- **THEN** the asset is cooked only for `tex_mobile`

#### Scenario: Override travels with the build request

- **WHEN** the framework builds asset `X` for target `T`
- **THEN** `AssetBuildRequest.settings` carries `X`'s resolved `T` override, for both the in-process and the worker cook path

### Requirement: Manifest write API for cook overrides

`AssetDataBase` SHALL provide a setter that replaces the `cook` block of a writable asset's manifest entry
while preserving the entry's `file` and `id` (and any unrelated fields), and SHALL reject the write for a
read-only mount. Writing SHALL persist to the manifest and invalidate the manifest cache.

#### Scenario: Persist an override and reload

- **WHEN** a writable asset's `cook` block is set and the manifest cache is invalidated
- **THEN** reloading returns the written `cook` block with `file`/`id` unchanged

#### Scenario: Read-only mount rejects the write

- **WHEN** a `cook` write targets an asset on a read-only mount
- **THEN** the write is rejected and the manifest is unchanged

### Requirement: Builder-declared reflected settings type

An `AssetBuilder` SHALL declare a reflected cook-settings type via `GetSettingsType()` (a type registered
with the process-wide `SerializationContext`), SHALL materialize the effective settings (preset overlaid
with the sparse override) into a reflected `Any` via `MakeSettings(bundle, override)`, and SHALL compute the
sparse override that reproduces an edited settings object via `DiffSettings(bundle, edited)`. A builder
without a settings type SHALL expose none. The editor SHALL inspect and edit these settings **only** through
the generic reflected form (`ReflectedFormView` bound to `PropertyObject{obj, node}`), never through
hand-rolled per-field controls.

#### Scenario: Image builder declares its settings type

- **WHEN** the image builder's settings type is queried
- **THEN** it is a reflected type whose members are `encode` (enum NONE/BC7/ASTC), `srgb` (bool), `quality` (enum), `block` (int), `maxSize` (int), `generateMip` (bool)

#### Scenario: Effective settings reflect the override

- **WHEN** `MakeSettings(bundle, override)` is called with an override that changes `maxSize`
- **THEN** the returned reflected object has the override `maxSize` and the preset values for the other members

#### Scenario: Diff yields only changed keys

- **WHEN** `DiffSettings(bundle, edited)` is called with an object differing only in `maxSize`
- **THEN** the result contains only `maxSize`, so the persisted override is sparse
