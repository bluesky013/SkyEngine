## Context

`sandbox-viewport-gizmo-controls` 已提供 EditorCamera::Ray、SceneViewport 的图像内容矩形、手势捕获和 SelectionService 绑定。该 change 尚未归档，Pick 实施依赖其集成，不重复设计相机或 Gizmo 绘制接口。当前 Q 不执行场景拾取。

Aurora adaptor 的 MeshAssetData、SceneAssetSource::ConvertSceneMesh 已掌握 CPU 顶点/索引、布局、bounds 和资源修订；RenderSceneBridge 掌握 Actor UUID、组件/part 到渲染实体的对应关系，但没有公共拾取查询出口。Physics Raycast 只覆盖 collider，且结果不是编辑 Actor UUID，不能作为通用可见对象选择入口。

当前 RendererRuntime 的回读路径面向整个 offscreen RGBA8 视口；RDG 现有颜色格式集合不包含 R32_UINT。因此 GPU ID pass 不是现成接口的一次调用，需要独立扩展格式、实体映射、深度一致性和异步区域回读。

## Goals / Non-Goals

**Goals:** Edit 模式点击选择静态三角网格的最近 Actor；无 collider 也可选；空白清空；主/浮动窗口一致；遵循共享选择、手势归属和生命周期；查询失败可诊断；保持现有模块/target 结构。

**Non-Goals:** 框选、多选、对象循环选择、子网格独立选择、隐藏对象选择、非网格图标选取、蒙皮/程序变形、alpha 纹理逐像素判定、地形/植被专用查询、物理碰撞查询替代渲染几何、GPU pass 和同步 GPU 等待。

## Decisions

### 1. 消费方接口与提供方适配

- `engine/sandbox/core` 定义 `IViewportPicker` 与中立请求/结果：相机射线和裁剪范围、文档世代、场景/视图/布局修订、请求序号；返回状态、Actor UUID、part、世界空间命中距离与位置。不返回 Actor/Component 裸指针、RHI 句柄或借用顶点视图。
- 具体 CPU picker 位于现有 `engine/sandbox/render` target，由 SandboxModule 组合。EditorCore 不依赖 EditorRender/Aurora，Aurora 不 include 或实现 Sandbox 接口。
- 在现有 Aurora adaptor 的 RenderSceneBridge 提供中立、只读、拥有内存的静态网格快照出口，复用提取时已确定的 Actor/part、世界变换、可见性、mesh payload 和修订。不得另写一套枚举具体组件类型的拾取分支，也不得把 editor selection 状态塞入 Aurora。
- `SceneAssetSource` 仍负责资产转换和加载错误，picker 不自行复制一套 asset 解码器。快照引用不可变共享几何，退出 World 或释放 source 后也不悬空；不得从 renderer worker 的可变容器直接借用数据。

不新增 target、模块入口、注册表或目录搬迁。若实施证明必须改变这些边界，先按 AGENTS.md 展示调整方案再征求确认。

### 2. CPU 几何拾取

使用包围盒粗筛，随后对当前支持的静态三角形数据做精确射线求交。将世界射线通过对象逆变换转到局部，命中点再转回世界，用沿归一化世界射线的距离比较；不能直接比较各对象局部 t。拒绝非有限或不可逆变换、越界索引、无效顶点布局，退化三角形跳过并记录可诊断状态。

遵循快照的可见性、有效裁剪范围及已支持的材质剔除状态；负行列式变换需同步处理绕序。首期仅承诺受支持的静态几何表面，alpha/透明和 shader deformation 不声称像素级一致。缺失必要几何/材质语义的候选返回 Unsupported，而不是用 bounds 冒充精确命中。

每个 mesh 几何修订缓存 bounds + 三角形 BVH，实例复用它，实例变换不重建 mesh BVH。缓存 key 包含资产身份、几何修订/布局；实现明确的字节预算、LRU 淘汰和无外部引用后释放，不做无界进程单例。查询只访问已准备的快照；资源加载/BVH 构建不在 pointer 回调里阻塞 UI。过期构建结果不能覆盖新修订。

备选：纯 AABB 成本低但会选中空洞/斜面外区域；Physics 不能覆盖无 collider 的可见网格；GPU 能更贴近最终图像，但当前基础设施改动较大。首期选择 CPU 精确几何查询。

### 3. 点击识别与手势优先级

优先级为现有模态/菜单 → 已有 capture → Alt+LMB orbit / RMB / MMB 导航 → Gizmo handle → 场景点击候选。点击候选仅在 Edit 模式、内容矩形内的 LMB down 建立，匹配 pointer/button 的 up 仍在内容内且移动未超过主题拖拽阈值时触发。Q 与 W/E/R 均可点空白位置选对象，Gizmo 命中优先。

候选期间若累计最大位移超过阈值，即使移回原点也不拾取。失焦、取消、Esc、切工具、布局/DPI/surface 变化、文档切换、进入 Play 或竞争手势都会清除候选。空白拖拽没有框选行为。不为场景点击增加第二份选择状态。

命中通过 SelectionService 单选，未命中清空；两者均不创建 World command、不标 dirty。Outliner / Inspector / Gizmo 由同一选择回调刷新。

### 4. 快照、过期结果与失败语义

请求在 host 线程创建，在可用的同一场景/相机修订快照上求交；相机和几何不可混用不同修订。相机刚移动而 snapshot 尚未同步时等待下一次 host 更新，在完成前保留当前 selection。首期是编辑几何查询，不承诺延迟呈现帧的逐像素命中。

应用结果前再次验证文档世代、场景/相机/布局修订、surface、最新请求序号和 Actor 身份；旧结果丢弃。文档切换/关闭、Play、对象删除、资产重载或更新点击不得被旧完成回调覆盖。

结果显式区分 Hit、Miss、Pending、Unsupported、Error。只有完整受支持查询的 Miss 才清空选择；Pending、Unsupported、Error 保持当前选择并提供状态反馈，不能静默当成空白。若射线范围内有未就绪/不支持的候选，不能跳过它直接断言其后的 Actor 是最近命中；首期保守返回不完整状态。离开射线范围的坏资产不阻止其它查询。异步重试只允许最新且上下文仍有效的请求。

### 5. GPU 扩展条件

只有需要 shader deformation、alpha clip 或与最终深度逐像素一致时，才在同一消费方接口后补 GPU 实现。前置条件为：受支持的 ID render target 格式和后端管线、帧内 Actor/RenderEntity generation 映射、与 scene 一致的 depth/cull/alpha 规则、异步小区域回读及过期结果守卫。RGBA8 编码若被选用必须禁用 sRGB、混合、后处理和会改变 ID 的 MSAA resolve。不得用每次点击整幅图像回读或阻塞 WaitIdle 代替这些契约。本 change 不包含该后端的任务。

## Risks / Trade-offs

- CPU 与像素不完全一致 → 明确几何拾取边界，对未支持材质/变形返回状态，不宣称视觉精确。
- 大网格冷启动或缓存内存放大 → 按几何修订共享 BVH、显式预算、可取消准备与 Pending；记录冷/热数据而不凭估算承诺时延。
- 非均匀/负缩放影响距离和剔除 → 世界距离排序，逆变换与绕序单测。
- 等待期间对象/文档变化 → 无裸指针结果，修订与序号检查，只有 host 线程更新 SelectionService。
- Gizmo、浮动窗口和 DPI 争用 → 复用 SceneViewport/router，同一内容矩形和主题阈值；回归覆盖真实 Shell 路由。

## Migration Plan

先补消费接口与纯数学用例，再添加 adaptor 快照及资源/BVH 缓存，最后接点击状态机和 SelectionService。沿现有 targets 构建；通过无 GPU 测试后在 Vulkan / DX12 的同一静态网格 fixture 上验收。没有持久化 schema 变更；撤销本 change 后仍能通过 Outliner 选择。

## Open Questions

无产品范围阻塞项。实施时必须核实当前材质/mesh payload 能暴露的可见性与剔除语义、CPU 数据所有权和修订来源，并以测试确定支持集合；拿不到的数据返回 Unsupported，不能默认视为渲染可见。macOS/Metal 在没有原生构建与运行证据时保持未验证状态。
