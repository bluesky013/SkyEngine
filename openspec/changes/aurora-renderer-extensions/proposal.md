## Why

Status: debt-record（未排期）。保留可注册 CPU prepare、更多光类型和生产 FrameGraph feature 的后续扩展。旧 pipeline 模板已由 `remove-unused-legacy-rendergraph` 确认无生产消费者并删除，不再作为迁移目标；其它待办不能因此视为完成。

## What Changes

- 接收原 3.11 的 CPU prepare 扩展和 4.1 尚未覆盖的逐 View/HDR 能力评估。
- 使用现有 FrameGraphBuildContext/feature 接入；已移除旧模板目标 delta，不复制第二套管线。
- 记录更多光类型、完整材质/HDR、Compute/Custom、storage/跨队列的扩展条件；每类开始前须限定格式、能力与可执行验收，不默认本 change 一次实现全部。

## Capabilities

### Modified Capabilities

- `aurora-pipeline`: 生产逐 View 管线及输出生命周期的后续验收契约。

## Impact

沿用 Aurora core/adaptor/runtime/rhi；World 资产事件输入由 aurora-scene-bridge 提供，CPU prepare 与 frame feature 保持各自职责。不得为支持 CPU/GPU 同构强行统一 system 与 pass。Metal 平台对等属于 aurora-renderer-platform-parity。
