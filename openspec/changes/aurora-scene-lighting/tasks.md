Status: active. L01-L04 Windows implementation, native editor checks and final regression passed. Metal/macOS and mobile native execution remain unverified; static-analysis cleanup is recorded separately in aurora-scene-bridge 5.3.

## 1. L01 Depth attachments and sampling

- [x] 1.1 Add depth-only raster declaration and validation/content rules.
- [x] 1.2 Implement Vulkan dynamic/render-pass, DX12 and Metal encoder/compiler handling.
- [x] 1.3 Validate scalar D32 sampled bindings and graph rejection cases.
- [x] 1.4 Verify depth write/read pixels in all three Windows modes; record Metal native gate separately.

L01 evidence (2026-10-21): graph rejection and native depth-only mip 1/layer 1 write, LOAD and scalar sampling passed (4 cases). Metal has a dedicated depth-array slice/scissor test but requires macOS execution. Corrected descriptor-allocation counts passed in all three Windows modes; the final broad regression is recorded below.

## 2. L02 Lights and directional shadows

- [x] 2.1 Generate bounded light parameters and verify array/layout contracts.
- [x] 2.2 Extract owned shadow caster geometry independently of camera visibility.
- [x] 2.3 Bind scene lights and directional shadow map through shared Pass slots.
- [x] 2.4 Verify light changes, shadow toggles, off-camera casters and resource retirement with pixels.

L02 evidence: off-camera caster, light intensity, cast/receive toggles and deletion pixels passed in Vulkan dynamic rendering, Vulkan render passes and DX12 (3 cases). ResourceTest 36/36, CoreTest 69/69 and ShaderTest 46/46 passed. MSL array reflection exposed a mixed-category scan omission; the corrected logical descriptor selection passes CPU reflection/layout tests, without claiming native Metal execution.

## 3. L03 HDR output

- [x] 3.1 Validate RGBA16_FLOAT render/sample capability and native format handling.
- [x] 3.2 Add pass-owned exposure/tone mapping to the scene output path.
- [x] 3.3 Verify HDR values, final display conversion and resize/in-flight isolation.

L03 evidence: linear values 4/1/0.25 produce expected display pixels after Reinhard mapping and sRGB encoding; queued exposure changes and resize retain independent pixels in all three Windows modes (3 cases). PipelineTest 33/33 passed. These readbacks cover UNORM display targets; native Metal and hardware sRGB target execution are not inferred.

## 4. L04 Environment and editor integration

- [x] 4.1 Add explicit environment resource inputs, required upload/view support and neutral defaults.
- [x] 4.2 Bind diffuse/specular IBL and BRDF inputs through shared Pass declarations.
- [x] 4.3 Expose persisted environment settings through existing reflected editor controls.
- [x] 4.4 Verify replacement, missing/pending inputs, save/reopen and native output pixels.

L04 evidence: environment default/replacement/intensity/specular-mip/BRDF pixels and RGBA16_FLOAT chunked upload/readback passed in all three Windows modes (6 cases). AdaptorTest 32/32, SandboxProjectTest 10/10 (including reflected exposure undo and environment asset UUID save/reopen), PipelineTest 33/33 passed. Publication validates the actual declared image format; a previous RGBA8-only resident check was corrected after the new tests exposed it.

## 5. Completion gates

- [x] 5.1 Run affected builds, regression tests, formatting checks and strict OpenSpec validation.
- [ ] 5.2 Execute Metal/macOS and mobile native coverage where available; otherwise retain an explicit unverified gate.

Final Windows evidence (2026-10-21): affected Debug targets and the normal SandboxModule build with configured runtime dependencies passed. GPU submission suite: 895 cases, 892 passed / 3 DX12-parameter skips for Vulkan-only surface cases, zero failures (217.448 s). The previously intermittent QueryErrorAfterAcquirePreservesCauseAndReleasesBorrowedOutput/2 passed this run. RHITest 133/133, EditorShellTest 129/129, AdaptorTest 32/32, SandboxProjectTest 10/10, PipelineTest 33/33, ShaderTest 46/46, ResourceTest 36/36 and CoreTest 69/69 passed. Changed-range clang-format and git diff --check passed, as did strict validation of the four active pass/bridge/editor/lighting changes. Scoped clang-tidy still has recorded warnings and is not claimed clean. Logs: cmake-build-debug/scene-final-gpu-tests.log/.xml, scene-rhi-final.log, scene-menu-shell-tests.log, scene-orientation-after.log and scene-orientation-fix-build.log. Native editor verification covered Environment exposure edit/undo, shadow toggle, save/reopen, corrected scene orientation and directional light defaults. No archive or commit was performed.
