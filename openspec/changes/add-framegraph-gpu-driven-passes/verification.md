# Verification - 2026-11-07

## Implemented corrections

- FrameComputePass stores exactly one CmdDispatch or ComputeIndirectDispatch variant. RasterDraw.indexed is the only indexed-shape selector.
- ResolveIndirectArgumentLayout centralizes native argument size, effective stride, four-byte alignment and required span. Invalid, uninitialized and mixed raster declarations fail before encoding.
- Vulkan logical-device creation enables the indirect features it reports; draw counts also respect maxDrawIndirectCount. Metal indirect dispatch no longer fails an unrelated explicit zero-count check.
- DX12 command signatures use the existing bounded cache. Compiled plans retain ComPtr owners; direct command buffers retain signatures until safe reset. Encoder failures flow through EndChecked/TrySubmit rather than producing success.
- Redundant native indirect command fields were removed. External imported buffer lifetime remains graph-owned.

## Validation

Actual Aurora.RHI.Static, Aurora.Core.Static, Aurora.Metal and Aurora.Vulkan builds passed with the existing Xcode Debug cache. Aurora.RHITest passed 210 tests, including argument layout/alignment/range validation, logical Vulkan feature negotiation and native Metal direct/indirect dispatch. Aurora.CoreTest passed 127 tests with one distribution-fixture skip; graph composition covers external buffer identity and ownership. Pipeline and renderer module tests also passed after using their declared working directories. The real Metal shipping cook and relocated compiler-free consumers passed 4/4, including compute/fullscreen rendering; the material fixture was corrected to declare its four required SHADOWS/IBL variants.

New Windows-target test cases cover Vulkan enabled-feature/count limits and direct/indirect dispatch readback; DX12 cases cover signature eviction lifetime, rejected invalid direct indirect commands and dispatch readback. These cases reside in Aurora.Renderer.GpuSubmissionTest, which is gated by WIN32; they were not built or executed on this host. DX12 therefore still needs Windows validation.

## Remaining

GPU compute-written argument production followed by indirect consumption and the full indexed/nonindexed native encoding matrix remain open (tasks 4.2/4.3). This does not implement GPU particle simulation. Strict OpenSpec validation and git diff --check passed. No commit or archive was performed.
