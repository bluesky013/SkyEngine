# Aurora Shader Design & Legacy Migration

Status: living document. Source of truth for the new Aurora shader pipeline and the
migration away from the legacy HLSL/`.tech` pipeline.

## 1. Language & modules

Aurora shaders are authored in **Slang** (`.slang`). There is **no HLSL** in `engine/aurora/`.

| Target | Kind | Path | Responsibility |
|---|---|---|---|
| `Aurora.Core.Static` | STATIC | `engine/aurora/core` | slang-free shader definition/parse/cache/variant/`RgBlockDesc`/resolver |
| `Aurora.ShaderCompiler.Static` | STATIC | `engine/aurora/shader` | Slang compiler impl (`ShaderCompilerSlang`) |
| `AuroraShaderCompiler` | SHARED (module) | `engine/aurora/shader` | registers the compiler into `ShaderCompilerFactory` |
| `ShaderHeaderTool` / `ShaderCookTool` | EXE | `engine/aurora/shader/tool` | offline codegen / usage-cache precompile |
| `Aurora.Pipeline.Static` | STATIC | `engine/aurora/pipeline` | pass templates, global resources (`pipeline/src/ReflectionValidation.cpp`) |

Shader source root convention (`engine/aurora/shader/CMakeLists.txt`): `global/` → set 0 (GlobalRenderResources),
`pipeline/` → set 1 (PipelinePass), `batch/` → set 2 (per-draw dynamic UBO).

## 2. Authoring model

- Resources are declared with **`[[vk::binding(set, binding)]] ParameterBlock<T>`**
  (see `engine/aurora/shader/assets/shaders/slang/global/GlobalBlock.slang`).
- Variant keys are authored either as a `@variant` comment block
  (`.../config/pipeline_variants.slang`, parsed by `ShaderVariantGen::Parse`) or in a
  per-shader sibling **`.slang.json`** companion (`ShaderCompanion.h`, `ShaderCompanion.cpp`):
  `sources[{name,width}]`, `variants[{key,source,width,default,spec}]`, `vertex[{name,semantics[]}]`,
  `entries[]`, `depends[]`.
- Strong variants (macros) fold at compile; weak variants (specialization constants) go to
  native specialization (SPIR-V/MSL) or DXIL folding.
- Includes resolve via `ShaderFileSystem` → `SlangFileSystemAdapter` (virtual includes; generated
  headers need not be on disk).
- Compile targets (`ShaderCompilerSlang.cpp`): SPIRV `spirv_1_5`, MSL `metal`, DXIL `sm_6_5`;
  target macros `AURORA_TARGET_SPIRV/MSL/DXIL`.

## 3. Identity, resolve, cache

- A technique references a shader via **`ShaderRef { relativePath, entry }`**
  (`engine/aurora/core/include/aurora/shader/ShaderRef.h`). There is **no `stages` field**;
  `stage` is a per-request input to `ShaderResolver`.
- Resolve priority: **offline cache → source compile → local cache** (`ShaderResolver`).
- Cache: content-addressed `<root>/<target>/{index.bin, blobs/<compileHash>}`,
  `target ∈ {vulkan, metal, d3d12}` (`ShaderCacheStore.h`).
- `ShaderUsageCollector` → `shader_usage.json` → `ShaderCookTool` precompiles the shipped cache.

## 4. Material technique model (new)

Material source `assets/materials/*.mat` (JSON, loaded/saved through the reflection codec
`MaterialAssetData` — `engine/aurora/adaptor/src/MaterialReflection.cpp`):

```json
{
  "techniques": [
    { "tag": "ForwardColor",
      "shader": { "relativePath": "standard_pbr.hlsl", "entry": "FSMain" },
      "renderState": { "depthStencil": { "depthTest": true, "depthWrite": true },
                       "rasterState": { "cullMode": 2 } } }
  ],
  "properties": [
    { "name": "Metallic", "type": 0, "value": { "index": 0, "value": 0.5 } },
    { "name": "AlbedoMap", "type": 5, "value": { "index": 8, "value": "textures/white.png" } }
  ]
}
```

- Loaded/written by the **hand-written JSON codecs** on `MaterialAssetData` / `MaterialInstanceSource`
  (`MaterialReflection.cpp`, via `LoadValueObject` / `SaveValueObject`); BIN codecs live on the data
  structs. `type` is `MaterialPropertyType` (= `RgFieldType`); `value.index` selects the `std::variant`
  alternative (`float` / `float2` / `float3` / `float4` / `mat4` / `int` / `uint` / `bool` / texture).
  Sampler state is **not** a standalone material parameter (per glTF / Unreal / Unity / Godot it lives
  on the texture); there is no `SAMPLER` property in the source.
- Asset payload: `MaterialTechniqueData { tag; ShaderRef shader; MaterialRenderState renderState; }`
  (`engine/aurora/adaptor/include/aurora/adaptor/assets/MaterialAsset.h`).
- Runtime: `aurora::Material` / `MaterialTechnique` (`engine/aurora/core/include/aurora/resource/Material.h`);
  `MaterialPropertyType = RgFieldType` (`RgBlockDesc.h`).
- The technique `tag` buckets RDG scene-raster queues; material→PSO is deferred to `aurora-material-pso`.

## 5. Legacy pipeline (removed)

- The legacy HLSL pipeline (`.hlsl`/`.hlslh` + `.tech`) and its DXC/glslang compiler were removed together with
  `engine/render/**` and `engine/shader` by `openspec/changes/legacy-render-thirdparty-cleanup/`.
- Any remaining `.hlsl`/`.tech` files under `assets/` are unconsumed data and may be deleted separately.
- Aurora uses the Slang-based compiler in `engine/aurora/shader`.

## 6. Migration map (legacy → aurora)

| Legacy | Aurora |
|---|---|
| `assets/shaders/*.hlsl` | `.slang` modules under the aurora shader source root |
| `#include "x.hlslh"` | Slang `#include` (virtual includes; generated headers via `ShaderFileSystem`) |
| `#pragma option({...})` variant macros | `.slang.json` `variants` / `@variant` |
| `.tech` `shader.path` | material `techniques[].shader` (now `.slang`) |
| `.tech` staged entries (`vertex/task/mesh/fragment`) | `ShaderRef.entry` (single entry; stage is per-request) |
| `.tech` `pass.tag` | `techniques[].tag` |
| `.tech` `depth_stencil` / `raster_state` / `blend_state` | `techniques[].renderState` (superset) |
| `.tech` `vertex_options` | shader variant/define (companion `variants`, not the material) |
| `.technique` reference from material | inline `techniques[]` in `.mat` |

**Dangling legacy refs to resolve during migration**: `unlit.hlsl`, `volumes/volume_simple.hlsl`
(referenced by `assets/materials/*.mat` but absent from `assets/shaders/`).

## 7. Migration steps

1. Add the `.slang` module (+ sibling `.slang.json` if it has variants / vertex schema / entries).
2. Run `ShaderHeaderTool` to emit the C++ mirror / `RgBlockDesc` where a block layout is needed.
3. Update the material `.mat` `techniques[].shader` to the `.slang` relative path; keep `tag`,
   `entry`, `renderState`.
4. Verify via `Aurora.ShaderTest` (slang compile/reflection) and the per-backend spike tests.
5. Delete the legacy `.hlsl`/`.hlslh`/`.tech` once no consumer remains.

## 8. Open gaps (blockers for full migration)

- No production module mounts the aurora shader source root or constructs a `ShaderResolver` yet.
- `ShaderRef` has no `stages`; multi-stage legacy entries collapse to one `entry`.
- Technique/PSO cache (`TechniqueKey`) reserved but unimplemented (`aurora-material-pso`).
- No production `.slang` graphics technique shaders exist yet.
