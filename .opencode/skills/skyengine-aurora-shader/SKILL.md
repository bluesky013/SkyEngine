---
name: skyengine-aurora-shader
description: Author and migrate SkyEngine Aurora shaders (Slang). Use when writing/porting shaders, wiring material techniques to shaders, or migrating legacy HLSL/.tech to the aurora slang pipeline.
---

# SkyEngine Aurora Shader (Slang)

Authoritative design + migration map: `docs/aurora/shader-design-and-migration.md`.
This skill is the quick standard; read the doc for full detail.

## Hard rules

- **Language is Slang (`.slang`).** Do not add HLSL under `engine/aurora/`. Legacy `assets/shaders/*.hlsl`
  + `assets/techniques/*.tech` are deprecated (removed with `legacy-render-thirdparty-cleanup`).
- **Bindings**: declare with `[[vk::binding(set, binding)]] ParameterBlock<T>`.
  Sets: `0` = GlobalRenderResources, `1` = PipelinePass, `2` = per-draw batch dynamic UBO.
  Single source of truth for set 0 is `engine/aurora/shader/assets/shaders/slang/global/GlobalBlock.slang`
  (reflected to a C++ mirror by `ShaderHeaderTool`).
- **Variants** are authored as a `@variant` comment block or in a sibling `<name>.slang.json`
  (`sources`/`variants`/`vertex`/`entries`/`depends`). Strong variants fold at compile;
  weak variants use native specialization. Do **not** use legacy `#pragma option({...})`.
- **Includes** resolve through `ShaderFileSystem` (virtual); generated headers need not be on disk.
- **Shader identity** is `ShaderRef { relativePath, entry }` — no Uuid, no `stages` field.
  `stage` is a per-request input to `ShaderResolver`. Legacy multi-stage entries
  (`vertex/task/mesh/fragment`) collapse to a single `entry`.
- **Compile targets**: SPIRV (`spirv_1_5`), MSL (`metal`), DXIL (`sm_6_5`); target macros
  `AURORA_TARGET_SPIRV` / `AURORA_TARGET_MSL` / `AURORA_TARGET_DXIL`.
- **Resolve order**: offline cache → source compile → local cache. Cache is content-addressed
  `<root>/<target>/{index.bin, blobs/<compileHash>}`.

## Material technique → shader

- Material `assets/materials/*.mat` `techniques[]` entries: `{ tag, shader:"<.slang path>", entry, renderState }`.
- `renderState` keys (see `MaterialSourceParse.cpp`): `depthTest, depthWrite, stencilTest, depthCompare,
  stencilFail, stencilPass, cullMode, frontFace, polygonMode, depthBias{constant,slope,clamp},
  alphaToCoverage, blend`. `cullMode`/`frontFace`/`polygonMode` names are case-insensitive.
- `tag` buckets RDG scene-raster queues. Material→PSO is `aurora-material-pso` (not this pipeline's concern yet).

## Migration checklist (legacy → aurora)

1. Port the `.hlsl` to a `.slang` module; port needed `.hlslh` includes to slang (or generated headers).
2. Move `#pragma option({...})` macros → `.slang.json` `variants` (or `@variant`).
3. Add `<name>.slang.json` when the shader has variants / vertex schema / multiple entries.
4. Run `ShaderHeaderTool` to emit the C++ mirror / `RgBlockDesc` for block layouts.
5. Update the material `.mat` `techniques[].shader` to the `.slang` relative path (keep `tag`/`entry`/`renderState`).
6. Verify with `Aurora.ShaderTest` (slang compile/reflection + per-backend spike tests).
7. Delete legacy `.hlsl`/`.hlslh`/`.tech` when no consumer remains.

## Blockers (see doc §8)

No production code mounts the aurora shader source root / builds a `ShaderResolver` yet; `ShaderRef`
has no stages; technique/PSO cache unimplemented; no production `.slang` technique shaders exist yet.
