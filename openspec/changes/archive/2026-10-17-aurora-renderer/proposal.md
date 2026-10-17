## Why

归档准备与剩余任务移交见 [closeout.md](closeout.md)。当前只申请归档 Windows 已验收范围；Metal 与扩展仍未完成。

Aurora 已有 ECS scene、RHI、RDG 和资产基础，但游戏与编辑器各自驱动最小帧循环，场景数据尚未形成真实绘制链路。
本 change 建立一个可独立验证的 Renderer 主体：多个场景、每场景多个视图、窗口与离屏输出，共享设备和帧调度。
当前已有 CPU 并行 prepare、Vulkan 双 compiler 与 DX12 并行编码，以及 B10 图资源/上传/缓存、B11 Windows 输出、B12 生产 Renderer、B13 feature/多 View/UI 合成。B14 已完成 Windows 游戏/编辑器共享主流程、CPU UI 快照/精确纹理版本、真实 loader lease 和非空场景宿主验收。Metal 对等归 P02，World 自动提取归 scene-bridge；准确能力与验证记录统一见 [status.md](status.md)，不以 proposal 的目标范围推断全部跨平台工作已完成。

## Delivery and Document Ownership

主线按 **B01–B14 + DX12/Metal 平台线 P01/P02** 推进。B14 后按已验收 Windows 范围准备部分归档，P02 余项移交 `aurora-renderer-platform-parity`，CPU 扩展/旧模板 HDR 移交 `aurora-renderer-extensions`。完整依赖、范围、spec 映射和验收在 [batches.md](batches.md)：先两套 Vulkan scope，再并行 encode 与失败收齐，之后资源/真实绘制、生产生命周期、多 View 和宿主切换。各批只推进对应子集，不复制同一 capability 的 spec，不把平台 Unsupported 当作完成。

| 文档 | 唯一职责 |
|---|---|
| [proposal.md](proposal.md) | change 的目标、范围与外部依赖 |
| [design.md](design.md) | 目标架构、线程/所有权、后端与资源协议决策 |
| [specs/](specs) | 可验收行为；不记录完成比例或测试次数 |
| [status.md](status.md) | 当前代码能力、限制和最近验证 |
| [batches.md](batches.md) | 批次顺序、依赖、入口/退出条件和 capability 归属 |
| [tasks.md](tasks.md) | 保留稳定编号的整项验收 checklist，与批次交叉索引 |
| [review.md](review.md) | 历史实施、设计复审与验证证据；不再定义当前执行顺序 |

已有 `aurora-scene-bridge` 继续负责 World/Actor/组件事件到 FramePacket 的单向提取，消费本 change 的 SceneProxy/IRenderer/资源协议。它可先用 fake renderer 独立测试；真实宿主集成在 B12/B14 就绪后验收。terrain/vegetation/navigation 等 change 消费 B13 的 feature 边界。

## What Changes

- 分离 `RenderScene`（ECS 状态）、`RenderView`（相机与逐视图求值）、`Viewport`（输出表面）。
- 增加 `IRenderer` 命令入口、带 generation 的句柄、拥有数据的 `FramePacket`、有界单生产者提交协议和完成通知。
- 使用主线程驱动的专用渲染线程；设备、GPU 缓存和场景 ECS 由渲染线程持有。
- 增加通用组件/资源读写声明与纯依赖编译；渲染线程协调 CPU prepare DAG，worker 按受限访问并行处理场景与 View。
- 定义与协程语法无关的任务组完成和 continuation 恢复契约；首版 prepare kernel 一次执行至完成，后续可接协程编排。
- 按 scene 更新一次、按 view 收集绘制，每帧合成一张 RDG；首版单 graphics queue，支持后端自己的串行/并行编码计划。
- 细化持久 ECS 的初建/增量更新、输入版本 stamp、冻结资源目录和精确版本 draw packets。
- FrameGraph 构建显式 roots、读取的 producer/version、CopyBlit 实际执行、编译后 bindings 与提交时状态发布。B10 分子批交付：已统一物理 image 导入与 mip/layer 内容/拷贝，接入 transient 声明、device pool/frame leases、同图整对象 alias/物理同步、完整 RGBA8 2D image 上传发布及 Texture2D/Texture2DArray 采样、显式 mip/layer view 和 shader 类型反射；已补原生子资源状态、部分初始化导入、局部矩形/显式行距、跨图内容接续、buffer 字节范围及 buffer/RGBA8 image 共用分块上传；第十二子批补齐单 FrameContext/graphics queue 下显式纳管资源的候选、Accepted/completed 状态账本；第十三子批已接自动 uploader 纳管/退休、Resident 租约转交和 accepted 状态导入；第十四子批已接逐 mip/layer 初末边界及账本接续；第十五子批接入 RGBA8/D32 附件的单 mip/layer 显式 view；第十六子批接入设备支持的 RGBA8 2/4/8× MSAA color 与 pass 内 resolve；第十七子批接入匹配采样数的 D32 MSAA depth，与 color resolve 组合；第十八子批设计的 D32 MIN/MAX depth resolve 已在连续收尾接通三条原生路径，同时完成 device PSO/RenderPass 缓存、GraphBuildArena/提交引用移交和 pass 能力边界。B10 固定 Windows 范围完成；跨队列、更多格式/存储与 Metal 独立归属，不继续扩张本批。
- 各后端实现完整 Compile，构建 native passes、同步、EncodeTaskGraph 与 SubmissionPlan；同一后端 DeviceFrameContext Dispatch/收齐编码结果后提交，公共层只约束图语义并提供可复用纯分析。
- 资源 ticket 去重、预算、取消/热更新与 version leases；区分 Resident 和当前图 InGraph，首版使用同 graphics queue 的 RDG inline upload。
- 增加 `FrameScheduler`，复用 `DeviceFrameContext` 的唯一 GPU slot ring；通过显式 `DeviceFrameLease` 统一 command/descriptor/staging/UBO，分离 CPU prepare/build arena 和 GPU 退休。
- 将 acquire 从 RDG 编译移至帧调度；view 常量与 bindings 按 view 和 frame slot 隔离。
- 提供最小验证 pipeline：程序化静态网格、简单 Slang VS/FS、PSO、DrawItem，支持两个相机与窗口/离屏输出。
- 宿主通过接口使用同一 Renderer；接口与实现均归 Aurora foundation，在 engine/aurora 内按职责分 target。
- **BREAKING**：pipeline context、viewport acquire、帧 fence 与 RDG 编译/执行入口改变；公共单 CommandBuffer Execute 迁移为后端 Compile + FrameContext Dispatch；淘汰空 Renderer / RenderDeviceExclusive 编排入口。

## Capabilities

### New Capabilities

- `aurora-renderer`: 主体对象关系、线程边界、命令协议、场景与视图求值、帧调度和最小出图验收。

### Modified Capabilities

- `core-ecs`: 可复用的访问描述、依赖推导及结构冻结后的受限读写视图；复用现有 ThreadPool 执行能力。
- `async-threadpool`: 将既有 DeviceFrameDispatcher 抽取至 Core 复用，补有界批次、显式失败和完成收齐；拥有通用 Completion 的状态、订阅竞态、交付容量与指定 executor 协议。
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
- 原生后端目录搬迁与重命名：按用户要求恢复并保留 `engine/aurora/rhi/{vulkan,dx12,metal,common}`，让本 change 的 diff 聚焦功能实现。Aurora 已明确属于 foundation，不再规划迁入 plugins；仅目录命名整理可后续独立评估，不作为 B10–B14 的前置。

## Impact

- 新增 engine 侧 Renderer 接口与 foundation 内部的 Renderer 实现/provider；修改 Aurora runtime façade、pipeline、RDG 和帧同步。
- Sandbox 的 GPU 帧入口改为接口接入；UI 编辑与快照生成保持主线程，UI GPU 实现不进入 Renderer 核心。
- 修复本闭环必需的 ECS 删除语义和 executor 绑定顺序；已有核心规范继续适用。
- 不增加三方依赖。按本机实际 CMake cache 校准开发、测试和 Sandbox 配置后再实施验证。
- Vulkan / DX12 为首批集成验收；Metal 的窗口线程与 drawable 行为在平台验证前不宣称完成迁移。
- 代码按 batches 逐批推进，与本 change 的相关 spec 一起提交；主 specs 只在确认 archive 时合并。commit/archive 均按仓库规则由用户确认。
