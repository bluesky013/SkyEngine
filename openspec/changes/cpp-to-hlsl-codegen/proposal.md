## Why

SkyEngine's 3-tier shader resource group system (PerPass / PerBatch / PerInstance) requires manual alignment between C++ struct definitions and HLSL shader resource declarations for the PerPass and PerInstance tiers. Every time a C++ struct like `ShaderPassInfo` or `InstanceLocal` changes, the corresponding HLSL header (`default_global_view.hlslh`, `default_local.hlslh`) must be updated manually—and vice versa. This is error-prone, produces silent runtime bugs (wrong data layout), and slows down iteration. The PerBatch tier is already mostly automated via Material reflection, but PerPass and PerInstance have no such automation.

A codegen tool that generates HLSL layout headers from C++ source-of-truth definitions eliminates this class of bugs entirely and makes C++ the single authoritative definition for shared data structures.

## What Changes

- **New build tool**: A codegen utility that reads annotated C++ struct/layout definitions and emits HLSL header files (`.hlslh`) with matching `cbuffer` declarations, resource bindings, and struct definitions.
- **C++ annotation system**: Lightweight macros or metadata attributes on C++ structs (`RenderBuiltinLayout.h`, `MeshFeature` bindings, pass resource declarations) that mark them as shader-visible and specify set/binding/type.
- **Generated HLSL headers**: `default_global_view.hlslh`, `default_local.hlslh`, and similar layout headers become generated outputs rather than hand-written files. They are produced by the codegen tool during the build.
- **Build integration**: CMake integration to run codegen before shader compilation, with proper dependency tracking so shader rebuilds trigger when C++ source definitions change.
- **Removal of manual duplication**: Hardcoded descriptor layout vectors in `MeshFeature::Init()` (`BINDINGS`, `MESH_BINDINGS`) and manual `ComputeResource` declarations in pass implementations (`ForwardMSAAPass`, etc.) are replaced by codegen-derived data or reflection-driven initialization.

## Capabilities

### New Capabilities
- `hlsl-codegen-tool`: The standalone codegen tool/module that parses annotated C++ definitions and emits HLSL header files. Covers struct-to-cbuffer mapping, resource binding emission, and type translation (Matrix4→float4x4, Vector4→float4, etc.).
- `cpp-shader-annotations`: The C++ macro/attribute system for marking structs and resource bindings as shader-visible. Covers annotation syntax, set/binding/type specification, and integration with existing `RenderBuiltinLayout.h` and `MeshFeature` patterns.
- `build-integration`: CMake build integration that runs codegen at the right time, tracks dependencies between C++ source definitions and generated HLSL headers, and ensures shader recompilation when layouts change.

### Modified Capabilities
<!-- No existing specs to modify -->

## Impact

- **`engine/render/core/include/render/RenderBuiltinLayout.h`**: Structs (`InstanceLocal`, `ShaderPassInfo`, `SceneViewInfo`, `MeshletInfo`) gain codegen annotations.
- **`engine/render/core/src/mesh/MeshFeature.cpp`**: Hardcoded `BINDINGS`/`MESH_BINDINGS` vectors replaced by codegen-derived or reflection-driven layout initialization.
- **`engine/render/adaptor/src/pipeline/ForwardMSAAPass.cpp`** (and similar passes): Manual `ComputeResource` declarations replaced by generated or reflected layouts.
- **`assets/shaders/layout/default_global_view.hlslh`**, **`default_local.hlslh`**, **`default_pass.hlslh`**: Become generated files (moved to build output or marked as generated).
- **`engine/render/shader/`**: Codegen tool lives here or in a new sibling module.
- **Build system**: New CMake targets for codegen step, dependency tracking between C++ annotations and HLSL outputs.
- **No runtime API changes**: The `ResourceGroup`, `Program`, and `Material` runtime APIs remain unchanged. This is purely a build-time improvement.
