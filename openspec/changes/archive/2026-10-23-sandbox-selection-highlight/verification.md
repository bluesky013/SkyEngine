# Selection highlight verification

Date: 2026-10-23. Windows x64, VS 2022, NVIDIA GeForce RTX 3080.

## Implemented behavior

- Supported static opaque meshes receive an orange screen-space outline. Visible edges use full opacity; occluded edges use the subdued opacity confirmed by the user.
- Selection is shared with the viewport and outliner, independent of the active transform tool. Clear, deletion, document changes and play mode remove obsolete feedback.
- Unsupported/custom geometry uses available bounds; objects without bounds use a pivot marker. Selected overlay proxies receive the theme accent.
- Editor owns selection policy. Aurora consumes owned generation-bound render identities and style values in existing targets; runtime/core has no Editor dependency.
- Actor-to-render mapping checks both component and part identity. A mesh and light on the same Actor cannot make the light eligible for the mesh mask.

## Build and automated coverage

Configured `cmake-build-debug` with its existing third-party path. Built affected pipeline, adaptor, editor, native renderer tests, SandboxModule and SandboxEditor in Develop and Debug. Both builds succeeded. The final component-identity correction was rebuilt in both configurations and the full adaptor suite rerun.

| Suite | Develop | Debug |
| --- | ---: | ---: |
| Aurora.PipelineTest, related filter | 30 passed | 30 passed |
| Aurora.AdaptorTest | 37 passed | 37 passed |
| EditorCoreTest | 177 passed | 177 passed |
| EditorShellTest | 141 passed | 141 passed |
| EditorRenderTest | 46 passed | 46 passed |
| EditorHostNativeTest | 9 passed | 9 passed |
| Total | 440 passed | 440 passed |

Pipeline filter: `SceneViewSettingsTest.*:*SharedPassHeaders*:*SharedPassHeaderCache*:PassResourceBindings.*:ShaderVariantAssemblerTest.*:GlobalVariantLayoutTest.*`.

Native tests run Vulkan dynamic rendering, Vulkan render passes and DX12. Pixel readbacks verify a selected mesh partly hidden by an unselected opaque mesh. Each backend/configuration detected 278 strong visible-outline pixels and 242 subdued occluded-outline pixels. Interior colors were unchanged. Clearing selection and submitting an obsolete entity generation both restored bytes exactly to the baseline. Existing editor-host retirement and production picking tests also passed.

Debug enabled `VK_LAYER_KHRONOS_validation` and the D3D12 debug layer. Logs contained no VUID, resource-state or depth-sampling errors. Vulkan reported external EOS duplicate-overlay and OBS older-API layer warnings.

Unit coverage includes empty/multiple/invalid identities, duplicate IDs, bounded counts, finite styles, surface eligibility, mixed mesh/light Actors, editing lifecycle, fallback geometry, custom provider unregister/reset, and unchanged dirty/undo state. Shared shader tests compile SPIR-V, DXIL and MSL; MSL compilation is not native Metal execution.

Logs and gtest XML are under `output/selection-*`. Shader header export checks, OpenSpec strict validation and `git diff --check` passed. Modified/new C++ files were formatted and checked for ASCII content.

## Cost and limits

- No eligible selection: no highlight mask, highlight depth or composition pass. Existing scene rendering still runs normally.
- Selected geometry: one mask pass draws only selected parts, followed by one fullscreen composition pass. The mask and its depth use RGBA8 and D32; together they require about eight bytes per viewport pixel before allocator alignment. Existing scene depth becomes sampleable and is reused.
- The effect currently renders on each submitted selected frame; it does not cache a persistent mask. The default two-pixel circular outline performs up to 13 mask loads per outside pixel; the accepted radius is bounded to 1-4 pixels. No CPU mesh decoding or production GPU readback is introduced.
- No steady-state GPU timing benchmark was recorded. First use may compile the two highlight shaders through the existing shader cache. This validation does not claim a frame-time improvement.
- Static opaque, position-preserving geometry is the supported silhouette contract. Alpha-tested, transparent, skinned/deformed and arbitrary custom geometry use fallback feedback until their rendering contracts are supported.
- The native pixel case covers one selected mesh and one occluder in LDR. HDR ordering and multi-selection union were reviewed in code and input tests; they were not separately measured in native pixel tests. Near-plane clipping of fallback segments reuses the existing proxy projection path.
- Native Metal/mobile, visual DPI review and interactive editor selection were not exercised in this run. The editor was built; the verified visual evidence comes from native GPU pixel readbacks.
- The unrelated full PipelineTest run hits an existing assertion in `AuroraVulkanTest.BatchDynamicUboRangeZeroRejected` (`VulkanDescriptorEncoder.cpp`, explicit dynamic buffer range). Only the related 30-test filter is reported as passed.
- Third-party preflight reports seven stale/missing metadata entries and zero missing output entries. Existing cached dependencies were used without changes.
- The overlapping Overlay draw-order/pick-priority issue was subsequently fixed during finalization in `sandbox-viewport-picking`; see its final review record.

## References

- [UE viewport selection outline and highlight settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/ULevelEditorViewportSettings)
- [Unity Selection Outline and Selection Wire](https://docs.unity.cn/6000.1/Documentation/Manual/GizmosMenu.html)
- [Godot editor selection box settings](https://docs.godotengine.org/en/latest/classes/class_editorsettings.html)

These inform the interaction and visual design; the mask implementation is local to SkyEngine and does not claim to reproduce those engines' internal algorithms.
