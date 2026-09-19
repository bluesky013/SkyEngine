## Context

Offline asset building is driven by `AssetBuilderManager` + the `AssetBuilder` interface
(`Request` / `GetExtensions` / `QueryType` / `LoadConfig` / `RequireImportSetting` / `Import`).
`AssetDataBase::RegisterAsset(path, build=true)` turns a source path into an `AssetBuildRequest`
and enqueues it on `AssetExecutor`. The only working image producer is legacy
`engine/render/builder/render/ImageBuilder` + `image/{ImageMipGen,ImageResizer,ImageConverter,ImageCompressor}`,
which emits the legacy `sky::ImageAssetData` (`ASSET_TYPE = "Texture"`) using legacy `rhi::PixelFormat`.

Aurora's asset is `sky::aurora::ImageAssetData` (v1) with `ImageAssetType` (2D/2DArray/3D/Cube), a
`depth` field in `ImageSliceHeader`, and a version guard; its runtime format is
`sky::aurora::PixelFormat`, and block geometry/uncompressed size helpers already exist
(`GetImageFormatInfo`, `GetImageRowPitch`, `GetImageSlicePitch` in `aurora/rhi/Core.h`). Aurora must
not depend on `engine/render`, so the image-processing helpers are ported, not linked.

Third-party state:

| Need | State |
|---|---|
| stb (png/jpg decode) | installed, header-only `3rdParty::stb`, `stb_image.h` only (no resize) |
| ispc_texcomp (BC7/ASTC) | installed, runtime DLL `3rdParty::ispc_texcomp`; only BC7 wired |
| astc-encoder 5.7.0 | integrated (5.7.0 tag, install patch + `Findastc.cmake`, `3rdParty::astc`); no RDO feature exists in this encoder |
| libktx | not declared; `Findktx.cmake` unused; all KTX code commented |

Two facts constrain the design:
1. `AssetManager::GetBundle` returns the first **non-matching** bundle (`!=` should be `==`), so
   saving to `tex_pc` / `tex_mobile` is broken today.
2. `AssetBuildRequest::target` is empty for editor `RegisterAsset` builds, so a builder must be able
   to resolve its output bundle itself.

## Goals / Non-Goals

**Goals:**
- A dedicated aurora cook module that plugs into the existing builder and emits aurora
  `ImageAssetData`.
- Source decode (png/jpg/jpeg/hdr/ktx), optional resize, mip generation, and ASTC/BC compression.
- Per-bundle (platform) output format selection driven by presets.
- Integrate `astc-encoder`; keep `ispc_texcomp` for BC; no libktx dependency.

**Non-Goals:**
- Mesh / material / lod cook (later changes).
- A standalone cook CLI executable and CI packaging (deferred; see D1).
- GPU-side mip generation; DDS output; BC1/BC3/BC4/BC5/BC6H and ETC (later; BC7 first).
- KTX supercompression (zstd/basis) or write support; KTX is read-only.

## Decisions

### D1. Cook module shape: static core + shared module, not an exe

Mirror `engine/render/builder`:

- `engine/aurora/cook/image/` → `AuroraCook.Static` (STATIC) — all decode/process/encode/builder
  logic, unit-testable without an app.
- `engine/aurora/cook/module/AuroraCookModule.cpp` → `Aurora.Cook` (SHARED, `REGISTER_MODULE`)
  — thin `IModule::Init` that calls `AuroraReflection` (idempotent) and
  `AssetBuilderManager::RegisterBuilder(new AuroraImageBuilder())`.
- Built only when `SKY_BUILD_TOOL` on Win32/macOS (`engine/render/CMakeLists.txt` precedent),
  selected from `engine/aurora/CMakeLists.txt`.

Rationale: the builder is a plugin into an in-process, async (`AssetExecutor`) pipeline that the
editor already drives; a shared module matches that and reuses the app boot path. A standalone exe
was evaluated and deferred: it would need its own `Application` (like `ShaderTool`), workspace FS
wiring, and duplicate module/config loading, for no benefit until headless CI cooking is required.
The split into a STATIC core keeps the exe a thin future shell (the dead `tools/asset_builder` is
the cautionary example of an exe coupled to editor-only APIs).

### D2. ASTC: link astc-encoder as a static library, not astcenc.exe

| Aspect | astc-encoder static lib (chosen) | astcenc.exe subprocess |
|---|---|---|
| Integration | in-process, called from `AssetExecutor` workers | spawn per image/mip |
| Perf | no process/file overhead; existing thread pool | process start + temp file I/O per call |
| Data flow | RGBA bytes in memory → block bytes | must write an intermediate image file, read back |
| Build | needs `Findastc.cmake` + ISA selection (below) | needs an exe artifact + locate/ship logic |
| Error handling | return codes / logs | exit codes, stdout parsing |
| Failure mode | link/target missing at build time | exe missing at runtime |

Chosen: **static library**. The exe route is rejected for v1 because the cook runs inside the
editor process and the ASTC call is naturally in-memory. Concrete integration:

- `cmake/thirdparty/Findastc.cmake` → `3rdParty::astc` via `sky_3rd_static`, include dir
  `include` (astcenc.h), libs selected per host ISA; astc-encoder 5.x builds one library per ISA
  (`astcenc-enc-<isa>`, `<isa>` ∈ `avx2`/`sse4.1`/`sse2`/`neon`/`native`).
- `sky_find_3rd(TARGET astc DIR astc)` + `3rdParty::astc` in `AuroraCook.Static`.
- Configure `ASTCENC_CLI=OFF` (already), `ASTCENC_SHAREDLIB=OFF`, `ASTCENC_STATICLIB=ON`,
  `ASTCENC_INVARIANCE=ON`, `ASTCENC_DECOMPRESSOR=OFF`; the first task verifies
  `python/third_party.py -p Win32 -t astc` actually installs (`build_3rd/Win32/astc` is currently
  empty) and adds a patch/`install` fix if not.
- API: `astcenc_config_init` / `astcenc_context_alloc` / `astcenc_compress_image` /
  `astcenc_compress_reset`; profiles `ASTCENC_PRE_FAST`/`MEDIUM`/`THOROUGH` map to a `Quality`.

### D3. BC: keep ispc_texcomp (runtime DLL)

Per request, BC stays on `ispc_texcomp`; the cook links `3rdParty::ispc_texcomp` and reuses the
existing `DynamicModule("ispc_texcomp")` + `CompressBlocksBC7` path. v1 targets **BC7 UNORM + SRGB**
(alpha-aware). BC1/BC3/BC4/BC5/BC6H are declared but not dispatched in v1.

### D4. KTX: minimal built-in reader, no libktx

`KtxReader` parses in-module:

- **KTX1** header (`«KTX 11»`), `glInternalFormat` → aurora `PixelFormat`, `pixelDepth`,
  `numberOfFaces`, `numberOfMipmapLevels`, `numberOfArrayElements`, and per-level `imageSize`.
- **KTX2** header (`«KTX 20»`), `vkFormat` → aurora `PixelFormat`; **reject** when
  `supercompressionScheme != 0` (no zstd/basis) with a clear log.
- Both: derive type = cube when `faces == 6`, 2D array when `arrayElements > 1`, else 2D; 3D when
  `pixelDepth > 1`. Block-compressed payloads (BC/ETC/ASTC) are copied verbatim into slices; only
  `format` metadata is translated, never re-decoded.

Rationale: libktx is a large dependency (transcoding, Basis, zstd) this pipeline does not need, and
a KTX input already carries its final GPU format. A minimal reader keeps the dependency graph small.

### D5. Per-bundle format selection + the `GetBundle` fix

- New `configs/image_build_presets.json`:
  ```json
  { "bundles": {
      "tex_pc":     { "format": "BC7",  "quality": "FAST", "maxSize": 2048, "generateMip": true },
      "tex_mobile": { "format": "ASTC", "block": "4x4", "quality": "MEDIUM", "maxSize": 1024, "generateMip": true } },
    "defaultBundle": "tex_pc" }
  ```
- `AuroraImageBuilder::LoadConfig` reads it into a `bundle → ImageBuildConfig` map.
- `Request` resolves the target bundle: use `request.target` when it is a known bundle, else
  `defaultBundle`; encode accordingly and `SaveAsset(asset, resolvedBundle)`.
- Fix `AssetManager::GetBundle` (`!=` → `==`) so `SaveAsset` lands in the requested bundle; without
  this, `tex_pc`/`tex_mobile` both write to the first bundle.

Alternative — a single global format — rejected: it cannot express the BC-desktop / ASTC-mobile
split the preset system already models.

### D6. Port image-processing helpers into the aurora cook module

`ImageMipGen` (Kaiser/Box polyphase), `ImageResizer`, `ImageConverter` (gamma), `ImageFilter`
kernels, and the `ImageObject`/`CompressedImage` containers are ported from
`engine/render/builder/render/{include,src}/.../image/` and converted to
`sky::aurora::PixelFormat` (using `GetImageFormatInfo` for block geometry instead of legacy
`rhi::GetImageInfoByFormat`). Namespace: `sky::aurora::cook`. No `engine/render` dependency.

Two correctness fixes over the legacy source (verified by tests):

- **Polyphase tap alignment**: the legacy kernel computed one shared weight row at offsets
  `k - windowSize/2` but applied it starting at `left = floor(center - width)`, which shifted
  every tap by up to half a pixel (an exact 2x box downscale of `[0,1,2,3]` produced
  `[1.5, ...]` instead of `[0.5, 2.5]`). The port builds normalized weights per destination
  sample from the true distance `(tap - center)`, so the kernel is correctly centered.
- **16-bit filtering**: the kernel read path had no `PixelType::HALF` case (fell through to 0,
  making R16/RGBA16 sources filter to black). `HalfToFloat`/`FloatToHalf` are now shared
  `sky::aurora::cook` helpers used by both the pixel helpers and the kernel.

Known inherited approximations kept as-is: clamp-to-edge boundary handling only; `FloatToHalf`
truncates (no round-to-nearest) and flushes subnormals, relevant only if a HALF intermediate is
ever produced (the pipeline uses U8 / float).

### D7. Output asset mapping

`AuroraImageBuilder` fills `sky::aurora::ImageAssetData`:
`version = CURRENT_VERSION`, `format`, `type`, `width/height/depth`, `mipLevels`, `arrayLayers`,
`ImageSliceHeader{offset,size,mipLevel,mipLevel,layer,depth}` per level/layer, and `rawData`. Slice
size uses `GetImageSlicePitch`-equivalent block math. Then `FindOrCreateAsset<Texture>(uuid)` +
`Data() = imageData` + `SaveAsset(asset, bundle)`.

### D8. Module wiring

- `engine/aurora/CMakeLists.txt`: `if (SKY_BUILD_TOOL AND (Darwin OR Windows)) add_subdirectory(cook)`.
- `Aurora.Cook` links `Aurora.Adaptor` (asset traits/reflection) + `AuroraCook.Static` +
  `3rdParty::stb` / `3rdParty::ispc_texcomp` / `3rdParty::astc`.
- `sky_add_dependency(TARGET Aurora.Cook DEPENDENCIES Launcher Editor)` (mirror render builder).
- `EditorApplication::LoadConfigs`: when `SKY_BUILD_TOOL`, register
  `Aurora.Cook` (dependency `AuroraRender.Editor`), alongside the legacy `SkyRender.Builder`
  entry.

## Risks / Trade-offs

- **astc-encoder install is currently broken** (`build_3rd/Win32/astc` empty) → first task verifies
  and, if needed, patches the package/install; the cook target is blocked until it links.
- **ISA selection** for the ASTC static lib can cause illegal-instruction faults if a too-new ISA is
  chosen → pick `sse2`/`sse4.1` baseline or gate `avx2` on host capability; document the choice.
- **KTX2 supercompression** unsupported → explicit reject + log, no silent corruption.
- **GetBundle fix** changes bundle selection behavior for any existing named target → audit current
  callers (all pass empty target today, so behavior is unchanged for them).
- **Ported image code drift** vs legacy → keep field/enum names aligned so future mesh/material cook
  can share, and cover mip/resize/encode with unit tests.
- **Memory**: cube/array + full mip chains are large; process level-by-level and move (not copy)
  `ImageMipData` buffers into `rawData`.

## Migration Plan

Additive module + one small framework bug fix. Rollback = remove `add_subdirectory(cook)`, the
editor entry, and revert the one-line `GetBundle` fix; no runtime asset format changes
(`ImageAssetData` v1 is untouched).

## Open Questions

- Should `common` carry an uncompressed RGBA8 fallback texture, or only platform bundles exist?
- ASTC block size per bundle: fixed 4x4, or configurable (4x4/6x6/8x8) — v1 plans 4x4 default with
  config override.
- Does the future headless cook exe share `AuroraCook.Static` as-is (intended), and does it need the
  intermediate FS getter that `AssetBuilderManager` currently lacks?
