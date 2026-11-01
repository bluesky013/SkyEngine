> 2026-11-01 整理：**ready-to-archive**。现行 owner：本 change 保留内部阶段抽取记录；语义后继为 `refactor-aurora-framegraph-contracts`。
> 剩余：旧 macOS 编译/native 门已由后继精确 XML 补证，当前无独立实现余项。下方较早状态与验收记录保留为历史。

## Why

FrameGraph analysis and native compilation have accumulated repeated binding lookup and long routines mixing resource checks, pass preparation and planning. This makes the existing contracts harder to review as pass support grows.

## What Changes

- Separate resource validation, per-pass access collection, backward culling and forward content/lifetime analysis within the existing implementation.
- Reuse binding lookup and view normalization while preserving declaration order and native dynamic-offset handling.
- Organize backend-private compilation helpers without sharing native barriers or introducing a second compiler framework.
- Record legacy RDG consumers for a separate migration review; retain its API and implementation in this change.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-framegraph`: behavior-preserving analysis and native preparation boundaries.

## Impact

Existing Aurora RHI interface implementation, Vulkan/DX12/Metal compilers and renderer tests. No new target, module ownership, directory relocation, pass ABI or Global/Pass/Material/Batch layout change.
