## Why

Sandbox 已能创建 World 对象、通过 Outliner 选择并在 Inspector 修改 Transform，但主视口仍是场景图像：没有可交互的编辑相机，也没有移动/旋转/缩放手柄。补齐相机导航与 Gizmo，可让用户直接观察并摆放对象，形成带撤销、保存和输入隔离的场景编辑闭环。

## What Changes

- 为编辑视口提供独立相机：右键观察与 WASD/QE 飞行、Shift 加速、中键平移、滚轮推进、Alt+左键绕选中对象旋转、F 聚焦。
- 工具栏提供相机速度调整与当前速度反馈；速度/灵敏度/吸附步长等配置通过反射数据及通用面板编辑。
- Gizmo 绘制通过可注入接口抽象，默认实现与自定义实现共用稳定的 handle 标识、投影和命中数据，不将具体外观绑定在交互控制器内。
- 为 `SelectionService` 中唯一选中的、具有 Transform 的 Actor 提供平移、旋转、缩放 Gizmo；支持轴/平面平移、轴旋转、局部轴/均匀缩放，以及平移/旋转的 World/Local 空间切换和步进吸附。
- 提供 Q/W/E/R 工具切换、X 空间切换、G 吸附开关、Esc 取消，通过统一 action registry、图标工具栏、菜单/上下文菜单及快捷键入口呈现。
- 一次有效拖拽产生一个撤销命令；预览与提交分离，取消恢复原值，Inspector 与场景同步，保存持久化最终 local Transform。
- 明确文本输入、菜单/模态框、相机导航和 Gizmo 捕获的优先级；覆盖失焦、切换文档、删除对象、拖出视口、浮动窗口和进入 Play 的收尾。
- 编辑相机在 Edit 模式拥有视图控制权；Play/Paused 沿用已有游戏相机路径，停止后恢复编辑视图。

首期使用现有 Outliner 选择入口，仅操作单个 Actor。不包含视口场景拾取/框选、多对象变换、自定义 pivot、地形/植被刷子、物理吸附、正交多视图、profiler overlay 或 Play/Pause/Stop 功能扩展。

## Capabilities

### New Capabilities

- `editor-transform-gizmo`: 单对象变换手柄、坐标空间、步进吸附、可取消预览、单步撤销及统一工具 action。

### Modified Capabilities

- `editor-viewport`: 补充编辑相机导航、输入捕获与快捷键仲裁、同帧 overlay 对齐及编辑/运行相机归属。

## Impact

- 扩展现有 `engine/sandbox/core`、`shell`、`module`、`render`、`ui`、`resources/icons` 与 `test`；复用 `EditorCore`、`EditorShell`、`EditorRender`、`EditorUi`、`EditorWidgets`、`SandboxModule` targets，不新增 target、模块入口或目录搬迁。
- 复用 `SelectionService`、`CommandService`、`WorldEditorState`、`TransformComponent`、`EditorActionRegistry`、`ActionBarBuilder`、`ReflectedFormView`、UI pointer capture 和 `UiSnapshotProducer`；核心计算保持无 UI/GPU 依赖。
- 消费 `aurora-world-editor` 已实现的 World 编辑链路和 `aurora-scene-bridge` 已有的提取能力；不依赖后者尚待完成的其它验收，不改写其职责或重开已完成任务。
- 编辑模式从“场景 CameraComponent 优先”改为“编辑相机优先”，场景 CameraComponent 的数据与 Play 语义保持一致；此行为变更需要专门回归。
- Gizmo 采用现有 UI 绘制/快照合成路径，不引入 ImGuizmo、Qt、物理插件依赖或 Aurora 模块调整。具体设计与验收见 design/specs/tasks。
- 导航性能修复增加编辑器帧生成节流、UI 三角形几何裁剪，并减少 Shell action 刷新和 Vulkan acquire 的重复 surface 枚举；沿用原 targets、完成回执与 native 退休协议。
