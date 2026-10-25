# Implementation and verification — 2026-10-25

Status: confirmed design implemented in the existing targets; Windows regression results are recorded below. No commit or archive has been performed. The existing staged skeletal/animation work is retained separately in the working tree.

## Delivered contract

- Core normalizes typed V/P/S selections. Vertex ownership comes from actual semantics; Pipeline and Shader inputs are checked independently. Material/Batch/fullscreen/compute supply S. Stage schema projection preserves distinct source symbols and native IDs.
- Slang 2026.19 inspects associated scalar declarations; bool/int32/uint32 values lower according to source and target. Native defaults, unsupported types/shapes and inconsistent compile declarations fail transactionally. Pure preprocessor keys remain compile-only.
- Full selection identity, stage code projection and executable identity are distinct. Native values share code without sharing the wrong executable. Vulkan maps and Metal function constants are stage-specific; DXIL folds selected values into code.
- Group 2 provides zero/one cbuffer, textures and samplers. Its complete declaration union plus S options generates Material keys. Canonical values retain inactive fields, validate types and bind through the final target's existing parameter transaction/upload plan. Legacy aliases require exact mappings. Static options remain on Material; instances only override ordinary values/resources.
- Dynamic Material fields use adaptor-owned typed models and reflection nodes bound to ReflectedFormView. Commands use document-local history, preserve inheritance on undo and expire with the bound generation. Existing IAssetViewDocument tick drains schema rebuilds and queued saves outside input dispatch. Save validates before source write/cook. Legacy technique/render-state controls are unchanged in scope.
- Scene Pipeline preparation uses four explicitly registered SHADOWS/IBL requests, not enumeration of arbitrary option ranges. Scene tone-map/highlight programs prepare before graph declarations/output acquisition. Existing feature extension APIs remain available; external features prepare their own programs.
- Published indices retain complete allowed selections, including native values. Unpublished selections fail in PRODUCTS mode even when generic code exists. Material v4, index/blob v10 and usage v3 are current; instance v2 and distribution manifest v2 remain unchanged.
- The optional 128-bit packed representation is separate from bounded logical metadata. Large logical selections remain valid; packed access rejects unrepresentable schemas. Source names and bounds are validated before decoding/publication.

## Windows verification

Environment: VS 2022 Debug, existing cmake-build-debug; dependency runtime D:/Code/Engine/sky_3rd/Win32; NVIDIA RTX 3080. Builds run serial MSBuild targets with /m:1 /nr:false /p:CL_MPCount=1. Native tests run outside the restricted process sandbox; temporary file tests require writable OS temp directories.

| Check | Latest result |
|---|---|
| Build Core/Pipeline/Adaptor/Shader/RHI, Renderer module/GPU tests, Aurora.Render.Editor, SandboxModule, Framework/Editor tests | Passed, including final packed-key hardening and document history/tick changes |
| Aurora.CoreTest excluding standalone ShaderDistribution | 106 passed after boundary hardening |
| Aurora.ShaderTest | 64 passed, including VS uint/id0 and FS int/id0 independent GPU values on Vulkan/DX12 |
| Aurora.AdaptorTest | 65 passed; includes generated-key/alias migration, inactive type checks, typed serialization and reflection model |
| Aurora.CookTest | 52 passed |
| EditorCoreTest / EditorShellTest | 179 / 143 passed |
| FrameworkTest *BinaryArchive* | 6 passed |
| Python shader project/header tests | 20 passed |
| Shared header export check | Passed |
| OpenSpec strict validation | Passed after final task bookkeeping |
| Renderer runtime / RHI / Pipeline regressions | 167 runtime / 51 RHI / 56 Pipeline checks passed; final guard change also passed 9 GPU, 16 Pipeline and 13 material-focused tests |

`python python/tests/run_shader_package_tests.py` passed: it invoked project cook, relocated products, removed temporary sources, and ran a compiler-free Core consumer (1 test) plus Vulkan and DX12 consumers (2 tests each). The updated consumers were copied into separate compiler-free hosts and rerun successfully with an additional unpublished-default-selection rejection. Compute/fullscreen use the nondefault PACKAGE_VALUE=1; scene products include the explicitly supported P/V choices. These tests do not substitute for a full material-editor interaction test.

Relevant logs are under cmake-build-debug/variant-*.log (local build artifacts). Failed intermediate probes were corrected before the passing runs: baseline fingerprint reset, readback heap/state, and matching DXIL VS/PS input/output signatures in the new stage-isolation fixture. Old tests which treated known inactive keys as errors or imposed 128 bits on all logical selections were updated to the confirmed contract. The packed-key boundary pass also corrected old fixtures whose source width exceeded totalBits; the production validation remains strict.

## Remaining acceptance boundaries

- macOS native Metal execution and stripped products: unavailable on this Windows host. Task 6.4 stays unchecked. MSL generation/reflection passes do not claim native Metal execution.
- Manual material-editor interactions (including instance override undo in a live viewer) are not claimed from model tests/builds. Generic reflected editing/history suites pass; a live editor acceptance pass remains separate.
- This change does not introduce material graphs, instance static overrides, arbitrary Material storage buffers, native variable thread-group metadata, new targets, or directory moves.


## Final commands and result

Final shader rerun completed at 21:21 local time: 64/64 passed after the fixture correction. Core's final boundary suite passed 106/106. All touched Windows modules and tools build successfully, including Aurora.Render.Editor and SandboxModule. OpenSpec tasks are 22/23 complete; only the explicit macOS native gate is unchecked.

```text
cmake --build cmake-build-debug --config Debug --target Aurora.CoreTest Aurora.PipelineTest Aurora.ShaderTest Aurora.AdaptorTest Aurora.CookTest ShaderCookTool Aurora.Renderer.ModuleTest Aurora.Renderer.GpuSubmissionTest Aurora.RHITest Aurora.Render.Editor SandboxModule FrameworkTest EditorCoreTest EditorShellTest -- /m:1 /nr:false /p:CL_MPCount=1 /v:minimal
python -m unittest python.tests.test_shader_project python.tests.test_shader_headers
python python/shader_headers.py --config engine/aurora/resources.json --check
python python/tests/run_shader_package_tests.py
openspec validate refactor-aurora-shader-source-techniques --strict
git diff --check
```

The build targets above were executed in several batches to keep loaded compiler DLLs separate from post-build copies. Native coverage filters: RendererRuntimeTest + FrameFeatureRegistryTest + MaterialVariantSelection + selection-outline + shader-owner validation (167); ShaderTest/ShaderParameter/UniformUploadPlan/VulkanBindingPlan/ResourceGroup/PipelineState RHI suites (51). The later 9-test GPU rerun covers preparation failure, explicit compute/fullscreen and shader ownership with the final Core library. The final stripped-host rerun passed all five consumers/checks with the final binaries.
