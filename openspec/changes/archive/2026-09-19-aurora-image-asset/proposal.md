## Why

Aurora's `ImageAssetData` (`engine/aurora/adaptor/include/aurora/adaptor/assets/ImageAsset.h`) is a placeholder: it only stores `width`/`height` (no depth), has no image-type discriminator, no per-mip/per-layer slice table, and no format version. It cannot represent a 3D texture or a 2D array, and there is no way to evolve the on-disk layout safely. Legacy render already solved this with `ImageAssetHeader{version, format, type, width/height/depth, mipLevels, arrayLayers, slices[]}` + a `CreateTextureFromAsset` builder.

## What Changes

- Extend `ImageAssetData` to a versioned, type-tagged payload:
  - `version` header with a `CURRENT_VERSION` constant; `Load` rejects mismatched versions.
  - `ImageAssetType` discriminator: `TEXTURE_2D` / `TEXTURE_2D_ARRAY` / `TEXTURE_3D` / `TEXTURE_CUBE`.
  - `width` / `height` / `depth` / `mipLevels` / `arrayLayers` / `format` (`PixelFormat`).
  - A slice table (`ImageSliceHeader{offset, size, mipLevel, layer/depth}`) plus a raw data blob, so multi-mip / multi-layer / 3D / cube payloads can be addressed explicitly.
- Add `CreateTextureFromAsset(Device*, const Asset<Texture>&)` that maps the header to an `aurora::Image::Descriptor` and uploads slices, selecting `Texture2D` / `Texture2DArray` / `Texture3D` / `TextureCube`.
- Update `ImageAssetData::Save` / `Load` for the new layout (backward-incompatible with the current placeholder; **BREAKING** for any existing `AuroraTexture` asset bytes).
- Keep `AssetTraits<aurora::Texture>` (`ASSET_TYPE = "AuroraTexture"`, `SERIALIZE_TYPE = BIN`).

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `aurora-adaptor`: the runtime asset layer requirement for images is expanded from a fixed 2D placeholder to versioned 2D / 2D-array / 3D payloads with a slice table and a `CreateTextureFromAsset` build path.

## Impact

- Affected code: `engine/aurora/adaptor/include/aurora/adaptor/assets/ImageAsset.h` (+ optional `src/ImageAsset.cpp` for `CreateTextureFromAsset`).
- Reuses `aurora::Texture` subclasses (`Texture2D` / `Texture2DArray` / `Texture3D`) and `Device::CreateImage` / `Texture::UploadImage`.
- No RHI/backend change. Asset bytes produced by the previous placeholder layout are not loadable (version guard fails); no shipped assets depend on it yet.
