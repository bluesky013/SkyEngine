## Why

现有 Animation 已有采样、混合、基础状态机与执行计划，但 Actor 缺少骨骼网格/动画组件，Aurora 生产路径拒绝 skin mesh，`.graph` 仍是版本号占位，资产预览也未接入实际渲染。需要建立可保存、可播放、可预览、可编辑的骨骼动画工作流，并用真实多实例角色验证跨层契约。

## What Changes

- 增加 `SkeletalMeshComponent` 与 `AnimationComponent`，分离渲染绑定、播放控制和每实例姿势；由 World 级动画系统统一求值，经消费方接口发布 renderer-neutral 骨骼模型空间矩阵。
- 按用户确认采用 Godot 式组合：独立 `AuroraSkin` 子资产保存 inverse bind 与骨骼目标标识；组件显式选择 Mesh / Skin / Skeleton 与 Pose 来源。Mesh 只含几何、joint/weight 槽数据，不强绑某一套骨架。
- 在既有 Aurora scene bridge、FramePacket、资源准备和场景 Pass 中接通顶点蒙皮、阴影、动态 bounds 与在途生命周期；复用唯一 RendererHost。
- 修正触及链路的骨架/skin 校验、坐标空间、稳定子资产身份与 clip 时长；保留已有 `.clip` 输入。FBX/glTF 动画 take 导出不伪装成已具备能力，首版不新增第二套模型 importer。
- 扩展现有 Framework 的 generated subproduct catalog 和产品组发布：子资产可选择、可触发 owner cook，保存返回显式结果，加载固定 revision，失败保留上一完整版本。
- 用版本化 Graph 文档和 cooked 定义替换空 `.graph`：Clip、Blend2、Output、bool/float 参数与单层状态机；明确编译诊断、依赖和实例化。复杂嵌套图、事件、角色 Root Motion 集成不混入首版。
- 增加角色/Clip 预览、骨骼树、时间轴与动画图编辑器，复用 Sandbox 的文档、撤销重做、反射属性、action、theme 和渲染宿主。
- **BREAKING**：clip/mesh/skeleton/graph 的受影响产品格式需升级与 recook；Mesh v5 内嵌 Skin / skeleton 引用拆为独立产品和组件引用，移除 Mesh::SetSkin/GetSkin/HasSkin；旧空 Graph 不可运行。Skin 的可变调色板改由实例/帧持有。
- 按资产与渲染基础、单 Clip 播放、预览、Graph 运行时、Graph 编辑器分批验收。新增动画 feature target/装载入口已在 2026-10-24 获用户 `apply` 确认，按 design D1 实施。

## Capabilities

### New Capabilities

- `animation-world-components`: 动画组件、World 更新、播放状态、资产切换、错误与 Pose 发布。
- `animation-graph-assets`: 图文档、参数/节点/状态机、cook 校验、依赖、运行时实例和热重载。
- `animation-editor`: 角色/Clip/Graph 编辑预览、骨骼树、时间轴、节点图、保存和调试。

### Modified Capabilities

- `animation`: 明确保留原始片段时长、播放/seek 行为、每实例执行状态和旧 Root Motion 输出的边界。
- `aurora-skinning`: 分离不可变 skin 绑定与实例调色板，定义蒙皮、bounds、Pass 一致性和 GPU 生命周期。
- `aurora-adaptor`: 增加骨骼网格组件、消费方 Pose 接口和 World 到 renderer 的原子快照。
- `aurora-cook`: 校验原始权重/节点空间，分别生成 Mesh / Skin / Skeleton 产品，保持 skin 槽与 rig 索引独立。
- `asset-source-identity`: 持久化 generated 子身份及 owner 关系、默认导入绑定元数据，不改变源 UUID 规则。
- `asset-product-loading`: 产品组原子发布、精确 revision 读取、generated child 按需 cook 与完成通知。
- `content`: 移除 Content 对空 Graph handler/builder 的规范所有权，Clip 继续由原模块维护。
- `aurora-mesh`: 清理旧 Mesh 持 Skeleton 的冲突条款，明确 JOINTS 索引 Skin 槽。
- `aurora-mesh-cook`: 同步原始导入校验、顶点空间处理及独立 Skin 产品格式；不恢复旧 meshlet 字段。
- `editor-asset-viewer`: 将 bool 预览占位接口扩展为 session 生命周期，Skin 提供反射只读摘要。

## Impact

既有 `engine/framework` 资产链、`engine/animation`、`engine/content/{adaptor,builder,editor}`、`engine/aurora/{core,pipeline,adaptor,cook,assets}`、`engine/sandbox/{core,widgets,render,shell}`；拟新增 `plugins/animation/{core,adaptor,builder,editor}` 与明确编译/加载配置。不创建 Aurora plugin，不引入新 RHI 后端或并行 Renderer，不搬迁既有目录。

`design.md` 的模块表已确认，保留旧 `Animation` Core-only 库作为现有基础，不借此扩大 engine 中的业务实现。旧 Content 的空 Graph handler/builder/view 将在原位置移除注册，由动画 plugin 唯一接管 Graph；Clip/Skeleton 原 handler 保持单一归属。

## Scope and status

**Status: applying。** 2026-10-24 用户在整体严格 review 后以 `apply` 确认 Godot 式组合、新增模块/target/入口与首版范围。实施进度以 `tasks.md` 为准，验证证据见 `review.md`；commit 和 archive 仍需分别确认。

首版从可用的 `.clip`、mesh、skin 和 skeleton 产品开始，完成组件、渲染、Graph 与编辑闭环；模型动画 take 导出、ACL 资产链路、Notify、物理驱动 Root Motion、IK、BlendSpace、挂点与重定向另行定界。已有 `animation-events`、`audio-animation-bridge` 保持各自所有权。与现有场景/着色器 change 的交界及证据见 `review.md`，不得把其待办标为本 change 已完成。
