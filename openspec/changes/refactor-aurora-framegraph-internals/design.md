## Context

The working tree already contains compute/custom, scene rendering and editor changes. This refactor applies to that current implementation and keeps those behaviors intact. FrameGraphAnalysis.cpp has a roughly 540-line entry point; DX12's Compile combines capability checks, descriptor allocation, state tracking and per-pass compilation in roughly 640 lines.

## Decisions

1. Keep graph data immutable. Analysis owns only temporary logical ranges, pass accesses and lifetime results. Extract named stages and typed pass handlers in the current file; preserve validation order and error codes.
2. Keep reads, writes and defines distinct. Validate declarations before culling, then validate content only for surviving passes and exports. Preserve logical pass ordering and inclusive alias lifetimes.
3. Reuse pure binding lookup and range normalization, including existing raster/compute policies. Preserve Custom resource array order, sparse descriptor sets, and base versus dynamic offsets. Do not add permanent normalized copies to FrozenFrameGraph.
4. Backend helpers own their native state and allocation rules. Keep Vulkan rendering modes separate, DX12 metadata initialization and scratch barriers intact, and Metal pipeline-specific limits intact. Compilation failure still rolls back within the same lease with device-loss priority.
5. Keep the current public IR and generated Global/Pass/Batch source contract. Header partitioning remains a follow-up; legacy RDG removal is tracked by `remove-unused-legacy-rendergraph`; no broad renames or module changes are needed here.

## Legacy migration inventory

The original inventory found only the old templates and dedicated tests consuming RenderGraph. The 2026-10-26 caller review confirmed no production callers and removed that closed dependency chain under `remove-unused-legacy-rendergraph`. Production remains FrameComposer/FrameGraphBuilder and FrozenFrameGraph. The mixed resource/viewport tests retain their independent RHI coverage.

## Implementation boundaries

- `FrameGraphAnalyzer` keeps resource validation, typed access collection, exports, backward culling and forward content/lifetime validation in named stages. Its temporary state exists only during analysis.
- `FrameBindingLookup` borrows declarations into the existing four-set, 32-binding limit. It preserves the first match and leaves duplicate/limit diagnostics to the existing validators. It neither sorts declarations nor owns shader resources.
- `D3D12CompileState` owns compilation-local native state. Capability checks, allocation, typed pass preparation and final transitions remain sequential. Raster attachment setup, draw preparation and descriptor writes have separate helpers. Descriptor reuse remains scoped to one raster pass; compiled draws retain their pipelines and the compiled frame retains native resources.
- `MetalPassCompiler` handles each pass alternative after resource-owner validation. Reservation, dispatch, command-buffer cleanup and GPU retirement remain in `MetalDeviceFrameContext`.
- Vulkan retains its existing compiler/resource split. Compute descriptor writes and barriers now reuse one normalized image-view range.

This change reduces the responsibilities inside individual routines. It does not claim a net source-line reduction or measured runtime speedup; backend policy and error checks remain explicit.

## Validation

- Run existing analysis cases for partial contents, overwritten producers, dead declarations, exports, attachment discard, dynamic offsets and custom kinds.
- Add focused checks where stage boundaries or shared normalization could change behavior.
- Build affected Windows targets and run GPU submission/RHI regression suites, including Vulkan rendering modes, DX12, aliasing, rollback and retirement.
- Verify formatting, scoped static analysis and strict OpenSpec validation. Metal source parity is reviewed here; macOS compilation/native tests require the platform validation environment.

## Verification results (2026-10-21, Windows Debug)

- Reconfigured the existing Visual Studio build and built `Aurora.RHI.Static`, `Aurora.DX12`, `Aurora.Vulkan`, `Aurora.Renderer.GpuSubmissionTest` and `Aurora.RHITest` with their dependencies. Final incremental checks also passed.
- GPU submission: 898 cases, 895 passed, 3 expected skips (Vulkan-only surface cases on the DX12 parameter). RHI: 133/133 passed. The previously reported intermittent frame-output error did not occur in this run.
- Three additional analysis regressions cover declaration/resource error precedence versus culling, sparse bindings and dynamic offset order, and shared image-view normalization with distinct descriptor/custom policies.
- Compared extraction against the pre-refactor working-tree snapshot: Metal copy/raster/custom bodies and Dispatch, plus DX12 copy/custom bodies and Encode, remain token-equivalent after helper/parameter renaming. Compute lookup and raster helper changes were reviewed with the native regression results.
- Scoped clang-format and whitespace checks passed. Clang-tidy completed for the four Windows translation units with no compiler errors. It still reports style/complexity diagnostics in moved code and gtest macros, the existing allocation-versus-noexcept diagnostic, and an attachment optional-access diagnostic whose precondition is established by graph analysis. These are not represented as a warning-free tidy run.
- Strict OpenSpec validation passed. Metal was reviewed at source level only; macOS compilation and native execution remain an external platform-validation requirement.
