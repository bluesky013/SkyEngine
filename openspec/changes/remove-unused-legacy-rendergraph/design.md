## Caller audit

The repository-wide source/include/factory search on 2026-10-26 found:

| Old API consumer | Result |
|---|---|
| RenderGraph.cpp / Compile.cpp / Execute.cpp | Closed implementation of the old setup/execute graph |
| VulkanRDGBackend / D3D12RDGBackend / MetalRDGBackend | Created only by the old graph through Device::CreateRDGBackend |
| PipelinePass and scene/fullscreen subclasses | Used only by PipelineTemplateTest; no host registration or production construction |
| GlobalRenderResources / BatchAllocator / BatchPackWriter | Only legacy pipeline resource tests; independent of FrameContext allocation |
| RDGTest / PipelineTemplateTest | Entirely old API coverage; removed |
| ClientViewportTest / ResourceTiersTest | Mixed coverage; remove only old graph/helper cases and retain direct RHI, viewport and reflection cases |
| Runtime/editor rendering | FrameComposer/FrameGraphBuilder → FrozenFrameGraph → DeviceFrameContext; no old graph consumer |

The removal contains 38 files and factory/header edits in the existing targets. GLOB source inventories are refreshed by reconfiguring the existing build. The source scan covers engine, plugins, tools, Python, configuration and CMake; it does not assert compatibility for external repositories.

## Retained boundaries

- `pipeline/shaders/{OpaquePass,ScenePass,TextureToScreenPass,...}.h` and persistent `.slangh` exports are shared resource declarations, distinct from the deleted public C++ pass classes with similar names.
- `SceneRenderFeature`, `TextureToScreenFeature`, `GlobalVariantLayout`, shader variant assembly, reflection validation and UniformUploadPlan remain production code.
- `rhi/common/FrameTransientPool` owns modern backing leases and remains in use. The deleted `rhi/interface/src/rdg/TransientPool` was the old immediate pool.
- DeviceFrameContext's checked lease/compile/dispatch/submit protocol and its existing direct-host interface remain. RenderViewport/ClientViewport and ordinary RHI encoders remain.
- RenderScene/LightSystem/SceneView and their independent CPU tests are outside this old graph deletion. No unrelated reflection ownership or module registration is changed.

## Specification reconciliation

Main specs are updated with the implementation in this working tree; this is not an archive operation. The accompanying deltas are rebased on that updated main state, following the existing active-change reconciliation. Removed or renamed legacy requirements are recorded below instead of emitting REMOVED operations against already-absent main requirements.

| Capability | Retired requirements / replacement |
|---|---|
| aurora-rdg | Old setup callback builders, six AddXxxPass APIs, RDGContext lookup and CopyBlit builder overloads removed. Lifecycle, typed payload, raster draws, output imports, handles/equality now describe FrozenFrameGraph and graph identity. Content, barriers, aliasing and native plan guarantees remain. |
| aurora-pipeline | ScenePass(HDR), TextureToScreenPass, OpaquePass class requirements, old Collect and PipelinePass lifecycle removed. Registered scene/fullscreen frame features remain; no unused template migration is promised. |
| aurora-resource-binding | Standalone BatchAllocator lifecycle replaced by WriteFrameBuffer/DeviceFrameLease allocation and retirement. Input rules retain the existing binding ABI; the separate shader/material migration owns resource-group numbering. |
| ui-render | Removed requirement to inherit PipelinePass and use DrawItem/BatchPackWriter. Describes current direct Aurora encoder, CPU projection and Batch textures, with graph ownership in the host. Multi-frame buffer retirement remains a separate open requirement. |
| aurora-shader | Pipeline variant ownership no longer names the deleted PipelinePass. |

The overlapping compute/custom and target-artifact deltas must not restore old declarations. The renderer-extensions plan drops old-template targets without claiming its other future work is complete. Dated audit observations retain an explicit retirement note; archived changes are untouched.

## Validation

Reconfigure and build the existing Windows Debug tree, then run RHI, Pipeline, Core and production GPU graph suites. Preserve their XML results and verify the remaining source tree contains no deleted public symbols/includes/factories. Check strict OpenSpec validation and actual read-only delta merge previews, not only Markdown syntax. Metal factory deletion is reviewed at source level on this Windows host; native macOS validation remains a platform limitation.

Fresh results are recorded in `verification.md`; no previous test result is counted as this cleanup's validation.

The integrated working-tree regression also exposed a missing vertexMask in a PreparedMaterialLayout fixture. That correction belongs with the V/P/S migration and is excluded from this standalone cleanup commit; it is not a legacy-graph caller.
