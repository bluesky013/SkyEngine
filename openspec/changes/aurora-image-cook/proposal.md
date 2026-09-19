## Why

Aurora has a runtime texture path (`sky::aurora::ImageAssetData` v1 + `CreateTextureFromAsset`) but
**no offline cooker**: nothing turns source images into aurora products. The only working image
builder is legacy `engine/render/builder/render/ImageBuilder` (stb decode + Kaiser mip + ispc BC7),
and it emits the **legacy** `sky::ImageAssetData` (`ASSET_TYPE = "Texture"`), not aurora's
(`"AuroraTexture"`). `AssetTraits<aurora::Texture>` exists in the adaptor but is never produced.

Two codecs are needed: **ASTC** for mobile (`tex_mobile`) and **BC** for desktop (`tex_pc`).
`astc-encoder` is now integrated at tag 5.7.0 (`cmake/patches/astc.patch` adds the missing static
library/header install rules, `cmake/thirdparty/Findastc.cmake` exports `3rdParty::astc`). Note the
encoder has no RDO feature (only an alpha-weight "RDO-like" constant-block heuristic). BC via
`ispc_texcomp` works as a runtime DLL (only BC7 is wired). Source `ktx` input has no third-party
dependency at all (`Findktx.cmake` exists but is never called; all KTX code is commented out).

## What Changes

- **New aurora cook module** `engine/aurora/cook` (mirrors `engine/render/builder`):
  - `AuroraCook.Static` — image cook logic + an `AssetBuilder` implementation.
  - `Aurora.Cook` (SHARED module, `REGISTER_MODULE`) — registers the builder with
    `AssetBuilderManager`; built under `SKY_BUILD_TOOL` on Win32/macOS.
- **`AuroraImageBuilder`** (`AssetBuilder`): extensions `.png` / `.jpg` / `.jpeg` / `.hdr` / `.ktx`
  / `.image`; `QueryType` → `AssetTraits<aurora::Texture>::ASSET_TYPE` (`"AuroraTexture"`); output
  is an aurora `ImageAssetData` (version 1) written through `AssetManager::SaveAsset`.
- **Encode pipeline**: decode source → optional max-size resize → optional linearize → mip chain
  (Kaiser/Box, ported from legacy) → per-bundle compression → flatten into `slices` + `rawData`.
  Supports 2D, cube (6 layers) and 2D array; alpha/sRGB aware.
- **ASTC via astc-encoder 5.x static library**: add `cmake/thirdparty/Findastc.cmake` +
  `3rdParty::astc`, declare/link in the cook target; ASTC 4x4/8x8 UNORM + SRGB.
- **BC via ispc_texcomp** (kept, per request): BC7 UNORM/SRGB through the existing runtime-DLL
  compressor path.
- **Minimal built-in KTX reader**: parse KTX1 and KTX2 (no supercompression) headers + mip/layer
  tables, passing block-compressed payloads through untouched; no libktx dependency.
- **Per-target format selection driven by bundle/preset**: bundle → format map (`tex_pc` → BC7,
  `tex_mobile` → ASTC, default → uncompressed RGBA8) loaded from a cook config; the builder writes
  to the matching product bundle.
- **Framework fix**: `AssetManager::GetBundle` currently returns the first **non-matching** bundle
  (`!=` instead of `==`), so named targets (`tex_pc` / `tex_mobile`) are broken; fix it so multi
  bundle output works.

## Capabilities

### New Capabilities
- `aurora-cook`: the aurora offline cook module, the `AuroraImageBuilder` contract, the
  bundle/preset-driven image format policy, ASTC/BC codec integration, and KTX source support.

### Modified Capabilities
- (none)

## Impact

- New: `engine/aurora/cook/` (`CMakeLists.txt`, `image/` static lib, `module/AuroraCookModule.cpp`,
  `test/`).
- Modified: `engine/aurora/CMakeLists.txt` (add `cook` under `SKY_BUILD_TOOL`),
  `engine/editor/src/application/EditorApplication.cpp` (load `Aurora.Cook` when built).
- Modified: `cmake/thirdparty/Findastc.cmake` (new), `cmake/thirdparty.json` (astc install
  verification), `cmake/thirdparty/Findispc_texcomp.cmake` (reused as-is).
- Modified: `engine/framework/src/asset/AssetManager.cpp` (`GetBundle` fix).
- New config: `configs/image_build_presets.json` (bundle → format), mirroring
  `configs/asset_build_presets.json`.
- Legacy `engine/render/builder/render/image/**` is the **porting source**, not a dependency
  (aurora must not depend on `engine/render`).
- Third-party bootstrap must rebuild with the new/updated `astc` package before the cook target can
  link.
