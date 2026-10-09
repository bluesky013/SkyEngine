## ADDED Requirements

### Requirement: Per-asset image cook override

`AuroraImageBuilder` SHALL apply the resolved per-asset override (from `AssetBuildRequest.settings`) over
the bundle config it resolves from `configs/image_build_presets.json`, for the keys `encode`, `srgb`,
`quality`, `block`, `maxSize`, and `generateMip`, before decoding/processing. `DescribeSettings(bundle,
override)` SHALL report the effective (global overlaid with override) values. An unrecognized override key
SHALL be ignored.

#### Scenario: Override resolution limit and mip generation

- **WHEN** the `tex_mobile` bundle is ASTC with `maxSize=1024`/`generateMip=true` and the asset override sets `maxSize=512`/`generateMip=false`
- **THEN** the produced texture is ASTC 4x4 (per the bundle), clamped to 512, with a single mip

#### Scenario: Override encode and block

- **WHEN** an asset's `tex_pc` override sets `encode=BC7` and `quality=SLOW`
- **THEN** the `tex_pc` product is encoded as BC7 at the SLOW quality

#### Scenario: Unset keys use the bundle preset

- **WHEN** an asset override sets only `srgb=false`
- **THEN** the effective encode/quality/block/maxSize/generateMip are the bundle preset values and `srgb=false` is applied

#### Scenario: ASTC block 6x6

- **WHEN** an asset's target override sets `encode=ASTC` and `block=6`
- **THEN** the product format is `ASTC_6x6` (6x6 is a supported block size alongside 4 and 8)
