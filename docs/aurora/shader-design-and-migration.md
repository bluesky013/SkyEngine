# Aurora Shader Design & Legacy Migration

Status: current authoring and migration record. The [resource-ownership change](../../openspec/changes/refactor-aurora-pipeline-resource-ownership/design.md) defines the implemented module roots, persistent exports, tagged scene techniques, explicit fullscreen/compute techniques and Global/Pass/Batch ownership. Its tasks record fresh validation and platform limits.

## 1. Language & modules

Aurora shader implementations are authored in **Slang** (`.slang`). C++ consumes authoritative `.h` declarations; the exporter produces persistent `.slangh` interfaces in each module shader asset root. CMake update/check targets validate synchronization without running Slang for C++ compilation.

| Target | Kind | Path | Responsibility |
|---|---|---|---|
| `Aurora.Core.Static` | STATIC | `engine/aurora/core` | slang-free shader definition/parse/cache/variant/`RgBlockDesc`/resolver |
| `Aurora.ShaderCompiler.Static` | STATIC | `engine/aurora/shader` | Slang compiler impl (`ShaderCompilerSlang`) |
| `AuroraShaderCompiler` | SHARED (module) | `engine/aurora/shader` | registers the compiler into `ShaderCompilerFactory` |
| `ShaderHeaderTool` / `ShaderCookTool` | EXE | `engine/aurora/shader/tool` | variant schema generation / usage-cache precompile |
| `Aurora.Pipeline.Static` | STATIC | `engine/aurora/pipeline` | pass templates, global resources (`pipeline/src/ReflectionValidation.cpp`) |

Resource tiers: set 0 = Global, set 1 = Pass, set 2 = Batch.
Dynamic offsets follow explicit binding types.

The resource contract remains Global / Pass / Batch. View data is optional and
declared by the consuming pass; it is not a replacement resource tier. Each pass
owns one shared `.h` declaration source. Native C++ types and resource metadata
are expanded from the same lists as shader declarations; shader reflection validates
the result. See [Shader System](../features/shader-system.md) for authoring rules.

The production Renderer now invokes registered pass features through
`FrameGraphBuildContext`. `PreparedRaster` resolves geometry, exact resource
versions and owned material uniform snapshots; it does not choose parameter layouts.
`RendererRuntime` validates declarations rather than a fixed three-UBO ABI.
The change and platform validation are tracked by
`openspec/changes/refactor-aurora-pipeline-resource-ownership`.

`ScenePass` / `OpaquePass` classes and `SceneRasterPassTemplate` remain legacy
RenderGraph templates with test consumers. Their default collection does not emit
draws. Production scene collection and technique selection use `SceneRenderFeature`
and `FrameComposer`; the shared Pass declaration headers serve both paths.

Built-in Global and Pass/Batch declarations live in `engine/aurora/pipeline/shaders/*.h`;
user pipelines retain their headers and shader sources inside their own targets.
C++ compilation needs no shader compiler or header generator. The module resource descriptor supplies exports, source inventory, include roots and dependencies.
`ShaderResources.Stage` deploys canonical paths under `assets/shaders/`; no C++ source embedding remains.
`SceneRenderFeature` uses OpaquePass for camera inputs, SceneBatch for world transforms,
SceneSurfaceBatch for Feature flags, and MaterialBatch for authored tint/surface values.
Lighting/Shadow/Environment/ToneMap own their additional inputs. Other interfaces may
use different layouts or omit resource tiers.

Shared headers expose typed slots and expected resource/block metadata.
`aurora/renderer/PassResourceBindings.h` in Aurora Core validates actual compiled shader use before replacing draw
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
- Includes resolve via `ShaderFileSystem` → `SlangFileSystemAdapter` from validated module inventories.
  Shared `.slangh` files are persisted in source and may be staged through any inventoried provider.
- Compile targets (`ShaderCompilerSlang.cpp`): SPIRV `spirv_1_5`, MSL `metal`, DXIL `sm_6_5`;
  target macros `AURORA_TARGET_SPIRV/MSL/DXIL`.

## 3. Identity, resolve, cache

- Each technique stage references a shader via **`ShaderRef { relativePath, entry }`**
  (`engine/aurora/core/include/aurora/shader/ShaderRef.h`). There is **no `stages` field**;
  `stage` is a per-request input to `ShaderResolver`.
- Resolve priority: **offline cache → source compile → local cache** (`ShaderResolver`).
- Cache: content-addressed `<root>/<target>/{index.bin, blobs/<compileHash>}`,
  `target ∈ {vulkan, metal, d3d12}` (`ShaderCacheStore.h`).
- `ShaderUsageCollector` → `shader_usage.json` → `ShaderCookTool` precompiles the shipped cache.

## 4. Material technique model (version 2)

Material `.mat` JSON is loaded/saved by MaterialReflection; its binary payload is version 2.
Explicit stage references replace the old single `shader` field. Uniform mappings name the
shader member first and the authored material property second:

```json
{
  "techniques": [{
    "tag": "opaque",
    "program": {
      "vertex": {"relativePath": "aurora/materials/Unlit.slang", "entry": "VSMain"},
      "fragment": {"relativePath": "aurora/materials/Unlit.slang", "entry": "FSMain"}
    },
    "uniforms": [{"binding": 2, "fields": {"tint": "baseColor", "surface": "surface"}}],
    "renderState": {"depthStencil": {"depthTest": true, "depthWrite": true}}
  }],
  "properties": [
    {"name": "baseColor", "type": 3, "value": {"index": 3, "value": [1, 0, 0, 1]}},
    {"name": "surface", "type": 3, "value": {"index": 3, "value": [0.5, 0, 0, 0]}}
  ]
}
```

- `program.variant` optionally maps schema keys to unsigned values. Each stage has an explicit
  canonical source and entry; arbitrary entry names work. Every source has a `.slang.json`
  companion, even when its variant schema is empty.
- SceneAssetSource preserves every technique and packs mapped properties using reflection.
  Duplicate Tags are rejected. FrameComposer selects `opaque` / `shadow` techniques by Tag;
  no match omits the draw. PreparedRaster resolves the selected geometry and resources.
- The built-in world/receive-shadow Batch blocks come from the Feature. The material owns its
  tint/surface block. These are separate set-2 bindings, not a generic fixed material layout.
  Other interfaces may use different layouts or one explicit combined-block assembler.
- Fullscreen and compute use ShaderTechnique directly, without a material, Tag or scene.
  Camera inputs live in concrete Pass declarations; Global contains common frame time.
- `value.index` selects float / float2 / float3 / float4 / mat4 / int / uint / bool / texture
  property storage. Current scene uniform assembly supports the first seven alternatives;
  material texture binding and bool/array GPU layouts are not implemented by this adapter.
- Editor technique rows expose separate vertex and fragment source/entry fields. Uniform
  mappings and variant values can be authored in JSON and are preserved during roundtrip.

## 5. Legacy pipeline (removed)

- The legacy HLSL pipeline (`.hlsl`/`.hlslh` + `.tech`) and its DXC/glslang compiler were removed together with
  `engine/render/**` and `engine/shader` by `openspec/changes/legacy-render-thirdparty-cleanup/`.
- The unconsumed repository `assets/shaders` HLSL files, six legacy `.mat` files and
  `assets/vertex/vertex_library.vtxlib` have been removed. Their old references are
  not supported material examples; use the version-2 model above.
- Runtime `ShaderBlockGen` HLSL generation has been removed. Shared `.h` declarations
  export persistent `.slangh`; `RgBlockDesc` remains layout metadata/validation.
- Aurora uses the Slang-based compiler in `engine/aurora/shader`.

## 6. Migration map (legacy → aurora)

| Legacy | Aurora |
|---|---|
| `assets/shaders/*.hlsl` | `<module>/assets/shaders/<owner>/.../*.slang` |
| `#include "x.hlslh"` | canonical `.slangh` includes through the registered module inventory |
| `#pragma option({...})` variant macros | `.slang.json` `variants` / `@variant` |
| `.tech` `shader.path` | material `techniques[].program.vertex/fragment` (canonical `.slang` paths) |
| `.tech` staged entries (`vertex/task/mesh/fragment`) | explicit stage-specific `ShaderRef.entry`; no inferred entry suffixes |
| `.tech` `pass.tag` | `techniques[].tag` |
| `.tech` `depth_stencil` / `raster_state` / `blend_state` | `techniques[].renderState`; scene execution currently supports depth/blend and rejects unsupported state |
| `.tech` `vertex_options` | companion schema plus material `program.variant` values |
| `.technique` reference from material | inline `techniques[]` in `.mat` |

This map is for migrating external legacy content; it does not imply that all old
material features have runtime equivalents.

## 7. Migration steps

1. Register the module root, dependencies, exports and programs in the common resource descriptor.
2. Put `.slang` and companion metadata under `<module>/assets/shaders/<owner>/...`.
3. Author the shared `.h`, run `ShaderHeaders.Update`, and commit its persisted `.slangh`
   alongside the source. C++ includes `.h`; shaders include `.slangh`.
4. Replace material `shader` fields with explicit `program.vertex/fragment`, preserve Tags,
   and map material uniform members to typed properties. Empty sources/entries are errors.
5. Recook material version 2 and shader cache index version 4. Project packaging aggregates
   selected owners into one index per target; runtime receives the products root.
6. Run export checks, layout reflection and affected native rendering tests. Remove obsolete
   sources only under the reviewed migration inventory.

## 8. Validation limits

Module roots, persistent exports, source staging, selected project cook and relocated product
lookup are implemented. Native tests cover tagged Unlit/Lit/custom programs, material shadow
selection, independent Pass snapshots and explicit compute/fullscreen without Batch.
The Core-only product consumer has no Slang link; native scene tests use empty sources and
an unregistered compiler, but their test executable still links the compiler library.

Remaining capabilities include automatic material texture/sampler binding, deformation,
full PBR shading and a general technique PSO cache. Existing lit/IBL behavior is preserved.
MSL compilation/reflection on Windows does not establish native Metal execution. See the
active change's tasks.md for current counts and platform-specific evidence.
