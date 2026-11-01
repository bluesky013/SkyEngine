## Status and confirmed direction

This change corrects the architecture after checkpoint `66a5463c`. Implementation covers persistent exports, module resources, project products, tagged techniques and resource tiers. Fresh validation and platform limits are recorded in tasks.md; historical results are not evidence for the corrected behavior.

The user confirmed these decisions during review:

1. ScenePass matches material technique Tags into RenderQueues. FullScreenPass and ComputePass explicitly select a technique.
2. Plugin registration declares shader import rules once, sharing resource-import metadata with assets where appropriate.
3. Authoritative shared `.h` declarations export shader-only `.slangh` assets through CMake tooling. Exports persist in the source tree and version control under the module's `assets/shaders/` root; C++ compilation does not depend on Slang.
4. Material data occupies set 2; Feature/draw Batch data occupies set 3. FullScreenPass and ComputePass provide only groups declared by their programs.
5. File inventories, unique paths and MultiFileSystem composition belong to the same registration rule as item 2. Aurora has one `engine/aurora/assets/shaders/` root; each plugin has its own `plugins/<plugin-id>/assets/shaders/` root.
6. View shader data belongs to Pass. The current extension uses Global/Pass/Material/Batch sets 0/1/2/3.

The user approved the exact 13-file mapping in migration.md; those moves are implemented. Shared C++ declaration locations and production target ownership remain unchanged.

## Technique selection and program ownership

| Execution kind | Technique selection | Resource sources |
|---|---|---|
| ScenePass | Each RenderQueue declares an accepted Tag; eligible scene items contribute the matching material technique | Global, Pass, Material and Feature Batch contributions |
| FullScreenPass | The Pass explicitly references its technique | Global/Pass as used; Batch only if explicitly declared and provided |
| ComputePass | The Pass explicitly references its technique | Global/Pass as used; Batch only if explicitly declared and provided |

A ScenePass may declare multiple queues. Culling, Tag matching and queue sorting preserve the selected technique on each draw. No matching technique means the item does not participate in that queue. Multiple techniques matching the same queue need an explicit selection policy; the initial implementation reports ambiguity instead of relying on array order. A matching but invalid technique reports an error; it does not silently choose another shader. The same material can provide different opaque and shadow programs, including its own deformation or alpha testing.

For OpaquePass, the actual Unlit, PBR or user-defined shader includes the exported Global and OpaquePass interfaces, adds its required material/Feature interfaces, and supplies the entry functions. The Pass defines inputs, outputs and resource contracts; it does not own a universal complete scene shader. Remove the implicit `SceneForward.slang` selection for an empty ShaderRef. Any default material is chosen by an explicit host/material policy and follows the normal technique/binding/cook path.

FullScreenPass and ComputePass do not need a scene, RenderQueue, matching Tag or dummy material. Reuse the existing technique description and shader resolution mechanisms, factoring shared value data only as needed; do not introduce a second technique library. Technique program descriptions must resolve explicit entry names and stages, including custom names, rather than infer a mandatory `VSMain/FSMain` or `entry + VS/FS` convention. Fullscreen validates graphics entries and output compatibility; compute validates compute-only execution. Dispatch dimensions belong to the Pass invocation, not the technique.

## Resource tiers and View ownership

| Tier | Scope and supplier | Examples |
|---|---|---|
| Global, set 0 | Pipeline-selected data common to the frame across its views | Frame time and frame index, if used |
| Pass, set 1 | Resources for one Pass invocation, assembled by the concrete Pass | Optional camera/view/projection, viewport, lighting, shadow map, IBL, fullscreen inputs or compute inputs/outputs |
| Material, set 2 | Authored material values and resources | At most one cbuffer, textures and samplers |
| Batch, set 3 | Draw or explicit invocation data | Feature transforms, instance data, palette, UI image/sampler |

Merge View shader parameters into Pass, without a separate View binding tier or mandatory camera block. A Pass may reuse a common camera declaration fragment within its own header. Remove duplicated camera matrices/position from Global; Global must not secretly vary by active view. CPU-side SceneView/PreparedViewData and view scheduling remain useful and are not removed.

Every view/Pass invocation captures its own Pass values and resource versions. Main-camera, secondary-camera and shadow-camera passes cannot overwrite a common mutable camera buffer while another invocation uses it. Passes without camera data omit those fields and require no dummy View. Scope follows the resource's use, not the producer's name: a Feature can produce a shadow map, but the consuming Pass binds that shared input in set 1.

### Batch composition

Material data supplies authored properties, textures, samplers and instance overrides. Features supply scene/draw data such as world transforms, instance inputs and supported deformation resources. The selected technique's interface identifies the required bindings and their providers; reflection validates the resulting layout but cannot infer who owns a value.

Material and Feature data occupy separate sets 2 and 3. Each binding has one declared provider; multiple Features may contribute distinct Batch bindings. One assembler owns each complete block; do not combine cross-set fields, overlap writes or use last-writer-wins behavior. This supersedes this stage's original combined set-2 design.

Compose and validate all contributions before replacing draw bindings. Reject duplicate providers, missing active resources, unsupported resource kinds and type/layout mismatches. MaterialPropertyMap's CPU offsets are not shader uniform offsets: pack supported property types into the declared target layout and check actual reflection. Never assume `SceneBatchParams{tint, world}`, copy arbitrary material bytes into that struct, or require that layout in the generic renderer. Unused declarations do not require values.

FullScreenPass and ComputePass omit Batch when their technique has no active set-3 resources. Their changing per-invocation data can all live in Pass. If an explicitly selected technique does require Batch, the caller must provide it; absent Batch is not permission to ignore reflected bindings. Reuse existing raster/compute validation and RDG resource/access declarations. The current raster helper's supported types do not imply automatic support for every compute/storage/array feature.

PreparedRaster continues to resolve geometry, resource versions and owned snapshots. Carry selected technique information and binding inputs through the existing resource/feature path; do not make PreparedRaster define material layouts or render-queue policy. Material and Feature snapshots retain their normal resource/module leases until accepted frame/GPU use completes.

## Shared declarations and shader header export

The Pass owner authors a constrained shared `.h` declaration containing the resource/field lists, binding numbers and supported layout metadata. C++ includes that source directly. Host-side export tooling integrated with CMake produces a pure `.slangh` asset. Slang includes this persisted shader header. Only explicitly registered shared declarations are export inputs; an ordinary C++ Pass class header is not automatically translated.

The exporter accepts the supported declaration grammar; it is not a general C++ parser or compiler. Unsupported constructs fail with source diagnostics. It does not invoke Slang or execute a target-platform binary. CMake tracks declaration/helper inputs, exporter version and output mappings, and exposes export/update and read-only verification operations. Unchanged inputs produce identical bytes without rewriting the output. C++-only targets do not wait for shader compilation or shader-header export.

Generated `.slangh` files are persistent, version-controlled shader assets beside handwritten `.slang` and `.slangh` files under the module's `assets/shaders/` root. Export preserves the shared declaration's domain-relative include path, changing only `.h` to `.slangh`: `aurora/pipeline/OpaquePass.h` becomes `aurora/pipeline/OpaquePass.slangh`, physically at `engine/aurora/assets/shaders/aurora/pipeline/OpaquePass.slangh`. A plugin declaration `toon/pipeline/ToonPass.h` exports to `plugins/toon/assets/shaders/toon/pipeline/ToonPass.slangh`. Preserve source spelling; these examples do not authorize case renames or relocation of the C++ header itself.

The authoritative `.h` remains the only editable source for an exported declaration; its `.slangh` is committed with the corresponding change. Handwritten shader helpers are separately owned files and cannot share an export's output path. Generated headers carry deterministic source provenance without build-machine paths, timestamps or configuration-specific content. Build directories may hold temporary comparison files, never the sole consumable copy of an exported interface.

Shader build/cook from a source checkout and CI verify exports against their registered declaration/helper inputs. Missing, stale or manually modified exports produce actionable errors rather than silently shipping mismatched layouts. Updating exports is an explicit development build operation; ordinary consumers and runtime loading do not rewrite source assets. Export verification does not require Slang. CMake clean must preserve tracked `.slangh` assets, and configuration/backend selection must not overwrite a shared asset with incompatible variants. Renames/removals update the registered output inventory and affected tracked exports together, without deleting handwritten helpers.

No complete VS/FS/CS implementation is generated from a Pass declaration. Validate C++ expected layouts against actual SPIR-V/DXIL/MSL reflection for supported fields; the exporter does not replace backend layout validation.

## Module ownership and unified resource registration

Aurora remains foundation under existing `engine/aurora` targets. It owns generic declarations, binding validation, compiler/cache interfaces and frame execution. Concrete pipeline/plugin implementations own their Passes and shaders. Framework/build tooling collects module resources; existing Aurora adaptor integration converts collected shader rules to the generic filesystem/resolver inputs. Aurora Core does not depend on plugin descriptors, Framework discovery, runtime plugin DLLs or the built-in Pipeline target.

Current `cmake/plugin_config.cmake` reads only target dependencies from `plugin.json`; `AssetDataBase::RebuildMounts` only creates workspace/engine mounts. There is no complete shared import registry to call today. Extend existing plugin/module metadata with a declarative resource contribution instead of adding unrelated shader registration lists. Resource collection is separate from runtime module activation; compiling a plugin alone does not select its resources for every project.

Each selected module owner registers one resource description; Aurora's internal targets contribute to Aurora's asset root, and a plugin's targets contribute to that plugin's asset root. Common asset/shader fields cover owner identity, physical source roots, logical namespace, file discovery/inventory, declared dependencies and distribution ownership. Shader-specific fields cover include exports, shared-header export mappings and shader program/variant requirements. Asset UUIDs, importer selection and product semantics remain asset-specific; ShaderRef remains path-based. Merging registration does not make shaders UUID assets or alter existing asset shadowing rules.

The descriptor is available to build tools, project assembly and offline cook without loading a runtime rendering DLL. Aurora, project modules and static/native plugins use the same resource-description contract even when they do not have a business-plugin descriptor. The project selects enabled contributions and their declared resource dependencies; the compiler, asset tooling and packager consume this collected description rather than independently scanning unrelated roots. Resource dependencies are explicit and are not inferred solely from CMake link dependencies.

Custom IFileSystem implementations provide an immutable file inventory plus their provider/lifetime handle through the same registration boundary. Existing IFileSystem has no enumeration API, so the integration cannot assume arbitrary mounts are discoverable. Native sources can generate inventories during collection, including persisted exports and their provenance; package providers use their manifest. Each export is inventoried once as a file in its module asset root, not mounted again from a generated directory. A custom provider used by offline cook must have an offline-accessible provider or staged equivalent, with equivalent canonical paths and content. No second VFS or shader-only plugin loader is proposed.

## Uniform roots, includes and path validation

All shader-bearing modules collect handwritten `.slang`, handwritten helpers, companion metadata and persisted generated `.slangh` under `<module-root>/assets/shaders/`. A physical resource root is distinct from the domain-relative shader path inside it. Internal CMake targets do not create separate shader roots by default. Configured filesystem providers may supply equivalent contents while preserving those paths.

| Owner | Registered source root | Paths inside the root |
|---|---|---|
| Aurora foundation | `engine/aurora/assets/shaders/` | `aurora/pipeline/OpaquePass.slangh`, `aurora/materials/Unlit.slang` |
| Plugin `toon` | `plugins/toon/assets/shaders/` | `toon/pipeline/ToonPass.slangh`, `toon/materials/Toon.slang` |
| Other plugin | `plugins/<plugin-id>/assets/shaders/` | `<plugin-namespace>/...` |
| Project owner | `<project-root>/assets/shaders/` | `<project-namespace>/...` |

The Aurora and per-plugin root policy and migration.md file mapping are confirmed and implemented. Root-relative paths are already canonical: includes and ShaderRef do not contain the physical `assets/shaders/` or repository `plugins/` prefixes. A plugin program `toon/materials/Toon.slang` can include `aurora/pipeline/GlobalBlock.slangh`, `aurora/pipeline/OpaquePass.slangh` and `toon/pipeline/ToonPass.slangh` when their interfaces are relevant and dependencies declared. Each plugin keeps its own root rather than contributing to a repository-wide `plugins/assets/shaders/` directory. Registered namespaces must remain unique across Aurora, plugins and the project.

- ShaderRef, includes/imports, dependency records, cache keys and package entries share canonical relative paths.
- Qualified includes resolve only within the owner and declared resource dependencies. A quoted local include resolves relative to the including logical directory and permitted include roots; ambiguous candidates fail rather than depend on search order.
- Include-root aliases must map unambiguously to canonical paths. Basenames may repeat in separate owners; there is no cross-owner basename fallback.
- Paths use `/`, reject absolute paths/traversal/invalid components and preserve one spelling. Case-only collisions fail for portable packaging without silently lowercasing ShaderRef identities.
- Validate owner identity, complete inventories, persisted export ownership, include mappings and dependency closure before publishing. An export colliding with a handwritten asset fails before any overwrite. Conflicts identify both providers and leave existing registrations unchanged. Reuse a dependency registration instead of mounting it again.
- Keep general-purpose MultiFileSystem's existing ordered behavior. The shader adapter validates contributions before composing it, so order cannot select between conflicting shader paths.
- Select development or package providers explicitly for a logical resource; do not mount both copies and rely on priority. Accepted compilation retains immutable inventories/content and provider leases. Macro includes and Slang imports must be covered by compiler-observed dependencies or a conservative complete declared closure, not a literal-include-only scan.

## Project cook, packaging and runtime loading

Project assembly collects one validated resource graph from registered module `assets/shaders/` roots for development, cook and packaging. Persisted `.slangh` files are collected alongside `.slang` and companion files, with the same canonical paths and no separate build-tree export mount. Source-checkout cook checks export freshness before compilation. Normal project cook gathers material technique requests, explicit FullScreenPass/ComputePass requests and their variants/dependencies. Runtime usage recording can supplement this list but is not the sole source of shipping requirements.

Reuse AssetTool/AssetBuilderManager, existing shader compiler/cache facilities and `package_project.py`. Compiled programs, reflection, schema and required variants enter the selected product distribution without manual per-plugin cook/copy commands. Preserve ShaderRef path identity; neither a second archive format nor a UUID for every include is required.

The recommended packaging path aggregates the selected shader cache records into one distribution index per target, using existing index/blob formats and retaining all selected owners' records. Conflicting request identities/content fail explicitly. Do not stack each plugin's `<target>/index.bin` under MultiFileSystem and let its first match hide the other indices. Source exports remain persisted in version control; this does not require including them in every shipping runtime package. Source/SDK distributions collect `.slang` and `.slangh` from the same module asset roots without requiring each consumer to regenerate them. Runtime shipping can omit sources once all declared program requirements are satisfied.

Production resolution receives the shipped product/cache roots. Development source compilation is an explicit mode; null cache roots plus embedded scene compilation are not the package contract. Acceptance runs a relocated package with source roots unavailable and runtime Slang absent.

## Integration points and execution order

| Area | Existing integration points | Required correction |
|---|---|---|
| Technique selection | Material, SceneAssetSource, RenderMaterialData, RendererRuntime, FrameComposer, SceneRenderFeature | Preserve techniques and actual entries; select by queue Tag; explicit fullscreen/compute selection |
| Binding | SharedShader, PassResourceBindings, scene shared declarations, existing frame validation | Material/Feature Batch ownership; optional Batch; View in Pass; no fixed scene ABI |
| Header export | CMake shader helper and owner declarations | Persistent version-controlled `.h` to `.slangh` exports, deterministic updates and read-only freshness checks |
| Registration | plugin/module metadata, plugin_config.cmake, asset mounts, Aurora adaptor | One declarative resource contribution plus inventory; no Core-to-Framework/plugin dependency |
| Distribution | Aurora material cook, shader cook/cache, AssetTool, package_project.py, runtime resolver | Unified request collection and selected-distribution index/product lookup |

First implement the declaration exporter and resource-collection contract with small owner fixtures. Then correct technique selection and binding, including View migration; integrate normal project cook and source-free loading; remove superseded registration, fallback and deployment code after consumers migrate. Keep existing frame submission/retirement mechanisms and targets. Present concrete physical directory moves separately before executing them.

Acceptance covers queue Tag matching and absent/ambiguous Tags; Unlit/PBR/custom and material-specific shadow programs; explicit arbitrary entry names; fullscreen/compute without scene or Batch; mixed material/Feature Batch with collision/missing/type errors; multiple views/shadow cameras with independent Pass values; deterministic persistent export, stale-output detection, clean-build preservation and layout reflection; custom provider inventories, local/macro includes/imports and path collisions; dependency invalidation and retirement; one relocated package containing multiple owners without source/compiler fallback.

Validation is recorded by implementation stage in tasks.md. This change's macOS native acceptance and cross-compilation tasks pass; mobile device acceptance remains in platform-parity. Commit and archive still require the user's confirmation.

## Implementation boundaries

Current payloads are Material 4, MaterialInstance 2, shader cache 10 and usage 3. Explicit stage programs and canonical typed values replace the old single-shader form and hand-authored uniform mappings. Supported legacy aliases migrate through exact declaration metadata; incompatible products require recook. Complete declarations remain distinct from active target reflection.

Built-in Batch set 3 supplies world, receive-shadow/surface and skinning palette inputs. Material set 2 supplies its one optional cbuffer plus authored textures/samplers. Core accepts supported explicit layouts; UniformUploadPlan encodes target bytes and parameter transactions reject overlapping providers. Each accepted frame retains its own values and exact resource versions.

Texture/sampler asset transport and skinning were added by the target-artifact and skeletal-animation changes. Descriptor arrays and a general material PSO cache remain separate work; old stage acceptance does not prove these later paths or native Metal changes. The later `remove-unused-legacy-rendergraph` change removes the unused compatibility templates; production remains on FrameGraph.

The editor's ordinary asset deployment excludes the shader child directory, leaving the module collector as its sole owner. The assets container remains a real directory, so staging cannot write through an engine-source symlink.

## Superseded content cleanup

The user requested obsolete data, documentation and code cleanup after migration. The
unconsumed root `assets/shaders` tree (34 HLSL/helper/variant files), six legacy `.mat`
files and `assets/vertex/vertex_library.vtxlib` are removed. They do not describe the
current material format and have no active code consumers. Fonts, shapes, textures
and current owner-scoped shader assets remain in place.

`ShaderBlockGen` and its four text/hash tests are removed; persistent `.slangh` export
is the declaration path. `RgBlockDesc` and its CPU layout test remain, as does the
variant-only `ShaderHeaderTool`. Unused old shader include macros and redundant Pass
lifecycle forwarders are removed. The later legacy graph removal retires its test-only template consumers while preserving
shared shader declarations and the production FrameGraph path.

Local obsolete `shader-bundles` staging trees and the generated `SceneShaderBundle.h`
are removed. Current shader products and unrelated build caches are preserved.
Current reference documents point to this contract; earlier change records are
marked as superseded where appropriate and keep their dated validation evidence.

## Metal physical binding correction

Logical `(set, binding)` keys do not establish unique Metal argument indices, and an active-resource query must use the native category/index namespace expected by the compiler metadata. A binding can occupy a different native index in VS and FS after stage optimization. Neither collapsing register spaces nor counting resources in merged declaration order is a valid mapping rule.

An MSL session includes a second, layout-only SPIRV target over the same source and preprocessor configuration. Matching parameters by name obtains authored logical keys from that declaration layout, including resources whose Metal layout reports mixed native categories. No SPIRV bytecode is generated. Metal layout remains the sole source of block offsets and sizes, physical argument indices and active-resource metadata; SPIRV packing is not used for Metal data.

Keep authored logical keys unchanged. Each compiled stage carries `ShaderReflection.nativeBindings`, whose `ShaderNativeBinding` records `set`, `binding`, one shader `stage` and the physical `index`; the matching logical resource supplies the buffer/texture/sampler category. Metal code generation and native encoder binding consume the same mapping, with collision and capacity errors reported explicitly. A native metadata query must not silently discard a live resource because it was given a logical descriptor key.

Technique and raster resolution preserve independent stage mappings, deduplicate identical `(set, binding, stage, index)` records and reject conflicting indices for the same logical stage key. Cache version 5 serializes these records; decoding rejects invalid stage values, truncated records and oversized counts. Packet resource accounting includes the additional owned records. The stripped scene/compute/fullscreen readbacks must pass before native acceptance is complete; codec or reflection checks alone do not establish that result.

## Planned shader ABI follow-up

The 2026-10-22 design review is tracked by `refactor-aurora-target-shader-artifacts`.
That proposed change replaces the auxiliary SPIR-V/name bridge above, makes native
reflection completeness and artifact identity explicit, and separates CPU parameter
layout from target ABI through UniformUploadPlan. It retains this change's resource
ownership, exports, registration, technique and packaging baseline. These corrections
are planned rather than delivered by the completed tasks recorded here.
