> 2026-11-01 整理：**ready-to-archive**。现行 owner：旧图退役记录；现行语义由后续 FrameGraph 契约维护。
> 剩余：旧 macOS 门已有后继原生证据补齐；25 条 delta 与现主 spec 全文一致，申请 history-only 归档。下方较早状态与验收记录保留为历史。

## Implementation

- [x] 1. Trace old graph construction, backend factories, templates and helpers across all repository consumers.
- [x] 2. Remove the closed old graph dependency chain in all three backends without changing target ownership.
- [x] 3. Remove exclusive tests and preserve independent cases in mixed test files and shared shader declarations.
- [x] 4. Reconcile main specs, overlapping changes, Aurora rules and current docs; record historical retirement separately.

## Validation

- [x] 5. Reconfigure and build the existing Windows Debug tree, including affected hosts and backend DLLs.
- [x] 6. Run RHI, Pipeline, Core and production GPU frame graph regression suites; correct the stale prepared-material fixture and pass its three backend cases plus variant-selection regression (4/4).
- [x] 7. Verify no dangling references, scoped formatting, Git index preservation, strict OpenSpec and delta merge previews.

## Platform follow-up

- [x] 8. Build and run native Metal regression in the macOS validation environment; reconciled with the recorded 2026-10-27 successor native results below.

## 2026-11-01 后继 macOS 证据复核

本次只核对既有文件，未重新执行构建/测试。2026-10-27 的 `cmake-build-debug/framegraph-rhi-final.xml` 记录 186/186、零失败与零跳过，其中 `AuroraMetalTest` 32 项实际执行，包含动态偏移、Compute、Custom 失败 discard/slot 复用、MSAA/resolve、双在途 slot 与深度读回；同轮 `framegraph-core-close.xml` 为 111 passed / 1 既有 fixture skip，`framegraph-pipeline-final.xml` 为 43/43。其构建与原生范围见 `refactor-aurora-framegraph-contracts/verification.md`。

这些后继证据覆盖本 change 原先仅因 Windows 主机而保留的 macOS 编译/native 回归门；不用于关闭后续 Windows 新契约回归、移动端生命周期或完整平台验收矩阵。历史失败及原平台限制不删除。
