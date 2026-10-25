# Aurora Shader Design & Legacy Migration

Status: current authoring and migration record. The [target artifact change](../../openspec/changes/refactor-aurora-target-shader-artifacts/design.md)
continues this design with immutable target artifacts, per-target reflection, named parameter transactions
and CPU-to-target upload plans. Cache format 10 / usage format 3 and material version 4 require recook. The [resource-ownership change](../../openspec/changes/refactor-aurora-pipeline-resource-ownership/design.md) defines the implemented module roots, persistent exports, tagged scene techniques, explicit fullscreen/compute techniques and Global/Pass/Material/Batch ownership (extended by the target-shader-artifacts change). Its tasks record fresh validation and platform limits.

## 1. Language & modules

Aurora shader implementations are authored in **Slang** (`.slang`). C++ consumes authoritative `.h` declarations; the exporter produces persistent `.slangh` interfaces in each module shader asset root. CMake update/check targets validate synchronization without running Slang for C++ compilation.

| Target | Kind | Path | Responsibility |
|---|---|---|---|
| `Aurora.Core.Static` | STATIC | `engine/aurora/core` | slang-free shader definition/parse/cache/variant/`RgBlockDesc`/resolver |
| `Aurora.ShaderCompiler.Static` | STATIC | `engine/aurora/shader` | Slang compiler impl (`ShaderCompilerSlang`) |
| `Aurora.ShaderCompiler` | SHARED (module) | `engine/aurora/shader` | registers the compiler into `ShaderCompilerFactory` |
| `ShaderHeaderTool` / `ShaderCookTool` | EXE | `engine/aurora/shader/tool` | variant schema generation / usage-cache precompile |
| `Aurora.Pipeline.Static` | STATIC | `engine/aurora/pipeline` | scene/fullscreen frame features, variant assembly and reflection validation |

Resource tiers: set 0 = Global, set 1 = Pass, set 2 = Material, set 3 = Batch.
Dynamic offsets follow explicit binding types.

The resource contract is Global / Pass / Material / Batch. View data is optional and
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

The unused RenderGraph and mutable pass template classes were removed on 2026-10-26.
Scene collection and technique selection use `SceneRenderFeature` and `FrameComposer`;
fullscreen composition uses `TextureToScreenFeature`. Shared Pass declaration headers
and their persistent shader exports remain available. See
[the removal change](../../openspec/changes/remove-unused-legacy-rendergraph/design.md).

Built-in Global and Pass/Batch declarations live in `engine/aurora/pipeline/shaders/*.h`;
user pipelines retain their headers and shader sources inside their own targets.
C++ compilation needs no shader compiler or header generator. The module resource descriptor supplies exports, source inventory, include roots and dependencies.
`ShaderResources.Stage` deploys canonical paths under `assets/shaders/`; no C++ source embedding remains.
`SceneRenderFeature` uses OpaquePass for camera inputs, SceneBatch for world transforms,
SceneSurfaceBatch (set 3, binding 1) for per-draw flags, and MaterialBlock (set 2) for authored tint/surface values.
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

`ShaderRef {relativePath,entry}` remains the logical source identity; stage is explicit.
Each sealed stage artifact owns exact code bytes, the actual native entry, complete logical
declarations and target reflection. ShaderFunction retains it; Shader links matching stage
artifacts and derives its layouts. There is no independent code/reflection pairing.

Development resolution captures source closure and actual compiler identity before checking
its compile key. Products resolution reads the selected canonical environment and resolves
published request → ArtifactID without a compiler or sources. The ArtifactID binds every
identity field, producer/runtime ABI and code/reflection payload. Index/blob format 10 and
usage format 3 require recook; Name handles and machine paths are not persistent identities.

SPIR-V, DXIL and MSL each produce their own native reflection. AuroraResource/AuroraField
attributes preserve logical identities. The current MSL profile is metallib_2_3, emitting
MSL source; native compilation explicitly uses language 2.3 on macOS 11 / iOS 14 or later.
See [Shader System](../features/shader-system.md) for environment and Unsupported boundaries.

## 4. Material technique model (version 4)

Material payload 4 and instance payload 2 retain explicit stage references and canonical
Key/Value properties. The complete Group 2 reflection (zero/one cbuffer, textures, samplers)
plus Shader-domain static options generates the schema; backend offsets are not asset data.
`ResolveMaterialDefinition` reconstructs it from source/product metadata. Legacy v3 aliases
such as `baseColor -> gMaterial.tint` migrate only through exact declarations; conflicting
or unknown targets fail. New saves use `gMaterial.tint` directly and move legacy technique
options into the Material properties. Instances cannot override static options.
Each stage still needs its canonical source/entry and companion key-to-symbol associations.

SceneAssetSource preserves tagged techniques and named CPU snapshots; it does not pack GPU
uniform bytes. The selected Shader's Name table and cached UniformUploadPlan determine actual
target encoding. Texture references become exact catalog versions and explicit graph reads.
Missing, stale or unsupported resources fail explicitly. Instances inherit unoverridden values;
new parameter/texture snapshots leave old in-flight frames intact.

Feature-owned world/receive-shadow/surface blocks belong to Batch set 3; material tint and
textures belong to Material set 2. Each block has one explicit provider or assembler. Fullscreen/compute can omit
scene, material, Tag and Batch altogether; camera data belongs to the consuming Pass.

ResourceGroup uses a Name-first typed transaction with an injected RHI-neutral update context.
Data, blocks, buffers, images and samplers share name/type/owner validation. End returns a new
immutable snapshot, preserves prior bindings on failure and never submits GPU work. Typed Pass
slots retain their exact CPU schema until the actual Shader selects a native upload plan.

## 5. Legacy pipeline (removed)

- The legacy HLSL pipeline (`.hlsl`/`.hlslh` + `.tech`) and its DXC/glslang compiler were removed together with
  `engine/render/**` and `engine/shader` by `openspec/changes/legacy-render-thirdparty-cleanup/`.
- The unconsumed repository `assets/shaders` HLSL files, six legacy `.mat` files and
  `assets/vertex/vertex_library.vtxlib` have been removed. Their old references are
  not supported material examples; use the version-4 model above.
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
5. Recook material version 4 / instance version 2 and shader cache index version 10. Project packaging aggregates
   selected owners into one index per target; runtime receives the products root.
6. Run export checks, layout reflection and affected native rendering tests. Remove obsolete
   sources only under the reviewed migration inventory.

## 8. Validation limits

Current implementation and device evidence are tracked in
`openspec/changes/refactor-aurora-target-shader-artifacts/tasks.md`; earlier resource-ownership
results remain historical evidence for that change. Cross-builds, compiler reflection checks
and native GPU execution are separate gates. Source-free MSL still invokes Apple's native
compiler and is not a precompiled metallib distribution.

Nested/FLOAT3/BOOL/half uniform packing, Metal descriptor arrays/argument buffers, typed/counter
storage buffers, material deformation, full PBR shading and a general PSO cache remain outside
this change. Linux/Windows/Android/iOS native tests require their actual available hosts/devices;
a successful macOS test or iOS 13 cross-build cannot substitute for them.


## 8. Confirmed variant contract (2026-10-25)

The active `refactor-aurora-shader-source-techniques` change retains JSON companions and
explicit stage programs; its older source-marker DSL was withdrawn. V/P/S selection lives
in Core. Each target derives compile/native lowering from the declared source symbols;
callers supply typed values only. IDs belong to individual stages. Full selection,
stage code and executable identities remain distinct. Published records authorize complete
selections, including native values; generic code reuse does not authorize arbitrary values.

See [typed selection](../features/shader-system.md#typed-selection-and-target-lowering)
for authoring, preparation and supported shapes. See the active change's implementation
record for fresh test results and the pending macOS native gate.
