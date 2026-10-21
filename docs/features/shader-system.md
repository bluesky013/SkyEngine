---
title: "Shader System"
description: "Pass-owned shared headers, shader compilation and binding validation."
module: "aurora"
updated: "2026-10-21"
---

## Shared declarations

Global and each Pass/Batch own a source header in `engine/aurora/pipeline/shaders/`.
For example, `ScenePass.h` declares the optional camera block; `SceneBatch.h` declares
tint and world transform. C++ and Slang include these same files. Global/Pass/Batch
remain sets 0/1/2; other passes may omit View or unused resource tiers.

A field list and resource list are written once. `aurora/shader/SharedShader.h`
expands them into native C++ structs, typed slots, expected reflection and the
legacy `RgBlockDesc` bridge, or into shader structs/resources. C++ names live in
`sky::aurora::shared`. Uniform resources use explicit set/binding ConstantBuffers;
matrices are column-major, matching `Matrix4`. The current flat uniform subset
supports float/int/uint, float2/float4 and mat4 fields and bounded float4 arrays; BOOL/FLOAT3 uniform fields (use UINT/FLOAT4), nested structs, storage-buffer declarations, descriptor arrays
and arbitrary uniform arrays are unsupported. Layout compatibility must be tested
for each target; no C++ packing algorithm substitutes for target reflection.

Lighting, shadow and environment declarations follow the same route:
`SceneLightingPass.h`, `ShadowPass.h`, `SceneEnvironmentPass.h`, `ToneMapPass.h`.
Resource metadata describes shapes; it does not automatically enable a raster
backend capability. `PassResourceBindings` validates actual resource use, uniform
size/member offsets/types/array strides and explicit static/dynamic policy before
changing a draw. Unused declarations need no value; mismatches return errors.

## Build and runtime

C++ compilation consumes checked-in source headers directly. It does not run Slang
or ShaderHeaderTool. CMake only embeds shared headers and shader implementations
in `build/engine/aurora/pipeline/generated/SceneShaderSources.h` for packaged virtual
includes. One CMake path list supplies both the text and its virtual include name;
registration iterates that table. These are text copies, not ABI mirrors. Header
edits trigger embedding and consumer rebuilds; generated strings stay in the build tree.

`RegisterSceneShaderSources` registers the shared helper, declaration headers and
scene implementations in `ShaderFileSystem`. Slang resolves both quoted and angle
includes through this filesystem. SPIR-V, DXIL and MSL are compiled independently.
The source closure hash follows these includes too, so shared-helper edits invalidate
cached shaders. Quoted includes search the including directory before the logical root.
`Aurora.ShaderCompiler` provides runtime source compilation; cache-only deployments
can omit that module. `ShaderCookTool` remains the offline cache builder.

`ShaderHeaders`, `GenerateShaderHeaders.cmake`, reflected C++ mirror generation,
and `AURORA_SHADER_HEADER_TOOL` are retired. `ShaderHeaderTool --variant` remains
available for variant schemas and no longer links Slang. Cross builds need no
host tool to obtain pass C++ types; shader cooking retains its separate toolchain.

## Changing a Pass

1. Edit its shared header's field/resource lists, keeping set/binding ownership explicit.
2. Update the consuming Pass and shader implementation together. For a new packaged
   source, add its virtual path to Pipeline's `SCENE_SHADER_PATHS` list.
3. Add the declaration to `SharedPassHeadersTest` and run Pipeline/Shader tests.
   They compare native C++ metadata with target reflection, including arrays and resources.
4. Run affected renderer GPU tests. Windows MSL source/reflection checks do not
   replace native Metal compilation, binding and readback on macOS.

Historical codegen plans are superseded by
`openspec/changes/refactor-aurora-shared-pass-headers`.

## Example: ScenePass reaches Slang

`SceneForward.slang` includes `ScenePass.h`, which includes `SharedShader.h` and
invokes `AURORA_SHADER_PASS`. The shader branch expands the declaration to:

```hlsl
struct ScenePassParams {
    column_major float4x4 viewProjection;
    float4 cameraPosition;
};
[[vk::binding(0, 1)]] ConstantBuffer<ScenePassParams> gScenePass : register(b0, space1);
```

This happens in Slang's preprocessor; no separate `.slang` header is generated.
C++ includes the same source header and selects its C++ branch using `__cplusplus`.
For packaged shaders, CMake reads the source text into `SceneShaderSources.h`,
`RegisterSceneShaderSources` publishes that table to the virtual filesystem, and
Slang resolves the same includes there before compiling the shader entry point.

## User pipeline ownership follow-up

The current source embedding and `sky::aurora::shared` namespace describe the
built-in pipeline integration. User pipeline/plugin declarations, owner-qualified
resource paths and shared runtime/cook bundle inputs are a separate proposed change:
`openspec/changes/refactor-aurora-pipeline-resource-ownership`. These capabilities
are not yet implemented. User resources remain owned by their plugins;
Global/Pass/Batch binding tiers do not determine directory or module ownership.
