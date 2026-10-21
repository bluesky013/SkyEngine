---
title: "Shader System"
description: "Pass declarations, persistent shader exports and module resource inventories."
module: "aurora"
updated: "2026-10-22"
---

## Declaration source and asset paths

C++ consumes the authoritative shared `.h` directly. `SharedShader.h` expands its
field/resource lists into native types, typed binding slots and expected reflection.
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
uniform fields, nested structures, storage-buffer declarations and descriptor arrays
are rejected. SPIR-V/DXIL/MSL reflection must agree with native C++ metadata; exporting
text does not prove target ABI compatibility. Multiple declarations in one header are allowed.

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
python python/package_project.py cook-shaders --project <project> --distribution shipping --target vulkan --target d3d12
ShaderCookTool <requirements.json> <sourceRoot> <cacheDir> [offlineCacheDir]
```

One cook invocation aggregates all selected owners into each target's existing
`index.bin`/blob format. Products live in `<project>/products/shaders/<distribution>/`
and packaging copies the selected distribution to `products/shaders/`. The tool verifies
that every requested artifact can be read back with no source filesystem. It does not
stack independently generated owner indices in MultiFileSystem.

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
Variant keys must be supported by each stage's schema. Compile requests and offline products
use the same identities.

Scene materials retain all tagged techniques. `FrameGraphBuildContext::ResolveDraws(tag)`
selects the matching technique before geometry resolution; no match omits the item and
duplicate Tags are rejected. Opaque and shadow queues use the material's corresponding
program. The default material is an explicit SceneAssetSource policy. Fullscreen and compute
features can create a shader from a technique and submit their Pass directly, without a
scene, queue Tag or material. Dispatch groups remain invocation data.

Bindings remain Global set 0 / Pass set 1 / Batch set 2:

| Input | Built-in supplier |
|---|---|
| Frame time / delta | GlobalBlock, common to all views |
| View projection / camera | OpaquePass; ShadowPass has its own light projection |
| Lighting / shadow map / IBL | Scene Feature's Pass resources |
| World transform / receive-shadow | Feature-owned Batch bindings 0 / 1 |
| Tint / surface parameters | Material-owned Batch binding 2 |

These Batch slots are the built-in material interface, not a generic renderer ABI. Custom
interfaces declare their own bindings. Material source version 2 specifies whole uniform
bindings and member-to-property mappings. `PackShaderUniform` packs values at actual reflected
offsets; it never copies the CPU property map as a GPU block. A combined block requires one
explicit assembler supplying all fields. Duplicate owners, missing active inputs and layout
mismatches fail transactionally through `PassResourceBindings`. Unused shared declarations
need no bindings; fullscreen/compute may have no Batch at all.

Material versions own compiled shaders and CPU uniform snapshots. Uniform storage is allocated
for the accepted frame and retained until GPU retirement; making a material resident does not
upload a dummy material buffer. `PreparedRaster` resolves geometry, exact resource versions
and owned snapshots; it neither chooses Tags nor defines material layouts.

The current material adapter supports scalar/vector/mat4/int/uint uniform properties. Automatic
material texture/sampler asset binding, arrays, skinning and a general material PSO cache are
separate capabilities; absent active resources produce an error. Built-in Lit provides the
current lighting/IBL path, not a complete PBR material system. See the
[change validation record](../../openspec/changes/refactor-aurora-pipeline-resource-ownership/tasks.md)
for platform coverage and remaining limits.
