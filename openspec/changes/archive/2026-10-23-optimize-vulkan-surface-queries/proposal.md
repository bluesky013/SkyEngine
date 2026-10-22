## Why

Sandbox 旋转相机的原生采样显示，每帧两次 Vulkan surface 完整协商；Release 单次约 3.38 ms，主要花在格式枚举。稳定帧应保留实时状态复核，同时避免重复枚举格式和呈现模式。

## What Changes

- 拆分实时 capabilities/窗口尺寸查询与 format/color-space/present-mode 协商。
- 创建、重建和 native SUBOPTIMAL/OUT_OF_DATE 后重新协商；稳定帧复用已应用选项。
- 保留获取后复核、原生错误、成功获取图像的同步责任、resize/close 退休约束。
- 补充原生调用计数、失效与恢复测试及 Debug/Release 同场景性能对比。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `aurora-rhi-core`: 明确 Vulkan 稳定帧 surface 查询的枚举预算及协商失效边界。

## Impact

仅修改现有 VulkanSwapChain 与 FrameOutputs 原生测试，无新 target、模块、目录搬迁或公共 RHI 接口。延续已提交 `sandbox-viewport-gizmo-controls` 的 acquire 后复核；不改 Pick 提案、renderer 调度所有权及其它后端。Develop 构建模式独立评估，不并入本次性能修复。
