> 2026-11-01 整理：**ready-to-archive**。现行 owner：本 change 保留内部阶段抽取记录；语义后继为 `refactor-aurora-framegraph-contracts`。
> 剩余：旧 macOS 编译/native 门已由后继精确 XML 补证，当前无独立实现余项。下方较早状态与验收记录保留为历史。

## Implementation

- [x] 1. Separate graph resource validation, typed pass access collection, culling and content/lifetime checks.
- [x] 2. Centralize repeated binding lookup and image-view range normalization.
- [x] 3. Extract backend-private compilation stages and preserve native allocation/synchronization ownership.
- [x] 4. Inventory legacy RDG consumers for a separate migration review.

## Validation

- [x] 5. Verify analysis-stage behavior and binding-order regressions.
- [x] 6. Build affected Windows targets and run GPU submission and RHI suites.
- [x] 7. Check scoped formatting/static analysis, diffs and OpenSpec; record platform limitations.

## Platform follow-up

- [x] Run macOS compilation and native Metal regressions in the platform validation environment; reconciled with the recorded 2026-10-27 successor native results below.

## 2026-11-01 后继 macOS 证据复核

本次只核对既有文件，未重新执行构建/测试。2026-10-27 的 `cmake-build-debug/framegraph-rhi-final.xml` 记录 186/186、零失败与零跳过，其中 `AuroraMetalTest` 32 项实际执行，包含动态偏移、Compute、Custom 失败 discard/slot 复用、MSAA/resolve、双在途 slot 与深度读回；同轮 `framegraph-core-close.xml` 为 111 passed / 1 既有 fixture skip，`framegraph-pipeline-final.xml` 为 43/43。其构建与原生范围见 `refactor-aurora-framegraph-contracts/verification.md`。

这些后继证据覆盖本 change 原先仅因 Windows 主机而保留的 macOS 编译/native 回归门；不用于关闭后续 Windows 新契约回归、移动端生命周期或完整平台验收矩阵。历史失败及原平台限制不删除。
