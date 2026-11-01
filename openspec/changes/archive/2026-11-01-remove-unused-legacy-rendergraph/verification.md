# Verification — 2026-10-26

The repository audit found no production consumer of the old graph. Removal covers 38 files (4,066 lines in those files), factory declarations/definitions in Device and all three backends, and 19 dedicated old-API test cases. Independent cases in ClientViewportTest and ResourceTiersTest remain.

## Fresh Windows results

| Check | Result |
|---|---|
| Existing VS 2022 Debug configure | Passed; GLOB inventories regenerated |
| Full incremental Debug build | Passed, including backend DLLs, renderer tests and Sandbox host; existing third-party PDB warnings remain |
| Aurora.RHITest | 143/143 passed |
| Aurora.PipelineTest | 48/48 passed |
| Aurora.CoreTest | 107/107 passed |
| Aurora.Renderer.GpuSubmissionTest, full run | 960 cases: 947 passed, 10 expected skips, 3 failures in the same stale prepared-material fixture; correction and rerun below |
| Prepared-scene fixture and MaterialVariantSelection rerun | 4/4 passed after the fixture correction and targeted rebuild |
| Source and generated-project references | No deleted graph/helper symbol or source entry remains |
| Scoped clang-format / whitespace | 12 changed C++ files formatted; `git diff --check` passed |
| Scoped policy scan | No exception/RTTI, shared-static, diagnostic residue, relative-include, non-ASCII C++ or duplicate-block finding |
| Original Git index | Byte-identical to the saved cleanup baseline |
| New change strict validation | Passed |
| All strict validation | 207 passed / 212 items; five existing no-delta plans remain invalid |
| Read-only spec merge preview | 66/66 deltas passed against current main specs |

The five no-delta plans are animation-events, audio-animation-bridge, audio-world-integration, aurora-cook-schema-layering and aurora-navigation-integration. Their status is unchanged by removing the old graph.

## Full-suite fixture correction

`IndexedRaster.PreparedSceneViewsResolveExactFallbackAndDrawOwnedPackets` failed on Vulkan Dynamic Rendering, Vulkan RenderPass and DX12 with the same exact-material-variant rejection. The pre-cleanup test fixture manually constructed PreparedMaterialLayout without its mesh vertexMask; production ResolvePreparedRaster now requires the actual vertex variant. The old graph is not part of this call chain.

The fixture now derives vertexMask from its actual mesh streams/attributes and checks that an omitted mask is rejected before performing the original fallback, LOD, owned-packet and pixel assertions. Production matching is unchanged. The targeted rebuild passed; all three backend cases plus MaterialVariantSelection passed (4/4). The full-run XML is preserved and is not reported as a single green full-suite run.

The ten skips are Vulkan-specific surface negotiation/presentation cases instantiated with the DX12 parameter, each with an explicit backend reason.

## Evidence and limits

Local logs/XML are under `cmake-build-debug/cleanup-review-2026-10-25/`: `legacy-rdg-configure.log`, `legacy-rdg-build.log`, `legacy-Aurora.*.log/.xml`, `legacy-rdg-fixture-build.log`, `legacy-prepared-fixture.log/.xml`, `legacy-rdg-spec-validate.log`, `legacy-rdg-policy.log` and `spec-merge-preview.json`. The `legacy-rdg/` subdirectory records before-images, the exact code/doc inventories and retired test names.

This is an incremental Windows build, not a clean or all-platform build. Metal's deleted factory/adapter and include closure were reviewed at source level; native macOS build/tests remain open. No commit, staging change or OpenSpec archive was performed.

## Standalone commit snapshot

Commit preparation also reconstructed HEAD `69e33334` plus only the legacy-graph removal, without the pending shader/animation/reflection changes. Its separate VS 2022 Debug build of RHI, Pipeline and Core tests passed. RHI passed 143/143, Pipeline 40/40, and Core passed 90 with one expected skip: the relocated shader-package fixture was not prepared in this isolated checkout. These counts differ from the integrated working-tree results above because the other pending features are absent.

The vertexMask fixture correction is excluded from this cleanup batch and remains with the V/P/S changes. Retirement specifications retain the predecessor's binding ABI; the later shader/material batch owns the four-group migration. The Metal source deletion is included, but a native Metal build remains unverified.

Evidence: `cmake-build-debug/cleanup-review-2026-10-25/split-v2/01-configure.log`, `01-build.log`, `01-Aurora.*.log/.xml`, and the prepared patch/file manifest. The isolated source and cache live under `cmake-build-debug`; the primary cache and original Git index are preserved.
