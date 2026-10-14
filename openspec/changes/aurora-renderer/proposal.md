## Why

Aurora 已有 ECS scene、RHI、RDG 和资产基础，但游戏与编辑器各自驱动最小帧循环，场景数据尚未形成真实绘制链路。
本 change 建立一个可独立验证的 Renderer 主体：多个场景、每场景多个视图、窗口与离屏输出，共享设备和帧调度。
设计基线见 [design.md](design.md)；当前阶段为设计，实施任务尚未开始。

## What Changes

- 分离 `RenderScene`（ECS 状态）、`RenderView`（相机与逐视图求值）、`Viewport`（输出表面）。
- 增加 `IRenderer` 命令入口、带 generation 的句柄、拥有数据的 `FramePacket`、有界单生产者提交协议和完成通知。
- 使用主线程驱动的专用渲染线程；设备、GPU 缓存和场景 ECS 由渲染线程持有。
- 增加通用组件/资源读写声明与纯依赖编译；渲染线程协调 CPU prepare DAG，worker 按受限访问并行处理场景与 View。
- 定义与协程语法无关的任务组完成和 continuation 恢复契约；首版 prepare kernel 一次执行至完成，后续可接协程编排。
- 按 scene 更新一次、按 view 收集绘制，每帧合成一张 RDG；首版单 graphics queue，支持后端自己的串行/并行编码计划。
- 细化持久 ECS 的初建/增量更新、输入版本 stamp、冻结资源目录和精确版本 draw packets。
- FrameGraph 构建显式 roots、读取的 producer/version、CopyBlit 实际执行、编译后 bindings 与提交时状态发布。
- 各后端实现完整 Compile，构建 native passes、同步、EncodeTaskGraph 与 SubmissionPlan；同一后端 DeviceFrameContext Dispatch/收齐编码结果后提交，公共层只约束图语义并提供可复用纯分析。
- 资源 ticket 去重、预算、取消/热更新与 version leases；区分 Resident 和当前图 InGraph，首版使用同 graphics queue 的 RDG inline upload。
- 增加 `FrameScheduler`，复用 `DeviceFrameContext` 的唯一 GPU slot ring；通过显式 `DeviceFrameLease` 统一 command/descriptor/staging/UBO，分离 CPU prepare/build arena 和 GPU 退休。
- 将 acquire 从 RDG 编译移至帧调度；view 常量与 bindings 按 view 和 frame slot 隔离。
- 提供最小验证 pipeline：程序化静态网格、简单 Slang VS/FS、PSO、DrawItem，支持两个相机与窗口/离屏输出。
- 宿主通过接口使用同一 Renderer；新增实现遵循 engine 接口、plugin 实现的分层。
- **BREAKING**：pipeline context、viewport acquire、帧 fence 与 RDG 编译/执行入口改变；公共单 CommandBuffer Execute 迁移为后端 Compile + FrameContext Dispatch；淘汰空 Renderer / RenderDeviceExclusive 编排入口。

## Capabilities

### New Capabilities

- `aurora-renderer`: 主体对象关系、线程边界、命令协议、场景与视图求值、帧调度和最小出图验收。

### Modified Capabilities

- `core-ecs`: 可复用的访问描述、依赖推导及结构冻结后的受限读写视图；复用现有 ThreadPool 执行能力。
- `aurora-rdg`: 后端完整编译、logical/native pass 分离、CPU 编码图与 GPU 执行图、typed copy、版本化访问/剔除与资源生命周期。
- `aurora-rhi-core`: FrameContext Compile/Dispatch/收齐/Submit、显式 slot lease、worker 编码资源与安全 reset；复用并完善已有 dispatcher。
- `aurora-pipeline`: 显式 view context、无共享相机可变状态、viewport 与 frame slot 契约。
- `aurora-launcher`: 游戏模块转发到 Renderer，移除宿主内联 GPU 帧循环。
- `aurora-adaptor`: SceneProxy 提交接口及编辑器共享 Renderer 的接入边界。

## Non-goals

- 完整 Framework 组件提取：继续由现有 `aurora-scene-bridge` 实施，它消费本 change 的命令入口。
- 完整材质资产到 GPU 的产品链路、PBR、阴影、透明、蒙皮、地形、植被、导航可视化。
  这些是后续工作，当前不存在独立的 `aurora-material-pso` change；最小出图不依赖它。
- 重写已实现的 shader cache/cook；新增 ECS 库；首版 GPU culling 或 async compute；不要求所有 pass 都能并行编码。
- 引入协程/fiber runtime 或改写全部异步资产接口；本次仅固定可适配的完成与生命周期契约。
- 全量迁移历史 Aurora 目录、删除 legacy renderer 或清理 cook schema 债务。

## Impact

- 新增 engine 侧 Renderer 接口与 plugin 侧实现/provider；修改 Aurora runtime façade、pipeline、RDG 和帧同步。
- Sandbox 的 GPU 帧入口改为接口接入；UI 编辑与快照生成保持主线程，UI GPU 实现不进入 Renderer 核心。
- 修复本闭环必需的 ECS 删除语义和 executor 绑定顺序；已有核心规范继续适用。
- 不增加三方依赖。按本机实际 CMake cache 校准开发、测试和 Sandbox 配置后再实施验证。
- Vulkan / DX12 为首批集成验收；Metal 的窗口线程与 drawable 行为在平台验证前不宣称完成迁移。
