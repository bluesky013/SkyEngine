## Context

`SandboxModule` 的 SceneImage 目前只展示纹理并更新 extent。`EditorRendererWindows::Tick` 从 RenderSceneBridge 取场景相机，缺失时使用固定 (0,0,5) 视图。`WorldEditorState` 已统一 Outliner、反射属性、World 文档和撤销；`SelectionService` 是唯一选择源。`CommandService::AddBeforeHistoryCallback` 已提供取消未提交预览的切入点。

`Framework/IGizmo.h` 仅有 Init(World, NativeWindow)，没有绘制、命中、更新或取消契约；本功能使用 Sandbox 消费方定义的独立可注入绘制接口，不修改该遗留扩展接口。现有 core 数学、UI 绘制和快照路径均可复用。`FirstPersonController` 直接订阅原生窗口输入，缺少编辑器焦点、模态、浮动 surface、orbit 与取消协议；这里复用数学类型，以纯状态导航接入 Shell 路由，避免额外原生事件订阅。

## Goals / Non-Goals

**Goals:** 可测试的相机/变换控制，独立可定制的 Gizmo 绘制，工具栏速度控制，单次拖拽撤销，输入/生命周期完整收尾，停留在现有 targets 内。

**Non-Goals:** 场景拾取、多选变换、自定义 pivot、刷子、碰撞/表面吸附、正交视图、场景 Camera 编辑替代导航、渲染器/目录/target 重新划分。

## Decisions

### 1. 现有 target 内分工

- EditorCore：相机状态/投影与导航数学、Gizmo handle/几何数据、绘制 provider 接口、命中与拖拽计算、反射控制设置；只依赖 Core/Framework。
- EditorShell：可聚焦的视口面板、UI capture、绘制数据到 UIPaintContext 的适配；颜色/尺寸从 UiTheme 传入。
- SandboxModule：绑定 WorldEditorState、SelectionService、CommandService、PlaySession，注册 action 和控制设置面板，持有控制器与 provider。
- EditorRender：接收值类型编辑相机快照；通过现有 UpdateView 和 UI snapshot 同帧提交。无新增 Aurora target/模块入口。

### 2. 可定制绘制与一致命中

定义无 UI/GPU 类型的 `IGizmoVisualProvider`。输入是相机、视口 extent、pivot/axes、工具/空间和样式尺寸的值快照；输出是拥有内存的视口局部布局坐标中的折线/凸填充多边形及稳定 handle ID。默认 provider 生成平移轴与平面、旋转环、缩放轴与中心手柄。宿主可注入其它 provider；共享所有权确保调用期间有效，替换时先取消手势。

绘制和命中消费同一输出，命中容差由主题转换为逻辑像素。控制器只识别 handle 的语义（X/Y/Z/XY/XZ/YZ/Uniform），不依赖默认外观。自定义 provider 可替换几何、颜色语义或隐藏手柄；无有效几何则不可命中。填充要求非退化凸多边形，凹形可分解成共享 handle 的凸块；这样绘制三角扇与命中轮廓一致。默认边缘朝向相机的退化平面不输出。非法/非有限几何拒绝并报告错误，导航仍可用。交互不访问 renderer 内部资源。

选择 UI 层合成是因为该路径已经提交 owned snapshot 并处理纹理/浮动窗口；不增加 ImGuizmo 或独立 GPU pass。首期 Gizmo 为置顶 overlay，不承诺场景深度遮挡；裁剪到实际图像内容矩形，中心在相机后方/近裁剪面以内时不显示。

### 3. 编辑相机与速度

Edit 使用独立相机，初始 (0,0,5)、朝 -Z、Y-up；导航不修改 Actor/CameraComponent，不进入 World 撤销/dirty。Play 和 Paused 沿用既有场景相机路径，返回 Edit 保留编辑视图。切文档重置相机并取消输入。

| 输入 | 行为 |
|---|---|
| 右键拖拽 | yaw/pitch，pitch 避开极点 |
| 右键持有 + W/S/A/D/Q/E | 前/后/左/右/下/上，按 deltaSeconds 积分，斜向归一化 |
| Shift + 飞行 | 临时速度倍数，释放恢复 |
| 中键拖拽 | 相机平面平移 |
| 滚轮 | 沿视线推进 |
| Alt + 左键 | 绕当前选中 pivot（无选中则已有 orbit pivot）旋转 |
| F | 对准唯一选中对象 pivot，采用当前有效 orbit distance |

工具栏速度减小/增加 action 与反射设置共用一份有界正数 speed，立即影响导航；当前速度可在工具栏提示/状态反馈读取。工具栏设置 action 打开 ReflectedFormView，提供速度、加速倍率、以度/像素表示的灵敏度和三种吸附步长，使用独立设置历史，不污染 World 历史；不手写逐字段配置 UI。设置默认是会话内数据，不增加磁盘 schema。

### 4. 变换与取消协议

仅唯一选中的、属于当前 edit World 的 Transform Actor 可编辑；多选/资产/失效选择显示禁用状态。pivot 为 Actor 世界位置。平移支持三轴/三平面，旋转支持三轴，缩放支持局部三轴和均匀缩放。World/Local 对平移与旋转生效；缩放始终使用 Local，空间开关在缩放模式禁用并展示真实状态。

拖拽开始冻结工具/空间、相机、父级与 local before、handle 约束、指针位置。所有候选由起点计算，吸附作用于相对增量（平移单位、旋转角度、缩放倍率），不累计取整。缩放使用正倍率并保留初始符号，不跨零镜像。零/非有限父缩放等不可逆情况显式拒绝，射线与约束近退化时保留最后有效值。沿现有 TRS 约定按父旋转/缩放转换位置和方向，不通过通用矩阵分解引入 shear。

状态为 Idle → Preview → Commit/Cancel。预览调用 Transform setter 并通知属性刷新，但不入历史、不标 World dirty。提交前恢复 before，再执行包含 before/after local Transform 的一个命令；这样 Execute/Undo/Redo 始终是绝对赋值，无重复增量。零变化不建命令。提交、undo、redo 标 dirty；取消恢复 before 且不清除已有 dirty。

Esc、pointer cancel、失焦、模态打开、文档切换、目标删除/替换、父变换改变、进入 Play、历史遍历、保存均先取消预览；对象已失效时只释放状态，不解引用旧指针。命令按 World 与 Actor UUID 解析，文档切换按现有逻辑清空历史。保存取消预览后只写已提交值。

### 5. Action 与输入归属

Q/W/E/R 对应选择/平移/旋转/缩放，X 切 World/Local，G 切吸附，Esc 取消，F 聚焦。全部声明为 EditorAction，ToolBar/MenuBar/快捷键通过 ActionBarBuilder；增加工具/导航分组，使用 SVG 图标。工具状态与命令 enabled/checked 共用源。工具切换、空间、吸附属于视口选项；聚焦选择属于上下文操作。

快捷键需要独立的 accelerator eligibility（默认不限制），避免将“焦点不在视口”当作 disabled action 而吞掉文本 W/E/R。工具栏调用仍按普通 action enabled。Eligibility 由宿主的视口 focus、模态与导航状态决定，不在公共 core 引用 UI。飞行期间 W/E/Q 归导航所有。主窗口与浮动窗口走同样的判定，保留原有文档/对话框快捷键。

Win32 在现有键盘事件入口补转发 WM_SYSKEYDOWN/UP，以传递 Alt 修饰键；仍保留 DefWindowProc 的系统键行为。鼠标按下事件附带当前 KeyMod 位，Win32/Cocoa 在事件源采样，避免系统菜单吞掉 Alt release 后锁住 orbit，同时支持先按 Shift 再按右键的加速。滚轮按已有 120 单位/格契约归一化。原生事件只通过已有 window/surface 路由进入面板；同一 pointer 的手势保持 capture，可在图像外完成释放。模态/菜单、输入框、Gizmo 拖拽、相机手势彼此互斥。焦点丢失清空所有按键和导航按钮；重排/换 surface/resize 在手势中取消，避免旧投影跳变。

### 6. 同帧与平台边界

相机、场景提取和 overlay 使用同次 host tick 的快照。沿 renderer 的 Y-up、[0,1] depth 和 top-left UI 映射计算，避免 Vulkan/DX12 重复翻转；DPI 只在 surface/layout 边界换算一次。Busy 时保持旧封包，后续输入不能修改已提交数据。首轮 native 验收为 Windows Vulkan/DX12；不得把 headless 可移植编译当作 Metal 原生验证。

## Risks / Trade-offs

### Navigation latency follow-up

Windows Debug/Vulkan 的相机回放测得默认约 5 个尚未完成的快照，快照到 GPU 完成通知中位 141.8 ms；限制为 2 个后为 67.3 ms。EditorRender 在生成 GUI/相机快照前检查最多 2 个已接收帧的 completion credits；BUSY 保留原封包，FrameCompleted（含无 GPU 工作的 serial=0）按精确 sequence 归还，PacketApplied 不归还。宿主仍处理输入、窗口事件和 PollEvents，停止走原 RendererHost drain；不丢弃、替换或合并生命周期/资源命令。此上限作用于编辑器宿主，不改 Aurora 队列、GPU slot 或公共 renderer 默认值。

Gizmo 三角形在 UIPaintContext 中应用 transform 后，以 double 插值裁剪到当前有效 clip，再提交有界顶点与三角扇；同样覆盖自定义 provider、线宽扩展和浮动 surface。Scissor 保留为渲染约束，非有限输入继续交由已有 snapshot 校验报错。

Vulkan TryAcquireFrame 保留已知失效状态检查和 native timeout=0 acquire，去掉内部重复的完整 QuerySurface。FrameScheduler 的 prepare 查询及 acquire 后实时复核保持原样，配置变化/错误时继续 abandon 并按 fence 退休；不跨帧缓存 surface 能力，不放松 resize/close 资源契约。Shell 在实际 Layout 时刷新 action 状态，输入调用仍实时校验 enabled/visible；图标重绑仅在 action/scope 重建或 SetIcon 改变时执行。

限帧改善输入反馈延迟，但不保证增加吞吐；CPU 帧图编译和全量 UI 绘制仍需按测量结果评估。本次不引入 UI 缓存失效体系或新的渲染 target。

Review follow-up: capture 按 pointer + 起始 button 归属；同一 pointer 的其它按钮边沿被消费，不改 focus/capture，原生 capture 释放跟随该 surface 的 router 状态。旋转 no-op 比较使用符号对齐的四元数分量差，避免 dot 接近 1 的 float 精度吞掉 0.1° 变更。飞行将视线与世界上下方向合成后归一化；填充轮廓的绘制/命中始终闭合，closed 仅控制折线。视口实体拾取由独立 `sandbox-viewport-picking` 提案跟踪。

- 置顶 overlay 无深度遮挡 → 明确首期语义，保持 handle 可见与命中一致。
- 近视轴手柄退化 → 采用稳定拖拽投影/约束并拒绝退化，覆盖相机对齐负例。
- 静态库跨 DLL 状态分裂 → 状态由模块显式传递；注册使用现有 Singleton，反射初始化使用 Once。
- 宿主输入/属性 callback 生命周期 → 使用现有 capture/cancel 与历史前置回调，析构/解绑先取消，测试失效对象与文档切换。
- 多窗口键盘路径不一致 → 主/浮动 surface 共用 viewport accelerator eligibility 与消费规则。

## Migration Plan

先实现/测试无 GPU 数学与绘制契约，再接 World 命令、UI、相机快照和 action。沿现有 build 目录关闭编辑器、重新配置、构建相关 targets，运行核心/壳/模块回归及有界 native 编辑会话。最终记录实际通过结果与平台限制；本 change 不自动 commit/archive。

## Open Questions

无阻塞问题。用户已授权在现有模块结构内完成绘制抽象、速度控制及实现验证。
