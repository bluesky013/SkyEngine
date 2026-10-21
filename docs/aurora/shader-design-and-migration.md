# Aurora Shader Design & Legacy Migration

Status: living document. Source of truth for the new Aurora shader pipeline and the
migration away from the legacy HLSL/`.tech` pipeline.

## 1. Language & modules

Aurora shader implementations are authored in **Slang** (`.slang`); Pass declarations live in shared `.h` sources consumed by C++ and Slang.

| Target | Kind | Path | Responsibility |
|---|---|---|---|
| `Aurora.Core.Static` | STATIC | `engine/aurora/core` | slang-free shader definition/parse/cache/variant/`RgBlockDesc`/resolver |
| `Aurora.ShaderCompiler.Static` | STATIC | `engine/aurora/shader` | Slang compiler impl (`ShaderCompilerSlang`) |
| `AuroraShaderCompiler` | SHARED (module) | `engine/aurora/shader` | registers the compiler into `ShaderCompilerFactory` |
| `ShaderHeaderTool` / `ShaderCookTool` | EXE | `engine/aurora/shader/tool` | variant schema generation / usage-cache precompile |
| `Aurora.Pipeline.Static` | STATIC | `engine/aurora/pipeline` | pass templates, global resources (`pipeline/src/ReflectionValidation.cpp`) |

Resource tiers: set 0 = GlobalRenderResources, set 1 = PipelinePass,
set 2 = Batch. Dynamic offsets follow explicit binding types.

The resource contract remains Global / Pass / Batch. View data is optional and
declared by the consuming pass; it is not a replacement resource tier. Each pass
owns one shared `.h` declaration source. Native C++ types and resource metadata
are expanded from the same lists as shader declarations; shader reflection validates
the result. See [Shader System](../features/shader-system.md) for authoring rules.

The production Renderer now invokes registered pass features through
`FrameGraphBuildContext`. `PreparedRaster` resolves geometry, exact resource
versions and owned material bytes; it does not choose parameter layouts.
`RendererRuntime` validates declarations rather than a fixed three-UBO ABI.
The change and platform validation are tracked by
`openspec/changes/fix-aurora-pass-resource-contract`.

Global and Pass/Batch declarations live in `engine/aurora/pipeline/shaders/*.h`.
Pipeline C++ compilation needs no shader compiler or header generator. CMake embeds
these files and the common helper as strings for production virtual includes.
`SceneRenderFeature` uses ScenePass for optional camera input and SceneBatch for
tint/world transform; Lighting/Shadow/Environment/ToneMap own their additional inputs.
Other passes can use different layouts or omit resource tiers. Opaque remains a placeholder.

Shared headers expose typed slots and expected resource/block metadata.
`PassResourceBindings` validates actual compiled shader use before replacing draw
bindings. Unsupported shapes fail explicitly; static/dynamic uniform selection is
explicit. Shadow/IBL/HDR and bounded float4 arrays are implemented, while nested
uniform structures remain unsupported. Native Metal validation remains a platform gate.

Launcher and the Windows Editor explicitly register the scene pass using
`RegisterScenePass` in their first packet, with a host-selected feature ID and
the defining module lease. Adaptor links Pipeline; Core does not link it.
There is no implicit scene raster when the feature is absent or disabled.
Frame/view constants use lease allocations; reflected layouts and shader objects
may be shared. Uniform bindings expose the actual payload length, excluding
allocation padding.

## 2. Authoring model

- Resources are declared once in shared `.h` lists. Uniforms expand to explicit
  ConstantBuffers, avoiding Metal argument-buffer semantics for flat constants.
  Shared-header tests compare SPIR-V, DXIL and MSL reflection.
- Variant keys are authored either as a `@variant` comment block
  (`.../config/pipeline_variants.slang`, parsed by `ShaderVariantGen::Parse`) or in a
  per-shader sibling **`.slang.json`** companion (`ShaderCompanion.h`, `ShaderCompanion.cpp`):
  `sources[{name,width}]`, `variants[{key,source,width,default,spec}]`, `vertex[{name,semantics[]}]`,
  `entries[]`, `depends[]`.
- Strong variants (macros) fold at compile; weak variants (specialization constants) go to
  native specialization (SPIR-V/MSL) or DXIL folding.
- Includes resolve via `ShaderFileSystem` → `SlangFileSystemAdapter` (virtual includes; shared
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
| `#include "x.hlslh"` | Slang `#include` (virtual includes; shared headers via `ShaderFileSystem`) |
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
2. Add the pass-owned shared `.h`, its reflection test and packaged source registration.
   C++ includes the source directly; CMake only embeds the same text for shaders.
3. Update the material `.mat` `techniques[].shader` to the `.slang` relative path; keep `tag`,
   `entry`, `renderState`.
4. Verify via `Aurora.ShaderTest` (slang compile/reflection) and the per-backend spike tests.
5. Delete the legacy `.hlsl`/`.hlslh`/`.tech` once no consumer remains.

## 8. Open gaps (blockers for full migration)

- Built-in scene sources are registered by Pipeline; external materials still require source/cache availability.
- `ShaderRef` has no `stages`; multi-stage legacy entries collapse to one `entry`.
- Technique/PSO cache (`TechniqueKey`) reserved but unimplemented (`aurora-material-pso`).
- Built-in SceneForward/SceneLit/SceneShadow/SceneToneMap are production shader implementations.
