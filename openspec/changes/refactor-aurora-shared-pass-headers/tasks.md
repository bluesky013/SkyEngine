## Implementation

- [x] 1. Add shared C++/shader declaration helpers and migrate the ten declaration headers.
- [x] 2. Migrate consumers and source embedding; remove Pipeline's shader-codegen dependency.
- [x] 3. Remove obsolete mirror-generation code and update active design/spec/rule documents.
- [x] 3.1. Consolidate packaged source registration into one CMake list and remove duplicate scene uniform/shader setup.
- [x] 3.2. Separate scene orchestration from settings/binding helpers; retire stale generated-header test names and local build outputs.

## Validation

- [x] 4. Verify native C++ compilation and compiled shader layout/resource agreement.
- [x] 5. Build affected targets and run Pipeline, shader and GPU renderer regressions.
- [x] 6. Check formatting, static analysis, scoped diffs and strict OpenSpec validation.

## Evidence (2026-10-21, Windows / VS 2022 Debug x64)

- Standalone MSVC compilation of all ten source headers passed with only Core/Aurora include roots, without Slang headers/libraries or generation. BOOL/FLOAT3 uniform negative fixtures fail compilation. Pipeline's five transitive build projects contain no ShaderHeaderTool/ShaderCompiler dependency.
- Pipeline, shader/compiler module, renderer/runtime, Core tests, ShaderHeaderTool, ShaderCookTool, SandboxModule and SandboxEditor builds passed. Embedded header text matches the source files; the retained `ShaderHeaderTool --variant` mode passed a real schema generation check.
- Aurora.CoreTest: 69/69; Aurora.PipelineTest: 39/39; Aurora.ShaderTest: 40/40.
- Shared-header tests compare all ten declarations against SPIR-V/DXIL/MSL reflection, compile all four packaged scene shaders without disk search paths, validate the supported field subset/resource-only declarations, and check transitive shared-helper cache invalidation.
- Aurora.Renderer.GpuSubmissionTest: 895 passed / 3 skipped (898 total), including the final source-closure changes. The three skips are Vulkan-specific surface cases instantiated for DX12.
- clang-format and scoped diff checks passed. clang-tidy reported only the existing recursive HashRec call-chain warning; no new warnings remain. New/affected changes and the current aurora-shader specification pass strict OpenSpec validation.
- Native Metal execution remains a macOS platform gate; Windows MSL compilation/reflection does not claim to satisfy it. See design.md for the intentionally bounded shared declaration subset.
- Logs and pre-refactor snapshots are under ignored `cmake-build-debug/shared-pass-*`. No archive or commit was performed.

## Cleanup validation (2026-10-21)

- Removed stale local `.gen.h`/`.gen.slang`/`.source.h` outputs before configuring; Pipeline now emits only `SceneShaderSources.h`. All 12 embedded entries match their source files and have unique virtual paths. The Pipeline library still has no shader compiler/header-tool project dependency.
- Consolidated scene uniform/shader setup and separated `BuildView` settings/binding logic. Scoped clang-tidy reports no warnings, including no cognitive-complexity warning for `BuildView`; scoped formatting/diff checks and strict OpenSpec validation pass.
- Rebuilt Pipeline, shader compiler static library, renderer module, SandboxModule, SandboxEditor and the affected test executables. A shader-DLL deployment collision with the running GPU tests was resolved by rebuilding after tests exited.
- Final binaries: Aurora.PipelineTest 39/39; RendererRuntimeTest + IndexedRaster GPU regressions 174/174 across Vulkan render-pass/dynamic-rendering and DX12. Native Metal execution remains unverified on Windows.
- Shared-block bridge tests now include `GlobalBlock.h` instead of duplicating its shader declaration. Test names and compiler comments use the current shared-header contract; the shader-system guide includes the ScenePass preprocessing flow.

## Stage closeout (2026-10-21)

- Current implementation scope and Windows acceptance are complete; no additional runtime code is changed during this closeout. The latest full GPU run is 895 passed / 3 skipped before the final cleanup; final cleanup acceptance is Pipeline 39/39 and the affected RendererRuntime/IndexedRaster GPU suites 174/174, not another full GPU run.
- Native Metal execution and the documented unsupported declaration subset remain explicit limits, not silently completed tasks.
- Follow-up `refactor-aurora-pipeline-resource-ownership` contains proposal/design/spec deltas and open implementation tasks for user pipeline/plugin ownership. It is not implemented by this checkpoint.
- Both changes remain unarchived and uncommitted. This stage's code/specs must be reviewed together; other in-progress workspace changes retain their existing status.
