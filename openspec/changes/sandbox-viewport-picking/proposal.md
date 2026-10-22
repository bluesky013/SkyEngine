## Why

Sandbox 已有视口相机和 Gizmo，但仍只能通过 Outliner 选择对象；Q 只是切到选择工具，不能点击场景选择。需要独立交付视口拾取，同时保持 Gizmo、导航、选择和撤销的现有归属。

Status: Ready for implementation after `sandbox-viewport-gizmo-controls` is integrated. 本轮仅提案，不包含 Pick 实现。

## What Changes

- 为 Edit 视口增加左键点击单选静态网格 Actor、空白点击清空选择；与 Outliner / Inspector / Gizmo 共用 SelectionService。
- 定义 EditorCore 消费方的拾取接口和中立查询/结果数据；在现有编辑器渲染适配 target 内实现 CPU 射线、包围盒粗筛、三角形精确最近命中。
- 使用自有、带版本的网格查询快照与加速缓存；支持父变换、非均匀及负缩放，区分未命中、资源待就绪、不可支持和错误。
- 明确 Gizmo / 相机优先、点击阈值、主/浮动 surface 坐标、焦点与文档切换取消协议。
- 增加可复现的几何、输入、生命周期、缓存失效测试，以及 Vulkan/DX12 原生验收。

首期不包含框选、多选、骨骼/顶点着色变形、透明纹理逐像素精确拾取、地形/植被专用拾取或 GPU ID pass。后续若要求与最终画面逐像素一致，再评估 ID pass 和异步小区域回读。

## Capabilities

### New Capabilities

- `editor-viewport-picking`: 视口点击单选、CPU 几何命中、查询生命周期和输入优先级。

### Modified Capabilities

无。消费 `sandbox-viewport-gizmo-controls` 的相机与输入契约，不改写其 Gizmo 能力归属。

## Impact

- EditorCore：拾取接口、数据和选择应用逻辑，不引入 Aurora/RHI/UI 依赖。
- EditorRender / SandboxModule：现有 target 内组合网格数据适配、缓存与查询服务；SceneViewport 负责点击识别。
- 复用 Aurora MeshAssetData / SceneAssetSource 的网格与资产修订信息；如需补充共享数据出口，仅在既有 adaptor 边界提供中立、拥有内存的快照。不得由 Aurora 反向依赖 Sandbox。
- 无新增 module/target、业务 plugin、装载入口和目录搬迁；不依赖 Physics Raycast 或 GPU readback 才能选择静态网格。
