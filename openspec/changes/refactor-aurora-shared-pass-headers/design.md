## Declaration ownership

Global and each Pass/Batch own one shared `.h`. Field and resource lists are written once, including binding numbers and array counts. C++ expands them into native data plus expected schema; Slang expands them into shader declarations. C++ types and offsetof/sizeof describe the CPU layout, not a hand-written GPU packing algorithm. Compiled shader reflection must match before binding.

The shared field subset includes scalar float/int/uint, float2/float4, column-major float4x4 and bounded float4 arrays. BOOL/FLOAT3 uniform fields are deliberately excluded because Metal uses different sizes; use UINT/FLOAT4. Nested structs, descriptor arrays and other uniform array elements are unsupported and fail compilation instead of silently losing fields. Resource declarations retain typed slots and shape metadata. Global/Pass/Batch remain sets 0/1/2 and View remains optional.

All current flat uniform blocks use explicit set/binding ConstantBuffer declarations. Slang lowers ParameterBlock to Metal argument-buffer layouts, which do not match the current direct buffer binding contract; no RHI tier or descriptor policy changes.

## Build graph

Aurora.Pipeline.Static directly includes the shared headers and depends only on its existing Core/RHI libraries. CMake embeds the very same headers and shader implementations as strings for virtual shader includes. Text embedding does not invoke ShaderHeaderTool or Slang. Shader compilation, cooking and reflection tests continue to use Slang independently.

The CMake `SCENE_SHADER_PATHS` list owns packaged virtual include names. It emits one `SceneShaderSources.h` table of paths and source text; registration iterates that table without a second hand-maintained list. Source content remains tracked by `CMAKE_CONFIGURE_DEPENDS`.

Scene `BuildView` orchestrates target, lighting/environment, draw binding, depth and tone mapping. Private helpers decode view settings and bind draws; static/dynamic pass uniforms are cached separately for the current view. Uniform writes share the existing typed helper, and shadow/tone-map shader creation shares a local compile/create helper. No public API or target changes are needed.

The compiler registers the logical include root for angle includes. ShaderResolver hashes their transitive contents; quoted includes search the including directory then the root. The conservative scanner may encounter inactive C++ branches whose headers are absent from packaged shader sources, so missing nested files are left for actual compilation to diagnose.

ShaderHeaders and GenerateShaderHeaders.cmake are removed. ShaderHeaderTool retains only its existing variant-schema mode and links Aurora.Core.Static instead of Aurora.ShaderCompiler.Static. No new target or module ownership is introduced. The reflected C++ mirror generator and its old declaration sources are removed; virtual include/reflection coverage remains.

## Validation

- Compile shared C++ declarations and the Pipeline target without building/running a shader-header generator.
- Compare all ten declarations against SPIR-V/DXIL/MSL compiler reflection, including members, offsets, sizes, arrays and resource shapes; native Metal execution remains a macOS gate.
- Run Pipeline, shader and GPU renderer regressions, plus a negative layout-mismatch case.
- Verify source embedding tracks header changes and works through the production virtual file system.
- Check current docs/rules for obsolete production codegen instructions. Keep archived historical records, with supersession notes where active changes overlap.

## Scope of shared declarations

The initial helper enables uniform blocks, textures and samplers. Storage-buffer and descriptor-array declarations are not exposed by this helper. An exploratory MSL fixture omitted RWStructuredBuffer from logical reflection; fixing that general compiler/backend path is outside this Pass-header refactor. Existing Compute/Custom interfaces remain unchanged. Shader-only implementations may still use their separately validated resource paths.

## Follow-up ownership boundary

This stage establishes the built-in pipeline's shared source and independent C++ build. Its fixed shared C++ namespace and scene-specific registration table are not a universal plugin packaging contract. Owner-qualified shader paths, configurable C++ namespaces, reusable foundation bindings, explicit custom pipeline assembly and runtime/cook bundle equivalence are specified separately in `../refactor-aurora-pipeline-resource-ownership/`; do not mix that helper/directory ownership migration into this completed stage. Source ownership, binding tiers and physical deployment locations remain distinct concepts.
