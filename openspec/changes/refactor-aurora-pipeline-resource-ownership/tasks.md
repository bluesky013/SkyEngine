Checked items are backed by the current validation record below. Native Metal and a stripped native shipping-host run remain platform/packaging validation limits, not implied by Windows reflection or resolver tests.

## 1. Review and ownership

- [x] 1.1. Inventory existing CMake deployment/package helpers, plugin resource metadata and host pipeline registration; choose the smallest integration seams.
- [x] 1.2. Present and confirm the exact generic binding helper ownership/include migration and host assembly changes before implementation; keep existing Aurora targets.

## 2. Shared declarations and bindings

- [x] 2.1. Add owner-selectable C++ namespaces to shared declarations and migrate built-in declarations without duplicating fields or binding numbers.
- [x] 2.2. Make generic Pass binding validation usable without the built-in pipeline library; preserve transactional apply, explicit dynamic policy and frame lifetime.
- [x] 2.3. Verify earlier plugin fixtures can use identical local Pass/type names, optional camera inputs and independent Global/Batch layouts without shader codegen for C++.

## 3. Owner resources and compilation

- [x] 3.1. Extend plugin/module metadata with shared resource fields and shader exports; collect Aurora's assets/shaders root and each plugin's own assets/shaders root without runtime rendering DLLs or per-target shader-root registration.
- [x] 3.2. Require explicit inventories for custom IFileSystem providers and validate canonical paths, include ambiguity and case collisions before composing existing MultiFileSystem roots.
- [x] 3.3. Feed one module resource graph through development compilation and existing project cook/package entry without loading pipeline runtime modules during cook.
- [x] 3.4. Carry the earlier snapshot/hash mechanics into unified registration, including generated exports, macro includes/imports and stable ShaderRef/companion/usage identities.

## 4. Pipeline assembly and lifetime

- [x] 4.1. Assemble built-in or user pipelines explicitly through existing host feature commands, preserving the single producer/Renderer/Device contract.
- [x] 4.2. Retain immutable source/dependency snapshots for compilation and reuse module/resource leases through feature retirement, failure and discard.
- [x] 4.3. Verify source retirement during compilation and feature retirement with queued/GPU work; no implicit built-in fallback or parallel plugin submission loop.

## 5. Acceptance and documentation

- [x] 5.1. Extend the existing external-style fixtures to validate the corrected module roots, customized includes, collision rules and actual project packaging/runtime loading.
- [x] 5.2. Verify shared dependency edits invalidate both build inputs and shader cache results; inspect the external pipeline build graph for unwanted built-in/compiler dependencies.
- [x] 5.3. Run affected Core/Pipeline/Shader and renderer GPU regressions; report SPIR-V/DXIL/MSL reflection separately from native Metal execution.
- [x] 5.4. Update authoring/deployment/migration docs and the outdated skyengine-aurora-shader skill; remove superseded integration paths and complete appropriate validation after implementation.

## 6. Pass/material and module-path correction

- [x] 6.1. Record all six review decisions plus confirmed module assets/shaders roots, domain-relative export paths and version-controlled persistent .slangh assets.
- [x] 6.2. Present and obtain explicit approval for migration.md; relocate its 13 shader/companion files under the confirmed roots while preserving declaration sources and production targets.
- [x] 6.3. Preserve material techniques through existing asset/prepared-resource transport and select them by RenderQueue Tag; handle no-match/ambiguity explicitly and remove implicit SceneForward selection.
- [x] 6.4. Verify Unlit, the existing Lit/IBL material and a fully custom program in one OpaquePass queue, material-specific opaque/shadow techniques, custom entry names and unused shared declarations.
- [x] 6.5. Verify custom include roots, local includes, reversed mount order, source/generated collisions, case-only collisions and stable identities across native/staged/package inputs.
- [x] 6.6. Connect project cook, selected-distribution shader index aggregation and runtime product lookup; verify multiple owners through a relocated Core-only consumer without sources/compiler linkage, and native scene rendering with sources/compiler registration unavailable.
- [x] 6.7. Implement CMake export/update and read-only checking of persistent .slangh assets; preserve .h domain-relative paths, diagnose unsupported grammar, and keep C++ consumers independent of Slang/export execution.
- [x] 6.8. Support explicit FullScreenPass/ComputePass techniques with stage/entry validation and no scene/Tag/dummy-material requirement; preserve invocation-owned dispatch inputs.
- [x] 6.9. Compose material and Feature Batch inputs through explicit binding ownership and target-layout packing; remove fixed SceneBatchParams checks without moving layout or queue policy into PreparedRaster.
- [x] 6.10. Verify disjoint Batch providers, duplicate ownership, combined-block assembly, missing inputs and type/layout failures leave prior bindings intact.
- [x] 6.11. Verify fullscreen/compute execution without Batch and rejection when an active reflected Batch input is omitted; preserve existing resource/access capability checks.
- [x] 6.12. Move camera/projection/viewport shader inputs into Pass, remove Global camera duplication, and verify independent main/secondary/shadow Pass snapshots plus camera-free Passes.
- [x] 6.13. Validate shared declaration layouts against actual SPIR-V/DXIL/MSL reflection and run appropriate backend regressions; report native Metal validation separately.
- [x] 6.14. Verify persistent source exports survive build clean, remain unchanged on no-op export, diagnose stale/manual edits, protect handwritten helpers and have no build-configuration inputs. CI/cook integration remains in 3.3/6.6.
- [x] 6.15. Include persisted .slangh in the single module asset inventory and source/SDK staging; retire build-tree-only export paths and update renamed/deleted tracked exports with their declarations.

## 7. Remaining native acceptance

- [ ] 7.1. Run a stripped native shipping host with source directories and the Slang DLL physically absent; audit its module selection as well as resolver behavior. The existing native test has no registered compiler, but still links compiler code for other cases.
- [ ] 7.2. Run native Metal scene/compute/fullscreen and package validation on macOS.

## 8. Obsolete content cleanup

- [x] 8.1. Confirm no active code consumers and remove 34 legacy shader/helper/variant files, six old material assets, the vertex library and the retired shader-root placeholder.
- [x] 8.2. Remove ShaderBlockGen and its four obsolete tests, unused include macros and redundant Pass lifecycle forwarding; preserve CPU layout validation and the variant-only tool.
- [x] 8.3. Reconcile current authoring/reference/editor docs and superseded change records; distinguish legacy RenderGraph templates from the production FrameGraph pipeline.
- [x] 8.4. Remove verified obsolete local shader-bundles staging trees and generated source embedding without deleting current shader products.
- [x] 8.5. Reconfigure/rebuild affected targets, run related tests and persistent-export checks, verify editor startup and record results.

## Implementation status

2026-10-22: approved 13-file migration, corrected technique/resource architecture and obsolete-content cleanup are implemented. Shared C++ declaration locations and production targets are preserved. This checkpoint is ready for macOS validation; the remaining native acceptance items above stay open.

## Current validation (2026-10-22)

- Affected Debug targets build: Core, Pipeline, ShaderCompiler, Adaptor, Renderer GPU tests, ShaderCookTool, SandboxModule and SandboxEditor. The editor asset deploy now preserves the collector-owned shader directory; its regression checks that ordinary assets refresh without deleting the shader inventory or copying legacy shader files.
- CoreTest: 80/80; PipelineTest: 42/42; ShaderTest: 53/53; AdaptorTest: 33/33. Framework project/preset checks: 6/6. Export/inventory/project/schema Python suite: 25/25.
- RendererRuntime/IndexedRaster/FrameFeatureRegistry: 200/200 across Vulkan dynamic rendering, Vulkan render pass and DX12. After the final equivalent shader-validation cleanup, 21 affected native cases passed again. New coverage includes mixed Unlit/Lit/custom programs, actual opaque/shadow Tag selection, arbitrary stage entries, unused shared headers, compute-to-fullscreen execution with no Scene/Material/Batch, missing active Batch failures, custom shadow Global input, and independent queued view values.
- Material source/binary payload v2 retains all techniques and explicit property mappings. Malformed technique metadata and legacy single-shader JSON are rejected. ShaderUniform tests cover target offsets, combined world/material assembly, missing/duplicate fields, type mismatch and overlapping ranges. PassResourceBindings tests cover atomic failure, disjoint/competing owners and dynamic policy.
- Real project packaging cooks 33 requests from Aurora plus two selected plugin owners for SPIR-V, DXIL and MSL, including explicit variants. One index is staged per target. Temporary project sources are removed before a Core-only consumer resolves every request; this target has no Slang/compiler link.
- The relocated products also render a real scene in all three Windows backend modes with an empty source filesystem and no registered compiler. This native GPU test executable still links the compiler library for its other tests: it does not prove native scene startup with the Slang DLL physically absent. A stripped native shipping-host run remains an explicit acceptance follow-up.
- SandboxEditor opened the existing B14Review project, initialized the swapchain, ran eight bounded frames and exited 0 under desktop permissions. No renderer/validation error was logged. This startup probe is not a full manual material-editor acceptance test.
- Persisted export checks, changed-C++ ASCII/clang-format checks, git diff whitespace checks and strict OpenSpec validation pass. New technique compile, uniform packing and material mapping functions were split to keep validation steps focused; scoped clang-tidy reports no diagnostics in the changed lines of ShaderTechnique, ShaderUniform, FrameComposer, SceneAssetSource, MaterialReflection and SceneRenderFeature. This is not a globally warning-free repository claim.

## Cleanup validation (2026-10-22)

- Reconfigured the existing Debug build after source deletion. CoreTest, PipelineTest,
  ShaderTest, AdaptorTest, ShaderHeaderTool, SandboxModule and SandboxEditor build successfully.
- CoreTest: 80/80; PipelineTest: 42/42; ShaderTest: 49/49; AdaptorTest: 33/33.
  ShaderTest has four fewer cases because the removed ShaderBlockGen's tests are retired;
  the remaining CPU layout test and Slang layout/reflection tests pass.
- Export/inventory/project/schema Python suite: 25/25; `ShaderHeaders.Check` passes.
  Editor asset deployment removes the old material/vertex copies while preserving the
  current shader inventory. SandboxEditor opens B14Review with `--frames 8` and exits 0.
- Strict validation passes for this change and the three reconciled lighting/shared-header/
  platform-parity changes. Changed C++ formatting and `git diff --check` pass.
- This cleanup does not rerun the full renderer GPU suite or close native acceptance
  tasks 7.1/7.2; the earlier GPU evidence above retains its original scope.

## Limits and migration notes

- MSL generation/reflection passed on Windows; native Metal execution and macOS/iOS packaging remain unverified. The existing Apple/POSIX package tests rely on platform path/mode/symlink behavior and were not treated as Windows acceptance.
- The adapter currently implements material uniform properties, not automatic material texture/sampler asset transport, arrays, deformation or a full PBR/PSO system. Existing Lit lighting/IBL is the lit-material acceptance case. Unsupported active resources fail instead of receiving dummy bindings.
- The current frame IR supports material depth and blend state. Unsupported raster/stencil/alpha-to-coverage requests are rejected rather than silently discarded.
- Old material binaries and the single `shader` source form need explicit stage/mapping migration. Shader cache index v4 records active-entry reflection and requires recook. Following the user's cleanup request, the unconsumed repository HLSL/material/vertex examples have been removed; external legacy content still requires migration.
- A pre-existing output `assets` symlink must be removed before shader staging; shader staging refuses an aliased root/container so it cannot write into engine source assets. The editor now deploys into a real assets container.
- The change remains active for native acceptance. Archive requires separate user confirmation under AGENTS.md.
