> 2026-11-01 整理：**active**。现行 owner：本 change 保持最终公共参数/DirectFrame/绑定组契约；平台矩阵由 `aurora-renderer-platform-parity` 接续。
> 2026-11-11 续作：本 change 同时承接已确认的后端编译归属、显式 output Export、分析预算及局部精简；2026-11-12 完成本机验证。Windows 原生回归 4.5 与 P02-LIGHTING 5.4 仍未验收，历史验证不代替本轮证据。

## 1. Native correctness and resource-use facts

- [x] 1.1 Produce compile-local typed resource uses and unify pre-cull declaration validation.
- [x] 1.2 Consume raster uses for Vulkan/DX12 storage synchronization and combined physical-buffer read roles.
- [x] 1.3 Lower Metal attachment preservation and resolve in one render pass.
- [x] 1.4 Validate and consume Metal plans safely and propagate recording failures without successful receipts.
- [x] 1.5 Add focused content, storage synchronization, resolve and plan ownership regressions.

## 2. Immutable binding groups

- [x] 2.1 Introduce immutable logical group references with independent per-use dynamic offsets and migrate graph consumers.
- [x] 2.2 Reuse compatible group target encoding and uniform uploads within graph/lease composition.
- [x] 2.3 Materialize/reuse native descriptors per compatible group and retain exact resource/range/view keys.
- [x] 2.4 Verify sparse sets, differing layouts/ranges, multiple draws and in-flight lifetime/reuse behavior on the available host; record Windows native follow-up separately.

## 3. Public protocol boundaries

- [x] 3.1 Separate named parameter updates from numeric descriptor writes and migrate consumers.
- [x] 3.2 Remove unused DescriptorBatch public protocol and preserve relevant native writer tests.
- [x] 3.3 Isolate direct-frame compatibility in an explicit interface/factory with checked begin/reset.
- [x] 3.4 Complete feature graph declaration operations and separate resource/draw services.
- [x] 3.5 Migrate API contract fixtures and add a GPU-only transient-buffer feature regression.

## 4. Verification and documentation

- [x] 4.1 Configure the existing macOS build and build affected interfaces, libraries, backends and tests.
- [x] 4.2 Run current applicable CPU/native regressions and record unavailable Windows/DX12 checks explicitly.
- [x] 4.3 Check scoped formatting, ASCII/includes, forbidden patterns, whitespace and strict OpenSpec validation; record unavailable clang-tidy.
- [x] 4.4 Review the final diff and update current API/design rules and verification evidence without commit/archive.
- [ ] 4.5 Run the Windows Vulkan/DX12 native storage/group-reuse and GPU lifecycle regressions listed in verification.md on the Windows validation host.

## 5. Physical Vulkan device compatibility

- [x] 5.1 Preserve explicit compiler modes, default to AUTO and reject invalid configuration; verify public/default/non-Vulkan behavior and actual capability selection.
- [x] 5.2 Validate version-correct Vulkan 1.1/KHR feature and command negotiation, mandatory synchronization and unsupported direct-rendering errors.
- [x] 5.3 Recook Android SPIR-V 1.3 products, pass the 40-check physics APK audit, verify falling/settled bodies over the 121-second device run and record the separate 15-second presentation-progress sample with controlled cleanup.
- [ ] 5.4 Transferred, not accepted: `aurora-renderer-platform-parity` P02-LIGHTING owns the observed shadow striping/vendor-warning visual review; the physics/package run does not establish visual-quality or validation-clean acceptance.

## 6. Approved architecture follow-up — 2026-11-11

- [x] 6.1 Combine DX12 sampled-image read states by physical backing/subresource and preserve compute creation native causes with exact-device checks.
- [x] 6.2 Require explicit successful output Export, deduplicate image exports and migrate Scene/UI/TextureToScreen producers and fixtures; preserve Accepted-boundary persistent output state.
- [x] 6.3 Resolve image readiness for the selected queue technique/variant, preserving strict variant, owner, type, epoch and range errors.
- [x] 6.4 Check raw declarations and predicted access work before expansion/culling, normalize separate content collections and add exclusive enum bounds.

## 7. Backend ownership and local simplification

- [x] 7.1 Extract concrete Metal compiler/encoder into private files within the existing target; retain per-pass depth refresh and submitted GPU payload ownership.
- [x] 7.2 Add bounded device/epoch compute PSO caches to all backends and graphics PSO caching to Metal; retain exact shader owners and independent plan pins.
- [x] 7.3 Resolve binding drafts once for shared analysis, add failure-only pass/resource/range context and preserve Custom order.
- [x] 7.4 Build frame-local resource/material lookup indices after resource preparation, use borrowed spans and narrow PassResourceBindings to FrameGraphServices.
- [x] 7.5 Remove GraphBuildArena with equivalent shared/weak ownership and Reset behavior; simplify cached external override replacement.

## 8. Follow-up verification

- [x] 8.1 Add regressions for corrected boundaries, cache reuse/retention, output paths, selected-technique readiness and budget rejection.
- [x] 8.2 Reconfigure the existing Xcode build, run source-ownership checks and build affected production targets and tests.
- [x] 8.3 Run available host CPU/native suites, scoped formatting/rules and strict OpenSpec checks; record actual evidence and unavailable Windows checks.
- [x] 8.4 Review the complete diff and update final design/verification without commit or archive.
