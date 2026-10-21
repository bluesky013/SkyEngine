## Why

新 frame IR 已经暴露 `FrameComputePass` / `FrameCustomPass`，但两种 pass 没有任何资源访问声明，所有后端在 `AnalyzeFrameGraph` 阶段直接返回 Unsupported。结果是 Compute/Custom 既无法用于延迟渲染的 GPU 效果，也没有可用的逃生门；同时因为缺少声明，barrier 无法复用后端已有的状态跟踪，只能各自另写一套——这正是当前不实现的原因。本 change 把“声明访问 + 后端接入”一次补齐，让 Compute/Custom 回到统一的 barrier/内容/lifetime 模型。

## What Changes

- 扩展 `FrameComputePass` / `FrameCustomPass`：增加显式的 buffer/image binding（set/binding、range 或 mip/layer、READ/WRITE/READ_WRITE）、sampler 以及可选的 custom encoder 回调；pass 仍保持声明式，barrier 由后端推导，回调不得自行插入 barrier。
- 为 buffer 边界状态补充 shader read / unordered-access 状态，使 compute/custom 的导入、导出和跨 pass 状态与 image 的 `SHADER_READ_ONLY` / `GENERAL` 对齐。
- `AnalyzeFrameGraph` 增加 compute/custom 的访问、内容定义、lifetime 与 handle/range/view/access 校验；无声明或声明不足的读取按未定义内容拒绝。
- Vulkan（RenderPass 与 Dynamic Rendering 两套 compiler）、DX12、Metal 接入 compute/custom 编码：按声明推导 barrier、拆分 native scope、串行 coordinator 编码；后端能力不足时明确返回 Unsupported，不做空分支伪成功。
- 测试覆盖：compute 写 storage buffer/image 后 copy/raster 读回、custom 回调读写、barrier 顺序、未定义内容/非法声明拒绝、两套 Vulkan compiler 对照、DX12 对照；Metal 实机验收复用 `aurora-renderer-platform-parity` 的 P02 环境。
- 收编 `aurora-renderer-platform-parity` 4.20 中 Compute/Custom 的实施范围，并把 `aurora-renderer-extensions` E2 的 Compute/Custom 条目指向本 change；两个 change 不再各自实现同一能力。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `aurora-rdg`: Compute/Custom pass 从“显式 Unsupported”改为带声明访问的受支持 pass kind，并明确 barrier、内容、lifetime 与后端编码契约。

## Impact

- 接口/数据：`engine/aurora/rhi/interface/include/aurora/rdg/FrozenFrameGraph.h`、`FrameExecutionTypes.h`、`FrameGraphAnalysis.cpp`。
- 图构建：`engine/aurora/core/src/renderer/graph/FrameGraphBuilder.*`。
- 后端：`engine/aurora/rhi/vulkan/src/rdg/*`、`engine/aurora/rhi/dx12/src/rdg/*`、`engine/aurora/rhi/metal/src/rdg/*`。
- 测试：`engine/aurora/core/test/renderer/FrameGraphTest.cpp`、`VulkanFrameCompilerTest.cpp`、`D3D12FrameCompilerTest.cpp`、`engine/aurora/rhi/test/MetalFrameContextTest.cpp`。
- OpenSpec：`openspec/changes/aurora-renderer-platform-parity/tasks.md` 4.20、`openspec/changes/aurora-renderer-extensions/tasks.md` E2 的归属调整。
- 不新增 plugin、不移动目录、不改变 Aurora foundation 归属；暂不支持跨队列/async compute、后端专属 custom 二进制契约。
