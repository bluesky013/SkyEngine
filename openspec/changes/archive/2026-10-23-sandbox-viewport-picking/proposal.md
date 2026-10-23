## Why

Sandbox 已有视口相机和 Gizmo，但仍只能通过 Outliner 选择对象；Q 只是切到选择工具，不能点击场景选择。需要独立交付视口拾取，同时保持 Gizmo、导航、选择和撤销的现有归属。

Status: active，阶段 A 15/15、阶段 B 20/20 和后续几何查询扩展接口 3/3 的验收记录见 `verification.md`。Overlay 绘制优先级 review 项仍待修，合计 38/39，不以原测试通过宣称 review 已清零。阶段 C 的 GPU 后端仅为目标设计，不列入本 change 的交付任务。依赖 `sandbox-viewport-gizmo-controls` 已于 2026-10-23 集成并归档。

现有基线暴露了新的边界：2 万三角形的 BVH 冷构建约 359 ms，查询逐实例扫描；支持能力依靠内置材质 ID 判定，无法接入普通自定义材质和非网格编辑代理。需要在现有消费方接口与模块边界内补齐这些能力，同时明确未来 GPU 拾取需要的屏幕、帧和遮挡契约。

## What Changes

- 为 Edit 视口增加左键点击单选静态网格 Actor、空白点击清空选择；与 Outliner / Inspector / Gizmo 共用 SelectionService。
- 定义 EditorCore 消费方的拾取接口和中立查询/结果数据；在现有编辑器渲染适配 target 内实现 CPU 射线、包围盒粗筛、三角形精确最近命中。
- 使用自有、带版本的网格查询快照与加速缓存；支持父变换、非均匀及负缩放，区分未命中、资源待就绪、不可支持和错误。
- 明确 Gizmo / 相机优先、点击阈值、主/浮动 surface 坐标、焦点与文档切换取消协议。
- 增加可复现的几何、输入、生命周期、缓存失效测试，以及 Vulkan/DX12 原生验收。
- 优化共享网格 BVH 构建，增加实例级空间索引、增量变换更新和有界查询调度；相机移动不重建几何，未发生变化时不重复维护加速结构。
- 在 EditorCore 定义可注册的编辑拾取 provider 契约，在既有 EditorRender 中组合实现；用拥有内存的三角形、线段和图标代理扩展对象种类，保持 Gizmo 绘制/拖动接口的归属。
- 以生产者显式声明、带修订的几何/材质能力替代 picker 内的材质身份特判；未知或需要 GPU 的语义仍明确返回 Unsupported。
- 扩充请求的视口像素/相机快照、策略和取消契约，结果区分几何表面、编辑代理和未来可见表面；规定遮挡、稳定排序和旧结果处理。
- 补充几何查询扩展接口：provider 发布粗粒度世界 bounds 和不可变查询对象，提供方可复用自己的 BVH/空间索引；现有 picker 负责有界续跑、取消和统一仲裁。本轮不实现地形/植被适配或新的空间索引算法。

本 change 不包含框选、多选、子元素独立选择、骨骼/顶点着色变形的实际拾取、透明纹理逐像素判定、地形/植被专用适配、自动文件监控或 GPU ID pass。设计明确后续 GPU 启动条件和跨后端验收，不能将目标设计当作已支持能力。

## Capabilities

### New Capabilities

- `editor-viewport-picking`: 视口点击单选、CPU 两层空间查询、自定义编辑代理、显式能力与命中语义、有界调度、查询生命周期和输入优先级。

### Modified Capabilities

无。消费 `sandbox-viewport-gizmo-controls` 的相机与输入契约，不改写其 Gizmo 能力归属。

## Impact

- EditorCore：拾取/provider 接口、数据和选择应用逻辑，不引入 Aurora/RHI/UI 依赖。
- EditorRender / SandboxModule：现有 target 内组合网格数据适配、实例空间索引、缓存与查询服务；SceneViewport 负责点击和坐标映射。已有业务 editor 模块可通过接口注册 provider，不向 runtime/core 反向引入 editor 依赖。
- 复用 Aurora MeshAssetData / SceneAssetSource 的网格与资产修订信息；如需补充共享数据出口，仅在既有 adaptor 边界提供中立、拥有内存的快照。不得由 Aurora 反向依赖 Sandbox。
- 无新增 module/target、业务 plugin、装载入口和目录搬迁；不依赖 Physics Raycast 或 GPU readback 才能选择静态网格。
