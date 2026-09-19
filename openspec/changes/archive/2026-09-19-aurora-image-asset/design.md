## Context

`aurora::Texture` already has type-specific subclasses — `Texture2D` (`IMAGE_2D`, `arrayLayers = 1`), `Texture2DArray` (`IMAGE_2D`, `arrayLayers = N`), `Texture3D` (`IMAGE_3D`, `extent.z = depth`) — and `Texture::UploadImage` takes `ImageUploadRequest{source, offset, size, mipLevel, layer, imageExtent}`. The CPU asset side (`ImageAssetData`) is a 2D-only placeholder with no depth, type, slice table or version.

Legacy render solved this in `render/adaptor/assets/ImageAsset.h`: `ImageAssetHeader{version, format, type, width/height/depth, mipLevels, arrayLayers, slices[]}` + `ImageAssetData{rawData}` + `CreateTextureFromAsset`.

## Goals / Non-Goals

**Goals:**
- A versioned `ImageAssetData` able to describe 2D, 2D-array and 3D images.
- Explicit addressing of mip/layer/depth slices.
- A build path `CreateTextureFromAsset(Device*, Asset<Texture>)` that produces the right `aurora::Texture` subclass.

**Non-Goals:**
- Offline build / `AssetBuilderManager` products (still runtime-load only).
- mip generation or compression codecs.
- Changing `AssetTraits<aurora::Texture>` (`"AuroraTexture"`, BIN).

## Decisions

### D1. Explicit `ImageAssetType` discriminator

Add `enum class ImageAssetType : uint32_t { TEXTURE_2D, TEXTURE_2D_ARRAY, TEXTURE_3D, TEXTURE_CUBE }`. Do not infer the type from `arrayLayers` alone (3D uses `depth`, 2D-array and cube use `arrayLayers`, and both could be > 1). Legacy used an explicit `type`.

Mapping to aurora resources:

| ImageAssetType | Image::Descriptor | Texture subclass |
|---|---|---|
| `TEXTURE_2D` | `IMAGE_2D`, `arrayLayers = 1` | `Texture2D` |
| `TEXTURE_2D_ARRAY` | `IMAGE_2D`, `arrayLayers = N` | `Texture2DArray` |
| `TEXTURE_3D` | `IMAGE_3D`, `extent = {w, h, depth}` | `Texture3D` |
| `TEXTURE_CUBE` | `IMAGE_2D`, `arrayLayers = 6`, `viewUsage = CUBE_MAP_COMPATIBLE` | `TextureCube` |

`TEXTURE_CUBE` requires `arrayLayers == 6`; its slices carry `layer = 0..5`.

### D2. Versioned header

`ImageAssetData` gains `static constexpr uint32_t CURRENT_VERSION = 1;` and a `version` field written first. `Load` reads `version` and returns early (leaving the data empty) when it differs from `CURRENT_VERSION`; the failure is logged. This makes future layout changes detectable instead of silently mis-parsed.

### D3. Slice table + raw blob

Mirror legacy:

```cpp
struct ImageSliceHeader {
    uint32_t offset   = 0;  // byte offset into rawData
    uint32_t size     = 0;
    uint32_t mipLevel = 0;
    uint32_t layer    = 0;  // array layer (2D array)
    uint32_t depth    = 0;  // z slice (3D)
};
struct ImageAssetData {
    uint32_t version = CURRENT_VERSION;
    PixelFormat format = PixelFormat::UNDEFINED;
    ImageAssetType type = ImageAssetType::TEXTURE_2D;
    uint32_t width = 1, height = 1, depth = 1, mipLevels = 1, arrayLayers = 1;
    std::vector<ImageSliceHeader> slices;
    std::vector<uint8_t> rawData;
};
```

The slice table lets payloads be non-contiguous or per-subresource (e.g. one slice per mip/layer), instead of assuming one tightly-packed mip-0 block.

### D4. `CreateTextureFromAsset`

New `CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset)`:

1. Build `Image::Descriptor` per D1 (`usage = SAMPLED | TRANSFER_DST`, `memory = GPU_ONLY`).
2. Allocate the matching `Texture` subclass and `Init(device, desc)`.
3. For each `ImageSliceHeader`, issue `Texture::UploadImage({ImageUploadRequest{rawData[offset..offset+size], mipLevel, layer, imageExtent}})`.

Cube uses the `TextureCube` path (`arrayLayers = 6` + `viewUsage = CUBE_MAP_COMPATIBLE`); a `TEXTURE_2D` with `arrayLayers == 6` is NOT auto-treated as cube.

### D5. Backward incompatibility

The new layout is not compatible with the current placeholder bytes. The version guard (D2) makes old data fail cleanly. No shipped `AuroraTexture` assets exist yet.

## Risks / Trade-offs

- **Slice table verbosity**: an extra per-slice record; acceptable and matches legacy, and is required for 3D/array correctness.
- **`ImageUploadRequest` extent semantics**: `imageExtent` is per-slice; the builder must supply the mip-appropriate extent or the full extent consistently. First cut uses the full `width/height` (and `depth` for 3D).
- **`PixelFormat` round-trip**: stored as `uint32_t`; `Load` casts back and validates against `PixelFormat::UNDEFINED`.
- **No `CreateTextureFromAsset` call site yet**: the `StaticMeshComponent`/material path does not consume textures yet; the function is exercised by a unit test.

## Open Questions

- Should the builder auto-derive per-mip extents from the base extent, or require the writer to record them? (Current: derive by halving.)
- Cube arrays (`TEXTURE_CUBE_ARRAY`, `arrayLayers = 6 * N`) — not covered yet; `TEXTURE_CUBE` is exactly 6 layers.
