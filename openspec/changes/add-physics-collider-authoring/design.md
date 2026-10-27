## Current delivery boundary (2026-10-27)

运行时物理资产、显式 World cell 宿主与 benchmark 已交付并验证。下面的制作/调度设计包含尚未实现的后续目标；实际实现决策见文末，剩余范围、owner、任务与验收见 [remaining-work.md](remaining-work.md)。不把初始调研描述当作当前实现状态，也不把设计目标视为已通过验收。

## Context (initial investigation)

状态：2026-10-27 已按用户确认实施材质/Collision 资产与 Framework cell 加载闭环，具体范围及验证见 verification.md。专用制作窗口、自动拟合/分解、空间分区策略与 Terrain 分页接入仍属后续工作。运行时保留 adaptor + render，离线新增 builder，共用少量私有 codec 源码，不增加 core。

已检查的仓库接入点：

- `engine/physics/include/physics/PhysicsShapes.h`：Box、Sphere、Capsule、ConvexHull、Compound、TriangleMesh、HeightField；子形状只有 pivot，没有 rotation；TriangleMesh 支持 UUID 或 CPU 数据。
- `plugins/jolt/adaptor/include/jolt/components/PhysicsBodyComponent.h`：PhysicsBodyData 内联形状、质量、BodyKind、CCD、重力、睡眠、trigger 和 filter；目前没有 Collision 资产引用或制作窗口。
- `plugins/jolt/render/JoltPhysicsRenderModule.cpp`：Aurora 产品到 CPU TriangleMesh 的桥；调用可能同步等待产品，不适合直接放进编辑器绘制循环。
- Sandbox 的 `ReflectedFormView`、`ReflectedInspectorPanel`、`CommandService`、`EditorActionRegistry`、`DocumentService`、`TransformGizmo` 已提供通用编辑基础。
- 资产窗口优先参照 `plugins/animation/editor/module/AnimationEditorModule.cpp` 的 `IAssetViewProvider::OpenDocument` 与代码 lease；其 `GraphDocument` 是可撤销源文档、原子保存、触发 cook 的实际参考。
- `framework/asset/AssetBuilder.h` 已有 Request、依赖记录、产品发布以及反射 cook settings。新工具应接入它，不另建资产数据库或独立导出路径。
- `AssetManager` 已有异步任务、产品依赖和 `ProductsOnly` 策略；`World::LoadJson/Build` 仍是完整世界构造，不能用于每个 streaming cell 的重复装载。`TryAttachToWorld` 是同步挂接，不接受未准备好的碰撞依赖。
- 地形已有异步分页、加载/卸载半径滞回与应用预算；`TerrainCollisionLayer::Update` 根据已加载 LOD0 tile 同步创建/删除 Body，尚无独立的碰撞准备状态、激活确认或支撑面驻留保护。这里只定义后续对接契约，不顺带重写地形实现。

### 其他引擎的经验

| 参考 | 已确认的工作流 | SkyEngine 采用的部分 |
|---|---|---|
| [Unreal 简单/复杂碰撞](https://dev.epicgames.com/documentation/en-us/unreal-engine/simple-versus-complex-collision-in-unreal-engine) | primitives/convex 为 simple，trimesh 为 complex；查询可选复杂度；ComplexAsSimple 不能让自身参与动态模拟 | 清晰区分几何类型与用途；第一阶段一个模拟形状集合，不伪装已有双查询通道 |
| [Unreal Static Mesh 制作](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-collisions-with-static-meshes-in-unreal-engine) | 在资产编辑器增加多个简单碰撞体，并用变换工具调整 | 独立碰撞文档、形状列表、生成/添加/替换、视口变换、保存后跨实例复用 |
| [Unity Mesh Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/mesh-colliders-introduction.html) | convex hull 会填平凹部；保留凹部可用多个凸形状；concave 可静态或运动学使用，限制不同于本项目 | 显式展示单凸包会封住孔洞；SkyEngine 当前将 trimesh 限定为 Static |
| [Unity cooking 参数](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshCollider.html) | 清理退化三角形、焊接重合点、烘焙与运行效率权衡 | 清理报告、可控容差、预览结果、记录配方与构建 key |
| [Godot 3D 碰撞体](https://docs.godotengine.org/en/stable/tutorials/physics/collision_shapes_3d.html) | Mesh 菜单提供单凸包、多凸包、静态 trimesh；Shape 资源可复用 | 从选中模型一键生成并预览；组件实例配置和几何资源分离 |
| [Godot 导入后缀](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/node_type_customization.html) | 支持碰撞专用节点与仅碰撞节点的导入约定，可关闭约定 | 保留 DCC 专用低模碰撞网格；命名识别可选、导入映射可见且可重现 |

这些是工作流参考，不承诺不同引擎的限制、性能和参数可以直接对应。

## Goals / Non-Goals

**Goals:** 制作者能完成“选模型 → 生成/编辑 → 预览 → 保存碰撞资产 → 场景引用 → cook/package”的闭环；动态和静态用法明确；生成可取消、修改可撤销、失败不破坏已有结果；运行时无编辑器或分解库依赖。

新增目标：碰撞和材质产品可异步准备、跨实例复用并按预算驻留；分块宿主能区分“数据已加载”与“物理已可交互”，卸载不破坏跨块动态对象、约束或共享资源。

**Non-Goals:** 首期不做骨骼 ragdoll、软体、逐帧变形网格重烘焙、全关卡自动碰撞重建；不承诺 Exact 回滚；不在本设计阶段实现工具或引入新 SDK。UE 式 simple/complex 双查询选择留给后续独立接口 change。

首期不实现通用 World Partition 编辑器、自动切块器或完整游戏存档系统；物理侧先交付可由分块宿主/测试驱动的准备与激活契约。World 清单与状态增量的格式由 Framework 配套设计落实，不新增 Jolt 专属场景文件。

## Decisions

### 1. 两个制作入口，共用一份文档模型

**碰撞资产窗口**：从 Mesh 资产上下文菜单执行“创建碰撞资产”，或打开已有 Collision 资产。

```text
菜单与工具栏：保存 | 撤销/重做 | 添加形状 | 从网格生成 | 预览/暂停/单步 | 显示
┌────────────────┬──────────────────────────┬─────────────────────┐
│ 形状列表        │ 3D 预览                   │ 反射属性面板         │
│ Box / Hull ... │ 模型 + 碰撞线框 + 选中项    │ 当前形状/生成参数     │
│ 来源/手工标识   │ 变换或尺寸操作柄            │ 预算与验证信息       │
├────────────────┴──────────────────────────┴─────────────────────┤
│ 构建状态、错误定位、形状/顶点/三角形数量、包围盒与预计产品大小     │
└────────────────────────────────────────────────────────────────┘
```

**场景 Inspector**：PhysicsBody 选择 Inline 或 Asset，配置 Static/Dynamic/Kinematic、质量、材质、filter、trigger、CCD 等；“编辑资产”打开上面的窗口，“生成独立副本”创建新的资产。修改共享资产先显示受影响实例数，场景中不偷偷改写共享数据。

首期保留一个 PhysicsBodyComponent，以免迁移现有场景。Collision 资产承载几何；身体质量、运动学类别和碰撞过滤留在实例。以后确有独立 collider 生命周期需求再拆组件。

### 2. 几何选择和生成算法

| 生成项 | 输入与算法 | 典型用途 | 限制/验收 |
|---|---|---|---|
| Box | 选定子网格顶点变换到资产局部坐标，AABB 中心和 half extents；可选 PCA 定向盒 | 箱子、墙、门、平台 | PCA 是近似 OBB，不声称最小体积；对近相等特征值使用稳定轴退化策略 |
| Sphere | 包围盒中心 + 最远点半径作为保守初值，可再用 Ritter 扩张拟合 | 球状物、触发器 | 最后验证全部点在球内，误差外扩；不声称最小包围球 |
| Capsule | 用户选轴或 PCA 主轴，求轴向范围及最大径向距离；用线段端点和半径包围所有输入点 | 柱状物、角色近似 | 数据 height 是圆柱段长度，总高为 height + 2r；退化为球，明确预览 |
| Single Convex | 清理、去重、有效点集 → Jolt ConvexHullShapeSettings 运行时凸包；产品保存规范点集 | 石块、简单动态道具 | 不能保留凹槽；退化、过多点和 SDK 限制显式报错，禁止无提示丢点 |
| Compound | 手工组合 primitive/hull，或多凸包分解输出 | 椅子、车辆外壳、带孔动态道具 | 形状数量是预算；首期整 body 一个 filter/material，不伪造子形状独立响应 |
| Static Triangle Mesh | 专用碰撞网格或选定 render LOD → 清理顶点/索引 → 分区产品 → Jolt MeshShape | 建筑、地形外静态环境 | 仅 Static；是三角表面，不是封闭触发体；拒绝 Dynamic/Kinematic 和 trimesh trigger |

所有生成先校验有限数、索引范围、单位/轴、变换、最小尺寸和输入预算。负行列式导入变换要烘入顶点并修正绕序；运行时负缩放与剪切仍明确拒绝。Sphere/Capsule 编辑器默认锁均匀缩放；非均匀需求通过显式“转换为凸包”处理，避免形状与 UI 半径含义不一致。

首期自动拟合生成保守外包形状。需要内缩、薄壁、通道保留时由用户编辑或提供专用网格，不能用无提示误差掩盖穿透或封洞。

### 3. 多凸包分解选型

**建议将 CoACD 作为第二阶段的离线候选。** [V-HACD 官方 README](https://github.com/kmammou/v-hacd) 已声明停止维护并推荐 CoACD；[CoACD 官方仓库](https://github.com/SarahWeiii/CoACD) 提供 C++ 接口、MIT 许可和碰撞感知误差控制。此处是基于维护状态的工程选型建议，还没有本仓库实测质量或耗时结论。

- 第一阶段先交付基础形状、单凸包、手工 compound、静态 mesh，不需要分解库。
- 第二阶段固定 CoACD revision，审核完整依赖及显式失败接口。检查第三方是否实际使用 throw/异常回调；如触发仓库异常规则，列明调用链与改造方案后再实施，不加入 try/catch 兜底。
- 反射设置包括误差容差、单位模式、分解精度、最大 hull 数、单 hull 顶点预算、预处理与随机种子；默认“快速预览”和“最终构建”分开显示，产品记录最终参数。
- hull 数达到预算后如果仍不满足误差，返回“质量目标未满足”，让用户选择改容差或预算；不能把截断结果标成成功。
- 质量样本至少含椅子、U 形件、环、薄壁容器、扫描噪声模型。测生成耗时/峰值内存、输出 hull 数、误差与通道可通过性。
- 分解使用可终止的离线任务；若固定版库无可靠取消，采用受管子进程。退出/超时必须收齐任务、清理临时产品并发出一次终态通知。

### 4. Collision 源资产与产品

新增 plugin 私有 schema `CollisionSource`（建议扩展名 `.collision`），包含：

- schemaVersion、sourceMesh UUID、LOD/submesh 选择、导入单位/坐标映射与内容 hash。
- recipe 列表：生成方式、参数、来源节点稳定 ID；手工元素有独立 UUID，重排不改变身份。
- 元素列表：id、kind、local translation/rotation、尺寸/点集或网格来源；区分 generated 与 manual，并记录被修改的生成元素。
- cook 配置与预算：焊接容差、退化阈值、目标产品 bundle、最大数量、分解版本。
- 不保存 PhysicsObjectId、JPH 指针或运行时材质 handle。

产品 `CollisionProduct` v1 保存经校验的 primitive、hull 点集、triangle 顶点/索引、局部变换、bounds、来源 fingerprint、构建版本及统计；产品独立于渲染 mesh 的 CPU 保留策略，shipping 不依赖源模型文件。

选择**规范几何产品 + adaptor 内 Jolt shape cache**作为 v1，避免一开始就把 Jolt 原生二进制 ABI 绑进资产格式。构建时完成高成本的分解和清理；运行时首次装载仍有 Jolt hull/BVH 构建成本，进入关卡前预热，禁止在 UI draw 或物理 Step 回调里同步构建。若测量表明不够快，后续独立设计 native cooked product，完整 key 必须包含 SDK revision、平台、精度、SIMD、产品版本，失配时重新 cook 或显式失败。

shape cache key 包含产品 hash、局部几何变换、有效缩放、collision margin 和其他影响形状的选项，并隔离 SDK/build 配置；不包含 body 位置、姿态、质量、过滤或 body 级材质。缓存对象随 SDK/code lease 存活，不让形状比 Factory/注册类型活得更久。准备、驻留与淘汰见第 9 节。

源 recipe 是编辑依据；预览使用同一生成服务的内存结果；正式运行通过产品加载器读取。保存源文件成功但 cook 失败时，显示“源已保存/产品过期”，保留上一个好产品；发布打包遇到过期产品必须失败，不能无提示带旧数据。

### 5. 接口增量与兼容

- 给 `ShapeDesc` 增加默认 identity 的局部 rotation，pivot 继续表示局部 translation。定义顺序：primitive 局部尺寸 → 子形状旋转/偏移 → 父 compound → body transform；缩放组合出现剪切时拒绝或在离线顶点中烘入。
- 旧 JSON 无 rotation 时取 identity；ShapeType 数值不变；增加嵌套旋转、重心和射线命中回归测试。
- PhysicsBodyData 新增显式 sourceMode 与 collision UUID；缺失字段默认 Inline。Asset 模式引用缺失必须报错，不能悄悄回退到旧 inline box。
- `engine/physics` 定义消费方的 `ICollisionGeometryProvider`（拟名）及只读几何/状态结果；Jolt plugin 的资产实现提供它。schema、序列化和 builder 实现在 plugin；现有 render mesh provider 作为旧 UUID mesh 的兼容路径。
- 制作参数中的碰撞材质引用在运行时解析为 world 内 handle；第一阶段使用 body 级材质，子网格材质映射与 triangle face 命中 ID 是后续接口增量，不能只做 UI 字段。
- 保存或拖动期间不直接销毁 live body。预先验证并构建候选，成功后在 world 安全边界替换；失败保留旧 body 与错误信息。需要引用约束的物体重建时保持句柄或显式重绑/恢复，不允许已有约束静默丢失。

### 6. 模块与元数据归属

此表是后续实施的 target 方案，不是本次迁移新增目标清单。

| 位置 / 拟 target | 职责 | 依赖与装载 |
|---|---|---|
| `engine/physics` / Physics | 消费接口、形状及验证数据 | Framework/Core 与 engine 接口；不引用 plugin |
| `plugins/jolt/adaptor` / JoltPhysicsModule | SDK、world、运行时 Collision 产品加载、shape cache、PhysicsBody 绑定 | Physics、Framework、私有 Jolt；game/editor 的物理 runtime |
| `plugins/jolt/render` / JoltPhysicsRenderModule | Aurora CPU 几何提供；旧 mesh UUID 兼容 | Physics、Aurora adaptor；不公开 Jolt 类型 |
| `plugins/jolt/editor` / JoltPhysics.Editor（拟） | CollisionDocument、view provider、反射表单、命令、视口与预览 | EditorCore/EditorUi/EditorWidgets、Physics、Aurora 接口；只由 editor 加载 |
| `plugins/jolt/builder` / JoltPhysics.Builder（拟） | 配方解析、拟合、清理、分解、产品写出 | Core、Physics 数据接口、必要的 Framework 资产构建接口、工具侧 CoACD；不链接 adaptor/render/editor |

不建立 `core` 或 `JoltPhysics.Static` 来转发 SDK。确实需共享的**纯 schema 与无注册的文件 codec**放 `plugins/jolt/shared`，作为 plugin 私有构建源供 builder/adaptor 使用，不新建 runtime 模块。builder 写规范字节产品，可复用 AssetBuildResult/产品发布 API；若当前管线只能借助 runtime typed loader 才能发产品，应先增加 Framework 的通用字节产品发布能力，不能让 builder 反向加载 adaptor。

元数据 owner 明确分开：JoltPhysicsModule 负责 PhysicsBody、world config 和运行时接口数据；JoltPhysics.Builder definition 负责生成设置/源文档 schema，离线 AssetTool 与 editor 都加载同一个 definition；JoltPhysics.Editor 仅注册窗口视图类型并 Bind 数据。runtime CollisionProduct 使用版本化 codec，不注册同一份 source schema。shared codec 不执行 RegisterMeta，测试 main 选择同一 owner 初始化一次。

builder 允许在编辑器进程中提供轻量拟合服务；高成本分解委托 worker。物理制作服务契约定义在消费方物理领域的 `engine/physics`，签名只引用 CPU 几何、生成参数和结果数据，具体实现由 builder 注册；builder 不依赖 Sandbox/editor target。文档、生成任务、回调和 native shape 持有相应模块 lease，Shutdown 取消任务、等待结束、撤销注册；不能靠 static bool 避免重复反射。

### 7. 编辑器实现约束

- 通过 `AssetViewProviderRegistry` 打开独立 CollisionDocument 窗口，使用 DocumentService 生命周期；源文档编辑借鉴 GraphDocument 的存储抽象，渲染产品仍经正常 asset loader。
- 所有配置对象（shape、fit、cook、预览 world、PhysicsBody）反射注册 LABEL/ORDER/CATEGORY/RANGE/ASSET_TYPE 等；用 ReflectedFormView / ReflectedInspectorPanel。变体可由通用可见性元数据或子对象选择呈现，不手写逐字段 switch 表单。
- 所有工具栏、菜单、快捷键来自 EditorActionRegistry；actions 包括 AddBox/AddSphere/AddCapsule、FitSelection、SingleHull、Decompose、StaticMesh、Delete/Duplicate、ApplyGeneration、Save、Undo/Redo、Preview/Pause/Step、ShowCollision。工具栏图标、尺寸、颜色遵循 UiSkin/UiTheme。
- 形状树是选择模型；平移/旋转复用 TransformGizmo；Box 半尺寸、Sphere 半径、Capsule 半径/圆柱高用专用几何操作柄。操作柄可以自定义视口几何，不代替通用属性表单。
- 一次拖动只产生一条 undo command；开始保留 before，拖动只改预览，结束校验后提交，Esc 恢复；ApplyGeneration 是可撤销的整组替换命令。生成结果不能绕过 CommandService 写数据。
- 每次生成带 document revision；旧结果、被关闭文档结果或取消结果不写回。后台线程只产生不可变结果，不访问 UI；错误按元素/来源定位，主线程接收一次完成事件。
- 预览使用独立 IPhysicsWorld，通过 registry 创建，有地面、落体/投射测试、重置/暂停/单步。开始预览时冻结文档 snapshot，离开或 reset 销毁 preview world；绝不推进当前编辑世界。
- 调试绘制消费 PhysicsDebugGeometry；显示 simple/triangle、AABB、COM、接触点、法线、约束及睡眠状态。当前数据不够的 COM/睡眠标记通过明确接口补齐，不 include JPH。

### 8. 导入、烘焙与错误诊断

流程：导入模型 → 选择碰撞来源/配方 → 内存生成预览 → 用户 Apply → 保存 `.collision` → AssetCookService → builder 解析依赖与几何 → 清理/拟合/分解 → 验证预算 → 原子发布产品 → runtime 预热 shape。

优先支持独立 collision mesh UUID 和模型命名节点的显式映射；可选识别 `UCX_<mesh>_<n>`、`UBX_...`、`USP_...`、`UCP_...` 风格标签以兼容常见 DCC 流程，但本项目规则必须文档化。识别后验证 primitive 是否真的符合盒/球/胶囊；不满足时报告，不能单凭名字假装有效。默认不把 helper 作为 render mesh 导入。

重导入保持稳定来源节点 ID；只替换未手改的 generated 元素；已手改项显示冲突并提供保留/重新生成，manual 元素不丢失。source UUID、选定 LOD/submesh hash、recipe、单位/坐标转换、清理/分解版本、目标 bundle 共同参与增量构建 key。

Mesh 清理报告焊接数、退化/重复面数、边界/非流形边、绕序和薄片风险。静态 mesh 允许开口，但说明单面碰撞及背面策略；只有明确选择修复时才改几何，禁止悄悄封洞。大型场景按空间块拆产品以控制 broadphase、加载与内存；首期报告建议，不自动重写场景层级。

预算默认值通过样本测量确定，所有硬限制与建议阈值分开显示。Jolt shape 构建失败返回结构化错误；不以断言、进程退出、异常捕获或空碰撞体替代失败。

### 9. 物理资产、场景持久化与 streaming

#### 9.1 官方设计参考与采用边界

本节资料核对于 2026-10-27。以下事实来自官方文档；右列是 SkyEngine 的设计选择，不推定其他引擎采用相同内部实现。

| 参考 | 已确认的机制 | 本项目采用的契约 |
|---|---|---|
| [Unreal UBodySetup](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/PhysicsEngine/UBodySetup?application_version=5.5)、[异步物理网格创建](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UBodySetup/CreatePhysicsMeshesAsync) | 多个 BodyInstance 共享碰撞数据；异步准备完成后仍需显式创建物理实例 | 不可变几何缓存与 world 内 Body 分开；准备完成不等于激活 |
| [Unreal World Partition 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine?application_version=5.6) | streaming source 驱动 cells；Loaded/Activated 分开；可预加载传送目的地；Actor 引用会影响共同加载 | World 宿主决定需求与依赖，physics 返回明确就绪状态；物理需求不能只取相机可见范围 |
| [Unity Addressables 场景加载](https://docs.unity3d.com/Packages/com.unity.addressables@2.3/manual/LoadingScenes.html)、[资源生命周期](https://docs.unity3d.com/Packages/com.unity.addressables@2.3/manual/MemoryManagement.html) | 加载可与激活分开；加载/释放对应引用计数；资源与 bundle 的释放时机不同 | 独立请求与实例引用；卸载 cell 不等于立即回收共享产品。Unity 延后激活可能堵住异步队列，本项目不复制这一队列耦合 |
| [Unity Physics.BakeMesh](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Physics.BakeMesh.html) | 可提前准备并复用碰撞网格；复用依赖几何、变换和 cooking 配置；同一 mesh 不应被并发重复 bake | 同 key 合并在途准备；完整几何 key；昂贵准备与主线程实例提交分开 |
| [Godot Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)、[Shape3D](https://docs.godotengine.org/en/stable/classes/class_shape3d.html)、[PhysicsMaterial](https://docs.godotengine.org/en/stable/classes/class_physicsmaterial.html) | Shape 和物理材质是可保存、引用计数管理的资源；路径缓存复用同一资源 | 材质与碰撞几何有稳定资产身份，实例仅持引用；共享产品不可原地改写 |
| [Godot 后台加载](https://docs.godotengine.org/en/stable/tutorials/io/background_loading.html)、[线程边界](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html) | 提交请求、检查状态、取结果分开；过早取结果仍会阻塞；活动场景操作有线程约束 | 当前 `BlockUntilLoaded` 路径不能作为 streaming 实现；后台只准备数据，活动 World 修改在安全点进行 |

#### 9.2 数据与所有权

| 对象 | 持久数据与职责 | 运行时所有权 |
|---|---|---|
| PhysicsMaterial 资产 | 版本、单一 friction、restitution、两种 combine；插件 schema，经已有资产管线发布 | 产品值可共享；每个 world 将资产 UUID + 产品版本解析为自己的 material handle，并统计 Body 使用 |
| Collision 资产/产品 | recipe、稳定元素身份、可独立装载的几何和版本/hash | 兼容 SDK session 内共享不可变 prepared shape；不保存 Body 或 material handle |
| PhysicsBody 实例 | 稳定 Actor/组件身份、inline 或 Collision UUID、材质 UUID、质量、运动类型、过滤、CCD、阻尼 | Body、约束、接触及材质句柄只属于一个 physics world |
| World/cell 描述 | 全局配置、cell 身份和 bounds、actor 数据、直接资产引用、必要驻留依赖 | Framework 宿主管理请求、实例归属、激活和退役；Jolt 不拥有世界分区调度 |

首期材质资产使用单一摩擦值，映射到现有接口相等的 static/dynamic friction；不引入当前后端不支持的独立静动摩擦。阻尼作为 Body 实例参数，不再为新的资产格式引入“零等于继承”的歧义；既有低层 API 兼容语义不在此阶段暗改。空材质 UUID 明确表示默认材质，非空但缺失/不兼容的资产必须阻止该实例激活。第一阶段不在 Collision 几何资产中隐式绑定材质，也不提供子面材质槽。

资产值与 schema 的 metadata owner 沿用第 6 节：source/settings 由 builder definition 注册，runtime 产品使用版本化 codec，editor 只 Bind。组件收集 Collision 和材质直接引用；产品记录其真实运行时依赖，来源渲染 mesh 仅作构建依赖，不能因一个碰撞体而加载渲染/纹理依赖闭包。

#### 9.3 准备、激活与取消

```text
Unloaded -> Preparing -> Ready -> Active -> Retiring -> Unloaded
               |            |
          Failed/Cancelled  +-> Cancelled（尚未进入 World）
```

- **Preparing**：复用 AssetManager/AssetExecutor，以 `ProductsOnly` 读取 Collision/材质产品、解析和校验，在受限 worker 队列准备独立 shape。Jolt worker 创建 shape 的并发范围须以固定 SDK 的契约和测试验证；任务不访问 Actor、UI 或活动 physics world。禁止从运行时 streaming 触发源模型 cook、凸分解或 `BlockUntilLoaded`。
- **Ready**：产品依赖及 native shape 均就绪，持有可释放的准备结果；不在 broadphase 中，也不能被物理查询命中。不能通过把 Body 设为 sleeping 伪装成未激活，因为 sleeping Body 仍参与碰撞。
- **Active**：宿主在前一轮模拟/jobs 和事件分发完成、下一次 Step 与对外查询开始前提交。预检查容量并准备未加入世界的 Body；以有界激活组一次发布。失败销毁仅本次候选，保留旧区域及其他已激活组，不允许查询观察半组对象。超预算组在 cook/宿主阶段拆小，不能承诺任意大小 cell 在一帧内原子装入。
- **Retiring**：在相同安全点先处理约束/持久对象归属，再移除 Body；接触结束通知按既有事件顺序交付，只保留稳定身份和必要事件数据，不引用已销毁 Actor。最后释放 world 材质引用、shape 引用和资产请求。
- 每次请求携带 world session generation、owner/cell generation 和产品版本；取消、卸载或新版本到达后，旧回调只清理结果，不再插入 Body。共享准备任务按消费者计数，取消一个 cell 不取消其他消费者。Shutdown 禁止新提交，收齐任务/退役队列后才释放 SDK/code lease。

接口只表达非阻塞 Prepare、状态/错误、Commit、Retire 与结果持有关系，名称在实现审阅时确定；复用现有 IPhysicsSystem 和资产任务，避免为每个状态新建 registry/manager。组件保持同步 attach，但 attach 前由宿主提供 Ready 数据；未就绪是可查询的 Pending，不能被当前 bool attach 误判为资产永久损坏。`World::Build` 仍只构造一次全局子系统，streaming 通过增量实例批次进入现有 world。

#### 9.4 驻留、预算与空间语义

一个游戏 World 的所有活动 cell 使用同一 physics world，以保留跨块接触和约束；独立 physics world 仅用于隔离预览或明确要求隔离的模拟。cell 是装载单位，不是求解边界。

宿主根据玩家、车辆、投射物等需求、预测运动范围、传送目的地和必要支撑区域提出碰撞需求，配置预取范围与更大的释放范围形成滞回。碰撞不能跟随 render LOD、遮挡或相机可见性直接卸载。区域 readiness 与 raycast 是否命中是两个概念：尚未装入不等于没有地面。传送/生成只在必需区域 Active 后完成；慢加载或失败返回 Pending/Failed，由宿主暂缓进入、使用已声明的保守代理或保留旧区域，不默认允许对象进入碰撞缺口。

地形继续使用现有分页来源和最高分辨率碰撞契约；后续接入须让物理需求保留所需数据/碰撞产品，并增加准备/激活确认。LOD0 render tile 的退出不能直接成为销毁仍被角色支撑面使用的 collider 的依据。这需要独立审阅 terrain 接入 delta，不以修改本文覆盖现有实现。

动态刚体移出出生 cell 后由 World 持久实例集合接管；首期在仍参与模拟时保持驻留，源 cell 卸载不得删除它，也不得重载时重复生成。sleep 本身不是可销毁证明。约束两端必须同时有效；跨 cell 硬依赖形成有上限的共同驻留组或显式 pin，卸载被 pin 拒绝/延期；不能因一端暂未加载将它当 world anchor。首期不支持的依赖环/超预算组显式报错，避免引用链隐式加载整个地图。

缓存同时统计在途 scratch、Ready 结果、活动 shape、CPU 产品及暂留空闲条目的字节预算；native 准备并发数、每帧实例增删数量/时间分别限额。活动引用和在途任务禁止淘汰；无消费者的条目可按预算和 LRU/短暂滞留回收，避免边界反复进出抖动。AssetManager 与 shape cache 的驻留分别记账，需验证其实际强引用策略，不能假定释放一个 Body 即释放整个 bundle。容量不足显式背压，不能靠无界 pending 队列规避。

热更新发布新的不可变产品版本，所有新依赖就绪后才在安全点替换；同一原子激活组不混用依赖版本。旧产品在最后一个实例/任务释放后退休，不原地修改其他 cell 仍在使用的 shape。实例替换保留速度、睡眠及约束契约，失败继续使用旧版本。

#### 9.5 场景、分块与状态保存

持久化分成三类，均不写 native 指针、BodyID 或 world 内材质 handle：

1. **可编辑场景/分块描述**：全局 manifest 保存 World 配置、版本、稳定 cell ID、bounds、cell 产品定位和必要加载依赖；cell 保存稳定 Actor/组件身份、Transform、Body 参数与资产 UUID。可复用子场景还需要宿主实例命名空间以避免 UUID 冲突。小场景可视为一个常驻 cell，旧 `.world` 继续走完整载入路径。
2. **资产产品**：独立版本化的 Collision/PhysicsMaterial 数据。cell 的运行时加载只解析该 cell 必需的依赖；manifest 列出全部 cell 供发现/打包，不把全部 cell 挂成启动时递归加载的强依赖。打包离线遍历所有 cell 的依赖闭包，包含从未在编辑器打开过的 cell。
3. **可选运行时状态增量**：使用稳定 World/Actor/组件身份保存移动后位姿、速度、sleep、迁移归属、生成及删除记录。恢复时先载入描述和资产，再绑定新运行时 handle 并恢复状态；现有 PhysicsWorldState 只能恢复已存在句柄，不能直接写成跨会话存档。完整状态存档属于宿主后续能力，首期动态对象先保持驻留。

编辑器保存的是完整文档索引与已修改 cell，不能枚举当前 resident actors 后覆盖整个场景，从而丢掉未加载 cell。分块产品与 manifest 采用现有产品发布/版本快照能力保持一致；跨 cell 约束按稳定身份在两端激活后解析。碰撞包按空间区域与共享几何分组，避免把整个世界烘成单个不可独立加载的 TriangleMesh；大资产可分成有 bounds 的独立子产品，不要求运行时解析全部几何才能定位一块。

#### 9.6 实施顺序与验证边界

先在现有 adaptor 和资产管线实现材质引用、可释放的异步准备结果、共享 shape 缓存和安全点批次接口，再由测试宿主驱动 cell 装入/卸载。Framework 分块清单/调度与 terrain 适配单独审阅后接入；不把编辑器窗口中的异步生成测试当作 runtime streaming 验收。该顺序不新增 core，也不要求每个 cell 有单独的 physics world。

必须覆盖：两个 cell 共用 shape/材质后卸载一个；同 key 在途请求合并与单消费者取消；请求完成前 world 销毁；卸载后立刻重载的旧回调；准备成功但提交容量不足；传送到未就绪区域；跨 cell 动态物体和约束；地形支撑面的驻留；未加载 cell 保存/打包；新旧产品版本并存及失败替换；长时间往返后的 native/CPU/pending 内存上界。记录 I/O、native 准备和主线程提交耗时，测量决定是否需要 SDK/platform 专属预烘焙产品。

## Risks / Trade-offs

- 规范几何产品仍有初次 native cook 成本 → 缓存、预热并记录真实耗时；需要时另做 ABI 明确的 native 产品。
- 非均匀缩放、嵌套旋转与重心容易错位 → 统一坐标契约、拒绝剪切、测试偏心 compound 和 mesh picking。
- CoACD 误差控制可能耗时/内存高或带异常路径 → 先做固定版离线 spike、可终止 worker 与预算，不增加 runtime 依赖。
- 共享资产改动影响大量实例 → 明确共享范围、文档命令、独立副本入口、cook 失败保留好产品。
- 现有 Jolt UpdateBody 已保持句柄和约束，但还没有资产版本发布和批次激活 → 将单体保证延伸到资产替换/分块提交，验收包含失败回滚与约束存活。
- 异步 I/O 完成但 native shape 未就绪或提交超预算 → 独立 Ready/Active、预取与背压，不能把资源加载百分比当成碰撞可用性。
- 分块卸载删掉移动物体/支撑地面或保存丢掉未加载 cell → 宿主稳定身份与驻留依赖、完整文档清单，配套改动独立 review。
- UI 底座未必已有全部扩展点 → 只给 Sandbox 增加通用接口；plugin 实现功能，不把 Jolt 类型或字段硬编码到 shell。

## Migration Plan

1. 审阅此设计及 editor/builder target/装载入口；与 Bullet 替换独立 review，保持旧场景可读。
2. M1：局部旋转/资产引用兼容、Collision/PhysicsMaterial schema/codec、基础 builder、Box/Sphere/Capsule/单 Hull/静态 Mesh、文档/反射编辑/Undo/预览；同时完成物理侧异步准备、缓存与安全点激活契约，无 CoACD。Framework 分块宿主与 terrain 接入另行审阅，完成前不宣称通用 World streaming 已交付。
3. M2：DCC helper 映射、重导入冲突与批量 cook、固定版 CoACD、多凸包质量/预算/取消；只工具依赖。
4. M3：运行时预热性能、平台产品验证；有测量依据再做 native cook、双查询几何和每面材质等独立扩展。
5. 回退通过保留旧 inline 数据与上个成功产品；新资产不自动写回旧格式。切换 sourceMode 是显式命令，未知 schemaVersion 拒绝加载并给出版本错误。

### 验收矩阵

- 基础几何：旋转盒、偏心组合、退化球/胶囊、全部输入点的包围验证、mm/m 单位转换与负行列式绕序。
- 网格：U16/U32、stride、选定子网格/LOD、坏索引、NaN、退化/重复面、开口与薄壁、动态/运动学 mesh 拒绝。
- 交互：资产多实例共享、独立副本、拖动撤销/Esc、生成撤销、dirty/save/失败状态、源变更后旧生成结果丢弃。
- cook：依赖 hash 变化、重导入 manual 保留、输出版本、过期产品阻止打包、取消/进程失败后一次终态与资源清理。
- 物理：形状替换失败旧 body/约束保留、质心/惯性更新、预览不改场景、模块重启与存活任务/文档 lease。
- streaming：第 9.6 节的生命周期、预算、空间依赖及保存/打包用例；只准备好而未激活的形状不进入查询和接触。
- 性能：记录样本顶点/三角形数、生成耗时、内存峰值、hull 数、native cook 时间和查询/接触成本；测量前不填虚构指标。

## Open Questions

- CoACD 最终固定 revision、取消/异常行为及依赖编译选项由 M2 spike 决定；当前未接入。
- 基础预算与清理容差预设由实际项目单位及样本确认，首期必须显示有效值。
- 双查询通道与原生 cooked product 是否需要，取决于 M3 的场景与性能证据，不阻塞 M1。

## 2026-10-27 implementation decisions

- `.physmat` 和 `.collision` 使用明确的 version 1 codec。Collision 产品是独立、规范化的 CPU 几何，不引用渲染产品；每个元素有稳定 UUID。原生 Jolt Shape 在 AssetExecutor 工作线程构建，因此产品不绑定 Jolt ABI。原生 cooked blob 的取舍仍属 M3。
- `JoltPhysics.Builder` 模块链接既有 builder 目录的 `JoltPhysics.Builder.Static`；它依赖 Physics/Framework 与共享的 `JoltPhysics.AssetCodec.Static`。adaptor 和测试链接同一个 codec target，源码只有一个编译 owner；没有新增 core。Runtime 注册产品 handler，builder 注册 builder；没有重复反射入口。
- Framework 增加 `SaveProductData`，复用既有压缩、产品 envelope、校验、原子 group 发布及索引逻辑；builder 无须加载运行时 handler。新增 `LoadAssetAsync` 将产品查找、读取、校验也调度到已有 AssetExecutor；旧 `LoadAsset` 并非完全异步，不能直接放在 streaming Tick 中调用。
- 组件保留 Inline 模式，增加 Collision UUID、PhysicsMaterial UUID、Body 阻尼和反射资产字段。空材质是默认值；缺失产品、错误类型、版本、几何或不支持的动态凹面阻止激活。旧的 render-mesh Inline 路径仅兼容普通加载；streaming 要求先制作为独立 Collision 产品。
- Shape 请求按不可变产品对象身份（固定产品 generation）、scale、collision margin 和 SDK session 合并。消费者取消只释放自己的请求；后台任务不捕获 World/Actor/组件，持有输入与 SDK/code lease。世界销毁或新编辑不会接收旧结果。
- `WorldStreaming` 是 Framework 的显式请求宿主，不是分区算法。World Tick 前执行准备和提交；外部空间/游戏策略使用 Request(Unloaded/Ready/Active)。Ready 不创建 Body，不参与查询。整组预检、分配完成后再加入 broadphase；失败只回滚该组。退役先删除整组物理实例，再发布 Actor 退出通知。
- World JSON 增加 `cells: [{uuid, initialTarget, actors}]`，目标值为 0/1/2。initialTarget 是作者配置，不是保存瞬时运行状态。全部未加载 cell 描述和资产根引用参与保存/打包审计。旧 actors/subSystems 文件继续可用。动态/运动学对象与其 transform 树提升为 World 驻留，cell 重载不复制它们；跨 cell 约束及现有支撑接触阻止退役。
- 初始硬上限：每组 128 Actor、清单 4096 cells、每帧处理 4 个准备 cell、每 cell 每帧克隆最多 8 Actor、每帧最多 2 组提交；SDK session 最多 32 个在途 shape 工作、1024 个形状资源、256 MiB 估算输入几何。单 shape 输入上限 64 MiB、4096 元素，codec 最多 4M 数值。资源统计明确是 source bytes，不代表完整 Jolt/native 或进程内存峰值。
- 初始目标、Ready 状态及显式 Request 允许游戏宿主实现传送门槛、预加载与卸载滞回；本次没有实现相机/玩家驱动的空间调度、cell bounds 自动划分或 Terrain 插件的驻留接线。接触 pin 是现有接触的保守保护，不替代预测碰撞覆盖策略。
