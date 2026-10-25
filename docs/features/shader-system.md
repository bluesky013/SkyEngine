---
title: "Shader System"
description: "Pass declarations, persistent shader exports and module resource inventories."
module: "aurora"
updated: "2026-10-25"
---

## Declaration source and asset paths

C++ consumes the authoritative shared `.h` directly. `SharedShader.h` expands its
field/resource lists into native types, typed binding slots and an independent CPU schema.
`python/shader_headers.py` exports those same lists to pure, version-controlled
`.slangh` assets. Slang includes `.slangh`; it no longer includes the C++ declarations.

| Module | Shader asset root | Canonical include/program path |
|---|---|---|
| Aurora | `engine/aurora/assets/shaders/` | `aurora/pipeline/OpaquePass.slangh` |
| Plugin toon | `plugins/toon/assets/shaders/` | `toon/pipeline/ToonPass.slangh` |
| Aurora material | same Aurora root | `aurora/materials/Unlit.slang` |

Existing built-in declaration sources remain in `engine/aurora/pipeline/shaders/`.
An export's `include` field preserves its logical domain path: `aurora/pipeline/OpaquePass.h`
becomes `aurora/pipeline/OpaquePass.slangh`. There is no repository-wide plugins shader
root, `plugins/` include prefix, or second mount for generated files.

The shared declaration grammar supports flat float/int/uint, float2/float4, column-major
mat4 and bounded float4 arrays, plus supported texture/sampler declarations. BOOL/FLOAT3
uniform fields, nested structures and descriptor arrays
are rejected by the shared authoring grammar. Read-only structured float4 buffers support
skinning palettes through the same graph transaction. Target layouts may differ from C++ offsets,
size, array stride and matrix major. `UniformUploadPlan` validates semantic compatibility and
encodes each actual target ABI. Multiple declarations in one header are allowed.

Exports carry `AuroraResource(set,binding,owner)` and `AuroraField(path)` user attributes.
Handwritten Slang resources must supply the same attributes; register numbers and short-name
matching do not establish logical identity. The compiler reads only the requested target's
layout and usage metadata. MSL has no auxiliary SPIR-V identity target. Native slot/category,
stage, read-only/read-write access and raw/structured stride are part of the artifact.
CPU helper layout changes do not alter the generated shader declaration fingerprint.

Keep HLSL `register(..., space...)` annotations target-specific with `AURORA_TARGET_DXIL`.
The configured Slang baseline is v2026.19. Conflicting Metal direct argument indices
are explicitly Unsupported. For MSL, let the target allocate addresses and consume its
actual reflection. Vulkan continues to use explicit `vk::binding` addresses.

## One module resource description

Aurora registers `engine/aurora/resources.json`. Plugins add `resources` to their existing
`plugin.json`; `sky_plugin_apply_target_dependencies` forwards it to the shared collector.
A project/native module may use the same descriptor without a business-plugin runtime DLL.

```json
{
  "resources": {
    "owner": "toon",
    "roots": [{
      "kind": "shader",
      "path": "assets/shaders",
      "dependencies": ["aurora"],
      "includeRoots": ["toon/helpers"],
      "exports": [{
        "source": "include/toon/pipeline/ToonPass.h",
        "include": "toon/pipeline/ToonPass.h"
      }],
      "programs": [{
        "path": "toon/materials/Toon.slang",
        "entries": [{"stage": "vertex", "name": "ToonVS"}, {"stage": "fragment", "name": "ToonFS"}]
      }]
    }]
  }
}
```

Paths are relative to the descriptor's directory and must stay inside it. Shader files
must live under their owner's namespace. `includeRoots` is optional: canonical angle
includes work without it; quoted local includes resolve beside their source. Custom
include directories must exist inside the owner's namespace. The dependency closure is
validated for missing/cyclic owners, case collisions and ambiguous include aliases before
publication. No mount-order override is permitted.

Run `cmake --build cmake-build-debug --config Debug --target ShaderHeaders.Update` after
changing a declaration or exporter. `ShaderHeaders.Check` reads and validates existing
exports; ordinary C++ targets do not depend on either export execution or Slang. Exports
are not CMake byproducts, so clean preserves them. No-op updates preserve timestamps;
stale/manual edits and handwritten output collisions fail. Remove renamed/deleted tracked
outputs with their mappings; the collector rejects orphaned generated assets.

## Source staging and lifetime

`python/module_resources.py` collects the descriptors into one `shader-sources.json` and
canonical `.slang`, `.slangh` and companion file inventory. `ShaderResources.Stage` deploys
it to `output/bin/<Config>/assets/shaders/`; source/SDK installation keeps that layout.
Test plugin resources use a separate test-assets root and are not shipped with the editor.

Hosts call `ShaderFileSystem::LoadBundles(root, owner)` to load an owner and its declared
dependencies; an empty owner loads the whole selected inventory. The root can be a native,
packaged or custom `IFileSystem`, but it must expose the explicit inventory. `AddBundles`
is the owned in-memory equivalent. Neither requires runtime plugin activation. The older
`aurora_shader_bundle`, embedded C++ shader strings and `shader-bundles/owner/sources`
deployment are removed.

Registration copies source bytes, normalizes CRLF and publishes atomically after validation.
Compilation snapshots expose only the owning module and its dependency closure. Hashing
covers all files and include roots in that closure, including macro includes and imports.
Accepted tasks survive source/provider/resolver retirement. The compiler and cache writer
must still outlive their pending work. Register on the owner thread before handing sources
to render features; feature/module leases follow normal GPU retirement.

## Project cook and shipping products

`package_project.py` calls the same collector during project packaging, after ordinary
asset cook. It collects module `programs`, project `configs/shader_requirements.json`, and
material `.mat` technique requests. A shader root/program requirement defaults to the
`common` product bundle. Root/program bundle selection follows the selected distribution.
Explicit `variants` are objects of schema key/value pairs; they are compiled as distinct
requests rather than only recording a hash. Runtime usage can supplement authored requirements.

The project descriptor selects plugin resource descriptions via `plugins` and can name
project module descriptors with `resourceModules`. Export presets expose `shaderTargets`
through the existing reflected settings panel; empty means the platform default.

```text
python python/package_project.py cook-shaders --project <project> --distribution shipping --platform windows --target vulkan --target d3d12
ShaderCookTool <requirements.json> <sourceRoot> <cacheDir> [offlineCacheDir]
```

Usage JSON format 3 persists an explicit canonical target environment. It contains target,
platform, code format, profile, target-language minimum version, capability requirements,
matrix convention and compiler options. Minimum versions encode major × 10000 + minor × 100
+ patch. The current baselines are SPIR-V 1.5, shader model 6.5 and Metal 2.3. Metal 2.3
requires macOS 11 / iOS 14; the iOS deployment target can still be cross-built for 13, but
that does not qualify these artifacts for execution on iOS 13. Unsupported capabilities or
language/profile combinations fail explicitly.

One cook invocation aggregates all selected owners into each target's
`index.bin`/blob format 10. Products live in `<project>/products/shaders/<distribution>/`
and packaging copies the selected distribution to `products/shaders/`. The tool verifies
that every requested artifact can be read back with no source filesystem. It does not
stack independently generated owner indices in MultiFileSystem.

A compile key includes actual compiler build identity, adapter/reflection versions, source
closure, canonical variants and target environment. ArtifactID binds exact code bytes,
source/native entries, stage, producer, runtime ABI 1 and full reflection protocol 1.
There is no word-count rounding. Blob validation/publication precedes index publication;
atomic replacement failures preserve the old index. Old artifacts require recook.

Producer identity is target-specific. Windows DXIL also hashes the deployed `dxcompiler.dll`
and `dxil.dll`, pins those files and loads from their verified directory. Missing libraries
fail development resolution before cache lookup; their machine paths do not enter the key.

`DEVELOPMENT` resolution requires sources and the current compiler identity. `PRODUCTS`
resolution reads published request → ArtifactID without accessing sources or a compiler.
The selected distribution environment supplies omitted runtime request options; explicit
request environments must match. Products from another installed compiler remain consumable
when their runtime ABI and device requirements are compatible. Source-free MSL products are
MSL source compiled by Apple's native Metal API, not precompiled metallib products.

The launcher passes the product root into the existing resolver. A source-only development
launch uses the staged source inventory. Scene asset sources and features accept their
source/product providers from the host; they do not install embedded default sources.
`python/tests/run_shader_package_tests.py` exercises the real project command, relocates
its products and removes temporary project sources before a Core-only consumer resolves
them without Slang. Native Metal execution still requires macOS.

## Techniques and resource bindings

`ShaderTechnique` names explicit vertex/fragment entries or one compute entry, plus variant
values. Every stage uses a canonical `ShaderRef`; entry names have no required naming convention.
Each source needs companion metadata, including an empty variant schema when it has no variants.
Keys are validated against the union of stage schemas, then projected onto each stage.
Logical declarations must agree across stages; their native IDs and lowering may differ.

Scene materials retain all tagged techniques. `FrameGraphBuildContext::ResolveDraws(tag)`
selects the matching technique before geometry resolution; no match omits the item and
duplicate Tags are rejected. Opaque and shadow queues use the material's corresponding
program. The default material is an explicit SceneAssetSource policy. Fullscreen and compute
features can create a shader from a technique and submit their Pass directly, without a
scene, queue Tag or material. Dispatch groups remain invocation data.

Bindings remain Global set 0 / Pass set 1 / Material set 2 / Batch set 3:

| Input | Built-in supplier |
|---|---|
| Frame time / delta | GlobalBlock, common to all views |
| View projection / camera | OpaquePass; ShadowPass has its own light projection |
| Lighting / shadow map / IBL | Scene Feature's Pass resources |
| World transform / receive-shadow | Feature-owned Batch bindings 0 / 1 |
| Tint / surface parameters | Material set 2, one cbuffer |

These Batch slots are the built-in material interface, not a generic renderer ABI. Custom
interfaces declare their own bindings. Material payload version 4 and instance version 2
persist canonical Key/Value data, typed samplers and ordinary texture references.
Material keys are generated from all Group 2 declarations plus Shader-domain options;
Group 2 permits zero or one cbuffer. Legacy v3 aliases are migration input: resolve exact
canonical targets, reject conflicts/unknown keys, and save canonical properties.
Asset dependencies include ordinary 2D textures. Linear data and sRGB color formats remain
separate through cook, CPU transport and native upload. Unsupported texture shapes fail
explicitly instead of being interpreted as environment radiance.

Material/instance snapshots retain Name + typed CPU values, exact image revisions and sampler
descriptors. They do not contain target uniform bytes. Prepared draws resolve image eligibility
and declare graph reads before named binding. Material and Feature providers must remain
distinct; combined blocks need one explicit assembler. Missing active inputs and overlapping
providers fail before replacing the draw. Inactive declared parameters need no native binding.

`ShaderFunction` retains a sealed immutable stage artifact. Shader linking merges its actual
stage artifacts and rejects unrelated reflection or incompatible stages/environments. Dynamic
UBO policy is explicit. A logical UBO with incompatible byte layouts across stages is currently
unsupported. Metal consumes explicit direct-argument plans, including sparse per-stage slots;
there is no empty-metadata ordinal fallback. Vulkan supports stage address fan-out and existing
arrays, but two independently updated logical groups cannot share one native descriptor set.
DX12 preserves SRV/UAV access class, raw/structured form, stride and checked view ranges.

## Unified named parameter transactions

The v10 shader index stores typed variant ownership (Vertex, Pipeline, Shader), Vertex
semantic requirements, target mappings and complete allowed selections. A shipping scene loads
these from the published index; missing or conflicting metadata is an explicit
recook error. Old index/blob versions are rejected. The authoring companion format
and explicit technique entry points are unchanged.

Vertex inputs derive from the actual mesh attributes and per-instance streams.
`INSTANCE` occupies the reserved sixteenth capability bit. Vertex definitions must
reference declared one-bit keys and known, nonempty semantics. Named effective
specialization values survive target lowering, so DXIL receives the selected value.
Technique selection validates the union of stage declarations; each stage compiles
against its own schema plus Pipeline keys, retaining stage-local defaults.

Material providers expose all declared set-2 resources, including textures, samplers
and custom blocks. Material declarations stay fixed across compile selections; values and target active usage may change.
SceneSurface flags, skinning palettes and UI per-draw textures use Batch set 3.
Every executable material variant is validated before packet application and charged
to the CPU resource budget; shared artifacts are conservatively charged per owner.

Async compilation requires an owned compiler registration and explicit cache owners
through `SetAsyncCacheOwners`. Futures retain the compiler module, compiler, cache
root and writer, and accept only one execution. Unregistration prevents new work;
accepted futures keep the module loaded until their owners are released. Borrowed
compiler/cache registrations remain synchronous-only.

`ResourceGroup::CreateEncoder(updateContext)` provides the same typed entry for data and
resources. The RHI-neutral context supplies device/lease validation, uniform allocation and
retention; it has no assets, UUIDs or graph handles. Canonical fields are `block.field` and
resource names come from declarations. Aliases must be explicit and unambiguous. Lookup
handles carry an interface generation and cannot be persisted or reused with another version.

```cpp
auto update = group->CreateEncoder(updateContext);
if (!update->WriteData(Name("gMaterial.tint"), tint) ||
    !update->WriteImage(Name("baseColorTexture"), imageView, ImageLayout::SHADER_READ_ONLY) ||
    !update->WriteSampler(Name("baseColorSampler"), sampler) || !update->End()) {
    return ReportParameterError(update->GetError());
}
auto nextGroup = update->GetResult();
```

`WriteBlock` takes an explicit CPU schema; `WriteData` carries semantic type and shape.
Same-sized FLOAT/INT/UINT values are not interchangeable. `WriteBuffer` validates storage
form, owner and range. Known inactive writes return a distinct `INACTIVE` status; unknown,
ambiguous, stale, duplicate or overlapping writes poison the transaction. End returns false
after any error, even when an individual result was ignored. Cancel/destruction publishes
nothing. End creates no queue or GPU submission.

The first successful version must supply all active required fields/resources. Later updates
inherit omitted values and return a new immutable group. Draws retain previous snapshots and
allocation leases until retirement. ResourceGroup and the frame graph adapter both delegate to the same RHI
`ShaderParameterTransaction`. It freezes encoded uniform bytes and opaque resource tokens;
separate materializers retain native resources or declare graph accesses under the existing
lease. Each block is encoded and uploaded once. The numeric descriptor operations remain
internal native execution primitives; backends do not search names.

Uniform plans are cached per shader version and CPU schema fingerprint. They encode native
offsets, matrix major/stride and float4 array stride, clear target padding and never read CPU
padding. A whole-block copy is allowed only when semantic compatibility and complete byte
coverage prove it contains no padding. Typed Pass slots retain CPU values until the draw's
actual shader selects the plan. Material fields use the same typed schema/plan rules.

Built-in Lit provides the current lighting/IBL path, not a complete PBR material system.
Descriptor arrays in Metal direct arguments, nested uniforms, FLOAT3/BOOL/half packing,
typed/counter storage buffers and material deformation remain unsupported. Native execution
coverage is recorded in the [target artifact change](../../openspec/changes/refactor-aurora-target-shader-artifacts/tasks.md);
platform builds and native-device results are tracked separately.


## Typed selection and target lowering

`ResolveShaderVariant` owns normalization for V/P/S. V comes from actual vertex semantics
(including INSTANCE and skin streams); P comes from the Pipeline; S comes from Material,
custom Batch, fullscreen or compute invocation. The four callers of S are producers of the
same domain, not additional bit regions. Duplicate, unknown, wrong-domain, wrong-type or
out-of-range values fail before publication. Omitted values use declared defaults.

Companions associate logical keys with source symbols explicitly:

```json
{"entries":["FSMain"],"sources":[{"name":"options","width":1}],"variants":[
  {"key":"TEXTURED","source":"options","type":"bool","width":1,"default":false,"symbol":"kTextured"}
]}
```

The key and symbol must differ so preprocessing does not replace the declaration name.
Declare `kTextured` as a scalar constant initialized from the `TEXTURED` macro. Native-capable
source uses `[SpecializationConstant]` with its native annotation; a DXIL branch uses
`static const bool kTextured = TEXTURED`. The pinned compiler inspects this declaration.
Pure preprocessing keys may omit `symbol` and remain compile-only. The association is required
when inspecting a scalar declaration for target-native lowering.
Business requests never contain native IDs or a macro/specialization selector. Supported
logical types are bool, int32 and uint32; native-dependent resource/array/thread-group shapes
are outside the fixed-metadata contract. Unsupported declarations fail without rewriting code.

Full selection identity contains all normalized typed values. A stage code key retains only
compile-lowered values; native values can share its code. Executable identity additionally
contains ordered stages and each stage's native mapping/value. VS and FS may use the same
native ID with different keys, values and types. Native required resources remain required
even when a function constant is false.

Material values remain typed CPU data. Known inactive values are retained and type-checked;
unknown keys fail. The final target ShaderParameterTable/UniformUploadPlan chooses active
bindings, offsets, strides and padding. MaterialInstance only overrides ordinary values,
textures and samplers; static options remain on Material. Editing a static option produces
another prepared version and must have a corresponding published selection.

Scene asset preparation uses the Pipeline's four explicit SHADOWS/IBL requests and two
supported vertex-stream configurations. Additional option ranges are not enumerated.
Scene feature Prepare resolves tone mapping/highlight programs before graph declarations
and output acquisition. Graph recording consumes those prepared shaders and exact resource
snapshots. Existing arbitrary feature code remains responsible for preparing its own programs.

Cook collects explicit requests from module/project requirements, material options and usage.
Each shared artifact records the complete allowed selections. In PRODUCTS mode an unlisted
native selection is missing even if its generic code exists. Cache/blob v10 and usage v3
require recook; the distribution manifest remains v2. Logical metadata is bounded separately
from the optional 128-bit packed key; larger selections retain canonical typed identity.
