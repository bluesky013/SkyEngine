## Context

状态：2026-10-24 实施中；用户在整体严格 review 后以 `apply` 确认 Godot 式组合、D1 新模块与首版实施范围。代码证据、严格复核结果及关联 change 见 [review.md](review.md)。

`Animation` 已是仅依赖 Core/可选 ACL 的独立算法库；`Content.Adaptor` 拥有 Clip 和空 Graph 资产；Aurora Adaptor 拥有 Skeleton 资产与静态场景提取。`World::Tick` 先 Actor 再子系统，Actor 组件与 World 子系统的容器顺序不能充当动画/物理依赖调度。Sandbox 有文档、反射、命令与统一 renderer，但资产预览只是占位。

上一轮已运行现有 Debug 测试程序：AnimationTest 54、Content.AdaptorTest 1、MeshCook 筛选 9 项通过；程序生成于 2026-10-21，未重新编译，不构成本设计的集成验收。

2026-10-26 提交拆分：通用 Core/Framework 产品事务、不可变快照、generated catalog 和子产品 cook 完成协议由 `refactor-asset-product-transactions` 独立承接。本 change 保留 Mesh/Skin/Skeleton、Clip/Graph、选择器与打包的集成要求；基础设施测试不替代这些验收。metadata code lease 仍归 `refactor-process-owned-reflection`。

运行时算法前置修正由 `fix-animation-sampling-validation` 独立承接：骨架/Clip 校验、reference pose 采样与虚图/Plan 播放控制。本 change 仍负责资产链路保留 sourceFrameCount、导入校验、实例隔离和编辑/渲染接入。

## Goals / Non-Goals

**Goals:**

- 单 Actor 使用骨骼网格与单 Clip/Graph；共享 mesh 的多个 Actor 独立播放、暂停、seek，并可保存重开。
- Mesh 与 Skin 独立；同一 Mesh 可由不同实例选择不同 Skin / Skeleton，同一兼容 Skin 也可被多个实例共享。
- 姿势经明确的 CPU 完成边界进入同一 renderer packet，颜色、阴影、bounds 使用同一版本。
- Graph 有可编辑源文档、有效 cooked 定义、错误定位、运行时实例和编辑预览闭环。
- 模块依赖、注册所有权、错误与卸载行为可 review、可测试。

**Non-Goals:**

- 本 change 不实现 FBX/glTF 动画 take 导出；验收使用受版本控制的可用 `.clip` 源与带 skin 的模型 fixture。模型 mesh/rig 导入的正确性属于本 change。
- 不实现 GPU 动画求值/compute skinning、morph、动画 LOD/人群、ACL 的资产链路、BlendSpace、嵌套状态机、IK、重定向、挂点、Motion Matching 或网络复制。
- 单个绘制实例只绑定一套已解析的 Skin / 骨架姿势；不做两套独立骨架同时驱动一个实例，也不实现不同绑定的权重流覆盖。不同权重须使用不同 Mesh 产品。
- 不将现有 root 相对参考姿势输出当作角色位移；物理 Root Motion、Notify 与音频联动不在首版。
- 不搬迁现有 Animation/Content 目录，不以功能 change 消化整个历史分层债务。

## Decisions

### D1. 模块和装载方案（2026-10-24 已确认）

| 归属 | target / 入口方案 | 依赖和职责 |
|---|---|---|
| `engine/animation` | 复用 `Animation` | 修复既有采样/播放/校验；新增消费方 `IAnimationClipSource`、`IAnimationSkeletonSource`，以及 Animation.Editor 消费的 `IAnimationInstanceControl` 和签名数据，不新放 World/Graph 编辑实现 |
| `plugins/animation/core` | 新 `Animation.Graph.Static` | Core + 既有 Animation；纯 Graph 定义、结构校验、控制逻辑、实例构建；无 renderer/editor/asset manager |
| `plugins/animation/core` 的资产适配单元 | 新 `Animation.GraphAssets.Static` | Graph core + Framework 的资产/归档契约；Graph schema、序列化、反射与 handler 注册；不依赖 World/adaptor/editor/builder |
| `plugins/animation/adaptor` | 新 `Animation.World.Static` + `Animation.World` 装载入口 | Graph core/assets + Framework + Aurora.Renderer.Interface；组件、World 系统、资产请求与 Pose provider；入口同属 adaptor，不增加依赖 adaptor 的核心 runtime target |
| `plugins/animation/builder` | 新 `Animation.Builder.Static` + `Animation.Builder` 装载入口 | Graph core/assets + Framework AssetBuilder；仅负责 `.graph`，不得链接 Animation.World、Content.Adaptor 或 Aurora.Adaptor |
| `plugins/animation/editor` | 新 `Animation.Editor` 装载入口 | Graph core/assets + Sandbox EditorCore/EditorUi/EditorWidgets/EditorRender；通过 World/组件注册与接口驱动预览，不 include World 实现头 |
| `engine/aurora` | 现有 Core/Pipeline/Adaptor/Render/Render.Editor | 保有 Skin、蒙皮执行、SkeletalMeshComponent 和场景桥；不 link/include 动画 plugin |
| `engine/framework` | 现有 Framework / AssetTool 路径 | 扩展 generated subproduct catalog、显式保存结果和产品组发布 / revision 读取；不增加动画语义或另一套 loader |
| `engine/content` | 原 targets | 保持 Clip 资产/构建/provider；在原文件删除空 Graph 的注册、builder 分支和只读摘要，不移动目录 |
| `engine/sandbox` | 原 targets | 通用 GraphCanvas、预览宿主、文档/actions 接入；不含动画节点语义或 plugin 具体类型 |

`Animation.GraphAssets.Static` 是 feature 内共享的资产层，允许使用 Framework 归档接口；它不是运行时 adaptor，不引用 World、renderer 或 editor。物理路径仍在 plugin/core，schema 不泄漏到 engine。`Animation` 仍是遗留 Core-only 实现库；这次仅沿原路径修改触及算法，完整 plugin 迁移另行结构 review，不声称已解决该历史例外。

Skin 的纯绑定数据留在 Aurora core；资产 handler / 反射 / 创建入口归既有 Aurora Adaptor，模型子产品生成归既有 Aurora Cook，Skin 摘要视图归 Aurora 编辑扩展。新增 Skin 格式读写只使用 Core 数据与 Framework archive，不调用 adaptor 运行时服务；现有 Cook -> Aurora.Adaptor.Static 是实际存在的 schema 分层债务，列入 review 风险，不通过增加目标或搬迁旧 schema 在本次顺带解决，也不把该依赖当作新 Graph builder 的先例。

编译开关拟为 `SKY_BUILD_ANIMATION`，在 `plugins/plugins.json` 注册。基础 plugin 可用于 game；Editor target 受 Sandbox 开关约束，Builder 受 develop/tool 条件约束。配置分别显式加载 `Animation.World`（game/editor）、`Animation.Builder`（cook/editor）和 `Animation.Editor`（editor）。装载依赖与静态链接不同，不能只改 modules_editor.json 就声称 AssetTool 可用。

| 宿主 | 拟接入的装载关系 |
|---|---|
| game/editor 生产 World | `Content.Module` 提供 Clip 服务，已有 `Aurora.Render` 提供 Skeleton 服务，二者在 `Animation.World` 前可用；`Animation.World` 不反向成为 Aurora 的依赖 |
| editor | `Animation.Editor` 在 `Animation.World` 与 Sandbox 编辑服务可用后注册；沿既有扩展注册/发现路径，不要求 Sandbox 编译链接 plugin |
| AssetTool/cook | 在 `tools/asset_tool/main.cpp` 的实际模块注册路径接 `Animation.Builder`；保留已有 `Content.Builder.Module` / `Aurora.Cook`，不加载 Animation.World/Animation.Editor |
| headless core/World 测试 | 直接提供 fake Clip/Skeleton services，不初始化 Aurora renderer；测试结果不替代生产模块装载验证 |

实现时还须核验项目覆盖配置/静态模块聚合路径，将此关系落实到实际合并后的装载图。当前 configs 未列 Content.Module，不能假定它已经为游戏加载。

Clip provider 由 Content 原模块发布，Skeleton provider 由 Aurora Adaptor 原模块发布；两个接口定义在消费方 `engine/animation`，用 Uuid、状态、诊断和既有 renderer-neutral clip/skeleton 数据返回。发布后的 clip/skeleton 内容不可由实例修改。World plugin 不链接这些提供方实现。Aurora 消费 Pose 的 `ISkeletalPoseSource` 定义在 `aurora/renderer`，由 Animation.World 实现；Animation.Editor 经 `IAnimationInstanceControl` 发送播放 / 参数命令并查询状态，不包含 World adaptor 实现头。两者使用同一 World epoch / Actor / instance generation 身份。跨模块服务复用 Framework `Interface<T>`。所有新共享注册表用 Singleton；每份反射数据由唯一 owner 在进程内注册一次，调用源头须明确，禁止通过 Once、已存在跳过或 scratchNode 吞掉重复注册。runtime 重启与窗口创建只消费已注册元数据，不重复反射。服务仅在主线程访问，撤销早于模块释放；借用/完成回调持真实 module lease 或在卸载前完全 drain，不把 Interface 内裸指针误认为所有权。2026-10-25 用户再次确认该约束；原“反射幂等用 Once”方案撤销；用户已回复“按此方案实施”批准 Aurora.Metadata、Content.Metadata、Animation.GraphAssets，详见 `refactor-process-owned-reflection/design.md`。

替代方案：把组件/Graph/编辑器继续塞入 Content 或 Aurora，短期 target 少但扩大 engine 业务实现和渲染耦合；整体搬迁 Animation/Content 则混入无关目录 diff。推荐上述新增实现归 plugin、原算法就地修复的边界。此表与装载列表须确认后才改 CMake/注册入口。

### D2. 组件、World 系统和唯一姿势来源

- `SkeletalMeshComponent`：独立的 mesh UUID、skin UUID、skeleton UUID，`poseSource = ReferencePose | SameActorAnimation`，material slot 覆盖、阴影/可见性配置；不持 GPU 对象。新组件默认 ReferencePose；添加动画的编辑命令显式改为 SameActorAnimation。导入元数据可提供默认三元组，写入组件后即为显式引用；Mesh 本体不保存 defaultSkin / Skeleton 引用。
- `AnimationComponent`：模式 `Clip | Graph`、clip/graph UUID、skeleton UUID、autoplay、非负有限全局 playRate、RootPolicy、参数初值；loop 仅对 Clip 模式生效，Graph 各 Clip 节点自行配置 loop。当前播放时间、Pose、加载请求及 graph 状态不进入场景序列化。SameActorAnimation 时两组件的 Skeleton UUID / revision / 布局须一致，不能从 Mesh 或组件遍历次序推断骨架。ReferencePose 可在没有动画 plugin 时由 Aurora Adaptor 加载 skeleton 构造。
- `AnimationWorldSystem`：World 内持有实例和资产绑定，组件仅注册/更新意图。每个 World tick 的该系统更新一次，所有实例求值与结果收齐在系统返回前完成；最初可串行，后续并行仍保持同一完成边界。

`World::Tick -> AnimationWorldSystem 完成 -> RenderSceneBridge 提取` 是本次时序；不依赖两个组件的 Tick 顺序，也不假设它与物理子系统的相对顺序。首版不写 Actor transform，因此不存在隐式 Root Motion/physics 冲突。停止模拟清理实例状态；编辑态预览只推进独立 Preview World，场景编辑态不自动播放所有动画。

v1 一个 Actor 至多一个 AnimationComponent / SkeletalMeshComponent；不新增 SkeletonComponent，动画实例持骨架和 Pose，纯参考姿势由渲染绑定持有。不同 Actor 可共用 Mesh 却选择不同 Skin / Skeleton。SameActorAnimation 缺失、错误或尚未就绪时须显示对应诊断；只在已验证 mesh/skin/rig 时显示明确标识的参考姿势，不把 fallback 报告为动画 Ready。跨 Actor Pose 引用仍不在本次范围。

通过消费方 `ISkeletalPoseSource` 查询时传当前 World 身份、Actor UUID、实例 generation 与 skeleton 版本。接口只借用 World 身份，不保存裸 Actor/组件地址；World reset/detach 使 generation 失效。返回 Pending/Ready/Error、骨架身份/布局签名、Pose revision 和拥有数据的模型空间关节矩阵。主线程把结果复制进 owned packet，渲染线程不调用 provider、不借用 Animation 对象。

### D3. 实例状态、播放和资产失败

每实例持有 player/参数/状态机/求值临时区/Pose。Graph 资产只共享不可变定义；现有 AnimationPlan 含 mutable posePool，v1 每实例独立创建并持有 Plan，禁止将它缓存为多实例共用对象。不为本 change 强行重写共享 Plan ABI；以后拆不可变程序/执行工作区需单独基准验证。

运行状态为 Unbound、Loading、Ready/Playing/Paused、Error。Clip 的 Pause 不改时间，Stop 回到零时刻并停止，有限 Seek clamp 到 [0,duration]、立即求值并保留播放状态；Graph 的 Play/Pause 控制整个实例，Stop 清除过渡、重置入口和所有节点时间但保留当前参数，Graph Seek 显式 Unsupported。反向/非有限速率及负/非有限 delta 拒绝。非循环到结束只停一次；循环 Advance 映射到 [0,duration)，显式 Seek(duration) 可显示末帧，下一次正 delta 再循环。零时长保持参考/末样本，不除零。

资产加载沿现有 AssetManager/provider 异步请求；UI 和 World tick 不 BlockUntilLoaded。带 generation 的请求完成后核验最新绑定，旧结果丢弃。第一次缺资产或不兼容显示诊断，不显示伪造成功的动画；只在 mesh/skin/rig 已通过验证时显示明确标识的参考姿势。

动画系统只负责 rig/clip/graph 的一致实例，不依赖 Mesh / Skin 资产。Aurora 绑定负责 mesh/skin/rig 与发布 Pose 的版本校验；其候选快照仅在全部匹配后切换。同绑定 reload 失败保留最后一个完整可渲染快照并标记旧版本；新动画版本尚不匹配时可冻结旧画面，不承诺旧动画继续推进。用户明确更换绑定时立即停止发布旧绑定的新画面，进入 Loading/Error；已提交帧正常退休。删除组件、撤销、World reset 和模块卸载先失效 generation，收齐已接纳 CPU 任务，再释放。

### D4. mesh/rig 坐标、权重和产品身份

首版每实例一个角色模型空间、一套 rig，最多 256 个 rig 关节（包括 helper）、256 个 Skin 槽，每顶点最多四个正权重 influence。JOINTS 的 `U_RGBA8` 索引的是 Skin 槽，不是 Skeleton 索引。原始 influence 先检查有限非负、重复骨合并与正权重数量，超限失败；零权重槽编码为合法索引 0。禁止在校验前使用 aiProcess_LimitBoneWeights 裁掉证据；skinned import 禁止 OptimizeGraph/OptimizeMeshes 在捕获节点身份和原始绑定前合并/移除节点。MeshOptimizer 后再次核验 JOINTS / WEIGHTS / bounds 同步。

实施核验发现当前 Assimp glTF 导入会丢弃负权重，并允许多个 node 的 Skin 覆盖同一 mesh 的绑定记录。因此 glTF/GLB 在进入 Assimp 前直接读取原始 JOINTS/WEIGHTS accessor，验证正权重数、范围和有限非负值；随后为每个绑定复制导入器 mesh 记录，正规化后再按几何/权重/槽布局判断共享。首版该入口支持内嵌 data URI 与 GLB BIN；外部 buffer、sparse influence accessor 和无法证明骨身份唯一的 Assimp 输入显式 Unsupported。此限制不宣称已覆盖所有 glTF/FBX 变体。

规范空间：mesh 顶点烘焙到约定角色模型空间，骨架姿势 `G_j(t)` 也在该空间。若原 mesh 节点到模型变换为 `M_mesh`，其原 inverse bind 为 `O_s`，该 Skin 槽保存 `IB_s = O_s * inverse(M_mesh)`；解析后的槽映射为 `r(s)`，调色板 `P_s(t) = G_r(s)(t) * IB_s`，世界位置 `W_actor * sum(w_s * P_s * p_model)`。每个有效槽验证 `G_r(s)(bind) * IB_s ≈ I`。必须先对每个原始 mesh/绑定正规化，再判断是否能合并；禁止全模型按骨名保留第一份 offsetMatrix。不同绑定的映射/IB 不同是合法的，只有同一目标绑定的契约冲突才拒绝。

保持完整关节祖先闭包及确定的父先子后 rig 顺序；rig 重排只重建 slot -> joint 映射，不把 JOINTS 改为 rig 全局索引。模型空间 / 槽布局签名描述坐标约定和有序输入槽身份，不等同于 Skeleton UUID。跨 Mesh 共享 Skin 须匹配此契约且逐槽通过参考姿势验证。同名路径歧义、不可逆/shear/非有限本地 TRS 均报错；动画未写的关节或 TRS 通道每次采样从参考姿势补齐，不能继承上一个节点/帧的临时区内容。

Skin / Skeleton 子资产身份保存在源 manifest 的子产品映射中，key 为导入器保留的稳定 rig / mesh-binding 源身份；没有持久源 ID 时使用明确路径，重命名视为新子身份，只有显式 rebind 才复用旧 ID，不用名称相似度猜测。recook / 源移动复用，复制源分配全新子身份。默认 mesh/skin/rig 组合是导入元数据，组件选择它时写入三项 UUID。generated catalog 记录 child UUID/type -> owner source，可供资产选择、Graph 依赖校验、按需 cook；child 请求合并到 owner source，完成事件扇出所有等待 child，避免永久 Loading。源移除子项保留失效诊断，不把旧 UUID 分配给别的绑定。

rig 的布局签名覆盖关节路径、顺序、parent 和参考姿势；UUID 相等但签名变化也要求重新绑定。Clip v1 工作流要求显式 skeleton 身份；未知通道不能静默遗漏，不能仅凭同名骨认为两个 rig 兼容。

烘焙顶点时，法线用 M_mesh 的 inverse-transpose，切线用线性部分再对法线正交化，镜像同步调整切线 handedness 和三角形绕序；不沿用现有把切线也乘 inverse-transpose 的实现。蒙皮后的法线/切线采用 D6 的统一 CPU/GPU 约定。

### D4a. Checked 产品发布与运行中实例

当前存储以 successor `simplify-asset-product-storage` 为准：SaveAsset/SaveProducts 返回 ProductStatus，flat UUID 文件包含版本、身份、精确长度与 checksum。完整产品列表先编码、stage、flush、读回校验，再逐文件 atomic replace。批量失败不得发 cook/package success；部分替换或 index 失败会使该批输出失效，不承诺恢复上一完整组。没有 ProductGroup、owner manifest、磁盘 generation 历史或旧格式 fallback；开发期清理旧输出并 recook，首个正式版之后再单独设计迁移。

source owner 与稳定 child UUID 仍由源 catalog 管理，child cook 合并与各自完成结果不变。Runtime 只读 UUID 产品与 product.index。新加载创建新的不可变 Asset instance，缓存用 GetInstanceId/GetRevision；旧对象的 bulk 与 GPU 使用由已绑定 stream/owned bytes 和 frame lease 保活，不按 UUID 路径重开文件。

Mesh/Skin/Skeleton 候选须完整校验输入槽、骨架布局、参考姿势与所持实例；产品变化时重建候选，通过后才切换活动资源。打包遍历 World 显式引用和 Graph/Clip UUID 依赖，不需要组元数据。多次文件替换不构成跨进程的多文件 snapshot 事务。

### D5. Clip 时长与格式兼容

复核表明 GetFrameCount 已使用最后 key time，问题不是稀疏 key 数量，而是 Compress 删除末尾重复 key 后原始时间丢失。新增权威 sourceFrameCount，在压缩前由原始通道最后时刻和显式时长确定；产品保存它，压缩不得缩短时长。无元数据的代码构造 Clip 保持 `(lastKey + 1) / frameRate` 回退。元数据小于最后 key、不同步的 time/value 数量、非单调/重复时间和非有限值均失败。

已有 v2 `.clip` source 可读入并在压缩前推导长度；新 source 可显式携带长度；新产品版本不尝试从已压缩的旧产品恢复丢失时间，要求 recook。Content 保留 Clip handler/builder；不把其迁到 plugin，也不重复注册同一资产类型。ACL 类当前独立存在，仍不宣称本资产路径已采用 ACL。

### D6. Skin 绑定、实例调色板与 GPU 运输

按用户确认采用 Godot 的组合语义，参考 [MeshInstance3D](https://docs.godotengine.org/en/4.6/classes/class_meshinstance3d.html) 和 [导入绑定说明](https://docs.godotengine.org/en/4.6/classes/class_resourceimporterscene.html#class-resourceimporterscene-property-skins-use-named-skins)：Mesh / Skin 是独立资源，实例另选骨架。这里独立资源以 `AuroraSkin` 导入子资产落地，不要求独立手写 `.skin` 源或新增专用编辑器。

| 数据 | 权威所有者 |
|---|---|
| 顶点 / 索引 / JOINTS / WEIGHTS、有序 skin 输入槽、槽布局/模型空间签名、每槽 mesh-space bounds | Mesh 产品；不含 inverse bind 或 Skeleton 引用 |
| 版本、有序输入槽契约、每槽 inverse bind、目标骨的规范名称/路径或索引 | Skin 产品；不含 Pose 或必需 Skeleton UUID |
| 层级、参考姿势、骨身份和布局签名 | Skeleton 产品 |
| mesh/skin/skeleton UUID、Pose 来源 | SkeletalMeshComponent |
| 解析后的 slot -> joint 映射、精确资源版本与验证结果 | Aurora 的不可变 ResolvedSkinBinding，按 mesh revision + skin revision + rig UUID/revision/signature 缓存 |
| 当前 Pose、palette、revision | 动画实例及渲染实例/帧 |

Skin 一个产品只用一种目标标识模式：Named 按规范路径/唯一骨名解析，缺失或歧义失败；Indexed 必须携带目标 rig 布局签名，签名不符拒绝，不能只检查索引不越界。默认导入使用 Named；无论哪种模式都验证输入槽顺序、有限可逆 IB 和参考姿势恒等。允许多个槽映射同一关节，但须逐槽验证。Skin 不把自己强绑到某个 Skeleton UUID；是否可用于另一骨架由解析验证决定，不提供自动重定向或自动重算 IB。

同 Mesh + 同 Skin 可配不同骨架运行时实例；同 Mesh + 不同 Skin 可配不同兼容骨架定义。相同骨数/骨名不足以证明兼容。权重不同须另一个 Mesh，两个骨架共同驱动一个绘制实例暂不支持。模型导入保留不同原始绑定记录，仅对相同几何、权重、槽布局和模型空间合并 Mesh；不能从导入结果恢复原始绑定关系时明确 Unsupported，不能全局按名字合并为一套 Skin。

发布后的 Skin 绑定不可变，移除其中的求值后 boneMatrices 所有权；每帧组合结果属于独立 SkinningPalette。移除 Mesh::SetSkin/GetSkin/HasSkin，几何是否带 influence 数据由明确顶点布局能力表示；不再存在修改共享 Mesh 来替换实例绑定的入口。`SkinnedMesh` 保留 tag，持实例选择的 Skin / resolved binding / palette，不与 Framework Actor 组件混用。

扩展现有实体描述/更新 payload，包含 mesh/skin/rig 精确身份和 revision、resolved binding generation、owned palette、Pose revision、world transform 与动态 bounds。桥接仍由唯一 host producer 生成 sequence，BUSY 重试保留原 packet；资源与姿势不可分别提交成半状态。每槽 palette 按 JOINTS 槽排列，不能把整个 rig 矩阵数组直接当 palette。

首版采用 vertex shader linear blend skinning。关节 palette 是 Batch 归属的只读 structured buffer；在现有 frame lease 内分配/upload，buffer range 与骨骼数匹配，descriptor/layout 由实际 Pass 声明决定。扩展权威 C++ shared declaration 并导出 `.slangh`，不硬编码新 set 或复用普通 per-object UBO 假定容量。若现有通道不足，只补这一运输类型，不新增上传 ring/loader，也不每帧把同一 mesh 当新全局资产上传。

Opaque 与 Shadow 使用同一 deformation helper、palette 和 revision，shader 选择在 prepare 期完成。支持 JOINTS 整数顶点属性；所有骨索引在入队前验证。法线使用逐关节 inverse-transpose 线性混合后归一化的 LBS 近似，切线经线性变换后与法线正交化；CPU oracle 采用相同约定。输入奇异关节矩阵在提交前返回错误；加权方向抵消时 shader 使用确定的参考方向回退保证有限输出，此退化行为须有专项测试。首版内置材质/阴影完整通过；自定义材质未声明 skin 输入即 Unsupported，禁止静默绑定静态 shader。

GPU 在途帧只读精确 palette；CPU 下一次求值、删除角色、reload、关闭窗口都不能覆盖未退休区间。沿现有 FrameContext/lease/完成机制释放。所有三个 backend 需接口与编译覆盖，Windows Vulkan/DX12 原生验收必做，Metal 实机验收单列未完成条件，不以 mock 代替。

#### D6a. 顶点、材质、Pipeline 三类变体输入（用户纠正后的实施门）

2026-10-24 用户再次明确：最终 shader 变体由顶点能力、材质 Batch 开关和 Pipeline 开关共同决定。沿用现有 `ShaderVariantAssembler` / `GlobalVariantLayout` 的 P | V | S 模型，不以独立 `SkinnedLit/SkinnedUnlit/SkinnedShadow` 源路径、专用材质副本或 `usageVariant = skeletal ? 2 : 1` 代替组合。Lit、Unlit 和 Shadow 仍是各自的程序/Pass 选择；开启蒙皮是这些程序的顶点域输入。

| 输入域 | 取值与权威来源 | 禁止的替代行为 |
|---|---|---|
| Vertex | 当前 draw 的顶点语义、是否启用 skinning、是否启用 instancing 等已验证几何/实例能力 | 材质强行开启/关闭骨骼或 instance；仅凭存在 JOINTS 就忽略实例的实际变形契约 |
| Material / Batch | 材质 schema 声明的开关及其显式值/默认值 | 覆盖 Vertex 或 Pipeline 域；把资源位于 Batch 等同于开关一定归材质所有 |
| Pipeline | 当前冻结 Pass/View 配置，例如 SHADOWS、IBL；由消费该 draw 的 Pipeline 提供 | 读取 live 配置；由材质覆盖全局开关；在资产首次加载时固定所有 View 的取值 |

三个域先验证 owner、类型、取值范围和必需输入，再组装规范化选择与 P/V/S key。重复、未知和跨域覆盖返回显式错误；不使用“最后写入获胜”。instance 输入必须有位置和身份，首版未支持的 instance 组合返回 Unsupported，不因此扩大本 change 为 instancing 实现。Skin 的矩阵内容、Pose 时间和 rig/skin 资源 revision 用于精确绑定与退休，不作为每帧制造 shader 变体的开关。

选择点必须具备 draw + material + Pass/View 的完整输入。同一材质在同一帧用于静态/蒙皮模型、不同 Pass 或不同 View 时可解析到不同程序；不得以材质 UUID 或 Tag 单独缓存已编译程序。`SceneAssetSource::MaterialPayload` 当前在缺少 Pass/View 输入时提前编译全部 technique，这一边界必须纠正，不能只把硬编码 `#define` 搬进材质 variant。

运行时准备、usage 收集、离线 cook 和 products-only 加载使用同一规范化规则及源/入口/stage/target/schema/layout/toolchain 身份；产品须保存无源码重建选择所需的元数据。现有 `ShaderUsage` 将 variantHash 重新计算为 `ContentHash()` 的路径也需核验，不能丢掉 P/V/S 输入。变体实际反射决定启用的顶点属性和资源布局，Mesh 属性按 semantic/index 匹配反射 location；这一匹配不能代替变体选择。

颜色与阴影使用同一 Vertex 输入和 palette revision，同时接受各自 Pass 的 Pipeline 输入。验收必须包含三域组合矩阵、跨域覆盖拒绝、同材质跨几何/View 共存、热切换及无源产品命中。此项为当前 apply 的 P1 阻断项；之前的独立 Skinned 路径编译/GPU 测试仅证明基础传输，不能证明变体设计正确。该纠正复用现有模块，不引入或批准 `refactor-aurora-shader-source-techniques` 的新 DSL/技术体模型。

### D7. 变形 bounds、选择与预览一致性

Cook 为每个输入槽保存所有正权重关联顶点的 mesh-space AABB，空槽单独标记。运行时用该实例的 `P_s(t)` 变换各槽 AABB 后取 union，再施加 Actor world transform；非负归一化 LBS 顶点位于此凸包界内。同一 Mesh 切换 Skin 不必重写其 bounds，也不能复用另一 Skin 的 joint-local bounds。所有 bounds 与 palette 同 revision，CPU oracle 验证实际变形顶点被包含。

现有静态三角形 picking/highlight 的 positionsPreserved 条件不适用于蒙皮。首版通过 Outliner/保守 bounds 选择并明确精度，骨骼 debug 和 bounds 可显示；禁用错误的 bind-pose 三角形拾取/轮廓。精确变形拾取及蒙皮轮廓后续在可用 deformation 查询契约上扩展。

预览用同一个 RendererHost 中独立 scene/view/output，仍由原 producer 提交；工具窗关闭等输出退休，不建立第二个 Device/Renderer。Preview World 独立于编辑世界和 PIE；模型/材质/骨架/动画走生产路径，不用另一套 preview-only skinning。

### D8. Graph 源文档、cook 与实例化

源 `.graph` 使用版本化 JSON：文档 ID、skeleton UUID、参数定义（稳定 ID/name/type/default）、稳定节点 ID/type/properties、带类型 pin 的连接、根输出、状态/过渡表，以及 `editor` 下的节点位置/视图状态。编辑布局不进运行图；显示名称改变不破坏引用。

v1 pose 节点为 Clip、Blend2、Output；Blend2 的 alpha 可为常量或 float 参数。Graph 可以是普通 pose tree，或一个顶层平面状态机；每个 state 拥有自己的 pose tree。pose 输入不允许环，v1 不共享同一有状态节点给多个父节点；参数可多处读取。状态转移环（Idle -> Walk -> Idle）合法，不可套用 pose DAG 的无环规则。未知节点、重复 ID/name、悬空/错型 pin、多个输出、无入口、缺依赖、骨架不兼容均给出文档/node/pin 诊断。

Graph builder 只做文档校验、源/产品依赖登记和规范化控制/pose 定义的 cook，不同步加载 runtime Clip 或依赖 Content.Adaptor。纯结构校验失败不产出新版本；骨架/Clip 内容兼容校验在 runtime 绑定前再次完成。产品包含参数槽、状态/条件/过渡、节点索引、Clip UUID、拓扑和 source node ID 映射，不能只保存 AnimationPlanBlob（它没有完整控制平面）。

加载流程：Graph product -> 请求 skeleton/clip providers -> 核验精确版本 -> 为实例构造控制对象 -> 使用既有 LowerToPlan 生成该实例 Plan -> 预分配 Pose -> 发布 Ready。只在创建/rebind 编译，不每帧重编。core 编译不持 AssetManager/renderer；Graph cooked 定义可共享，任何 node raw pointer、player 和 posePool 不共享。

Graph 原 `AnimationAssetData` 只有版本号：保留资产类型字符串 `AnimationGraph` 和源 UUID，但产品升版；旧空源导入为明确空文档，要求建立输出才能 cook。旧产品不能静默运行参考姿势当作成功。切换时 Content 取消 `.graph` 扩展/handler/view 注册，Animation plugin 一次性接管，确保任意装载顺序只有一个 owner。

### D9. 首版状态机与 Root Motion 边界

参数首版支持 bool/float，类型固定，SetParameter 校验名字/ID/type/有限值。bool 只支持等于/不等于，float 支持显式比较；transition 内条件为 AND，空条件为 true。Blend2 有限 alpha clamp 到 [0,1]，TRS 在本地空间混合、rotation 使用归一化最短弧插值，再计算模型空间矩阵。每次 Clip 采样先恢复完整 reference pose，避免部分通道留下旧值。

normalized exit time 必须指定本 state 的一个 Clip 节点作为 timeSourceNodeId，使用其从本次进入起累计的非回绕时间 / 正 clip duration；阈值限制 [0,1]，达到后在本次驻留中持续满足，参数条件可稍后触发。零时长 clock 仅允许阈值 0；没有 clock 的 Blend2 状态不能猜用哪个 Clip 的时长。过渡 priority 越大越优先，同优先级按稳定 ID 升序，每 tick 最多开始一个。

每 tick 先冻结参数，在 tick 起点用累计 clock 选择过渡，然后用全局 rate 缩放后的 delta 推进活动 Clip 与 fade，最后采样/发布；本 tick 才越过的 exit 阈值在下个 tick 判定。duration 必须有限非负，0 当帧切换，正值混合 source/destination；跨过 fade 结束的大 delta 中 destination 推进完整 delta，source 只推进到完成点，不能再启动第二个过渡。过渡期间不抢占，完成后下个 tick 才评估后继。reentry 重置节点时间，v1 拒绝 self-transition；inactive state 不推进。Pause / rate=0 冻结 clock、fade 和状态转换；允许参数更新后重采样当前姿势。

不将现有 Plan 单 activeState 自动推广为嵌套状态机；v1 graph schema 明确拒绝嵌套/多个状态机节点。为 crossfade 扩展必要的活动 source/destination 控制与 pose blend，测试双边启用/非活动禁用。seek、loop 等控制必须在 Plan 路径生效，不能依赖它未调用的虚节点 PreTick。

首版 Root Motion 仅 `KeepInPose | LockToReference`；后者在最终本地 Pose 上恢复全部 rig root 的参考 TRS，再计算模型空间矩阵，不暗指髋骨或任意 helper 下的 locomotion root。绝不写 Actor transform，不提供伪每帧位移 API。将来新增 ExtractDelta 时必须定义 locomotion root、前后时间、跨圈、双 Clip 混合、消费一次和 physics 时序，并与 `animation-events` 共同 review。

### D10. 编辑器：共享画布、反射与保存

Animation.Editor 注册 Skeleton/Clip/Graph 专用编辑扩展；Clip/Skeleton 的既有只读摘要仍可并存。通用 GraphCanvas 置于 Sandbox widgets，仅负责缩放/平移、选择、拖动、pin 命中、连线、框选、删除/复制；节点语义、graph schema 和状态机模型在 plugin。不新接 Qt 或外部节点编辑框架。

EditorCore 的 IAssetPreviewProvider 扩展为 CreateSession / Update / Resize / Close / Status 契约；session 持独立 generation，shell 通过现有 RendererHost 提供 scene/view/output，provider 在输出退休前保持代码 lease。现有 bool GetPreview 只表示能力，不足以拥有多个预览。Skin 由 Aurora 扩展注册只读反射摘要并显示源 owner；用户重导入 owner 来修改绑定，不直接编辑 cooked 矩阵。Animation.Editor 对所选三元组创建生产组件并通过 IAnimationInstanceControl 控制，缺 provider 显式不可用。

GraphDocument 管理源文档/dirty，CommandService 管理节点/连线/参数/过渡和布局修改，拖动合并为一个事务。所选节点/组件/cook 设置经 ReflectedFormView；动态节点属性用节点描述符提供反射对象，不在 shell 写逐类型 switch 表单。ActionRegistry 统一菜单/工具栏/快捷键，UiTheme/UiSkin 决定尺寸与颜色。

角色/Clip 工具分别选择 mesh、skin、skeleton，显示绑定模式、解析映射与具体不兼容槽；提供骨骼树、变换、bounds、播放与 Clip timeline。Graph 工具显示参数、state/过渡和 source node 诊断；Graph 不提供伪装为全图 seek 的 scrub。临时调试覆盖与资产默认值分开。首版只预览最后成功 cooked Graph；未保存草稿只作结构校验，不用 source 直接绕过产品 loader 创建生产实例。

Save 在主线程延后执行：校验文档可序列化/schema -> 通过 AssetDataBase 解析路径 -> 原子写源一次 -> RequestCook 入队 -> tick 处理 -> 收到结果后请求新 preview 实例。允许保存缺输出/缺连接的可编辑草稿，执行语义校验失败使 cook 失败，不阻止保存工作。UI dispatch 不重建自身 widget、不 cook。source saved、cook result 和 preview revision 独立显示；失败保留旧预览并明确标记。关闭 dirty 文档复用宿主保存流程，退出时等输出退休。

dirty 文档关闭或替换时，宿主立即显示模态 Save and Close / Discard Changes / Cancel Close 确认，复用当前文档 scope 的 ActionRegistry，不仅在状态栏提示或隐藏于菜单。确认期间阻断底层文档的指针、文本和快捷键，Esc 取消关闭，Tab/Enter 可选择操作。保存失败保留文档与确认框；正在保存时禁用重复保存和放弃。若仍有未提交的字段或拖动事务，禁用保存并提示先取消关闭、完成编辑；明确放弃则在 tick 取消交互后释放文档，不能被 active edit 永久阻塞。取消关闭保留交互和草稿；所有销毁仍延后到 tick。

Graph 内容向宿主声明适配可用视口，画布保持可见，Graph/选中节点/预览配置通过 scoped actions 驱动的页签切换；属性继续使用有独立滚动的反射表单。页签复用 ToolBar 的输入/状态解析，以 UiSkin.DrawTab 呈现，不另写配置控件。默认展示预览配置，选择节点切到节点属性。画布坐标以逻辑单位保存，DPI 只在绘制和命中转换处应用一次；提供 Frame all，节点优先显示 Clip 资产名和 Output 所属 state 名。

预览缺失或加载失败时，视口内部显示可换行的诊断和后续操作，不仅显示黑色空框；骨骼/包围盒不遮挡未就绪提示，无 Mesh 时明确为骨骼预览。首次可用时按 bounds/骨姿势居中相机；配置 Mesh 时必须等 Mesh bounds，不能提前按先到达的骨架结束聚焦，取景距离同时考虑视场角与窗口宽高比。Mesh/Skin/Skeleton 位于配置首部。选择 Mesh 后只在导入绑定唯一、既有 Skeleton 未指定或匹配时补齐空 Skin，作为同一次可撤销配置编辑。编辑器启用已有 Content.Builder.Module 以识别 .clip；命令行项目在全部模块注册完成后刷新目录，避免 Init 阶段尚未注册的 builder 造成资产遗漏。以上复用既有 target、provider 与 renderer。

### D11. 性能、错误与验收约束

首版以正确性和闭环为目标，不宣称 SIMD/GPU 求值或多人群性能。热路径复用实例 Pose/矩阵/Plan 临时区，缓存容量已足够时不因 Evaluate 每帧分配；packet 的 owned transport 另行计量，不把整帧零分配当成已有结果。

外部数据及原生 API 失败用 Result/Status/错误码；错误带资产/node/actor 上下文。禁止 throw/catch/dynamic_cast，不用 assert/terminate/静默忽略处理坏资产。测试分配计数器既有 `throw std::bad_alloc()` 不自动照搬；若需修改该测试钩子，先说明不抛异常且保持正确分配契约的替代/隔离方案，单独确认后处理。

阶段门见 tasks；覆盖二维以上对比：同 mesh 两实例、不同 state/参数、同一 World 编辑与 PIE、两帧在途、失败 reload、BUSY、删除/撤销/关闭。GPU readback 用已知两骨 fixture 与 CPU LBS oracle 核验颜色/阴影的形变；不能只用类型存在或 shader 编译成功验收。

## Risks / Trade-offs

- [既有 Animation/Content 分层债务] -> 原路径局部修改，新增业务实现归 plugin；不让 Aurora 反向依赖 plugin，后续结构迁移单独 change。
- [目标数增加] -> 每个 target 有明确不可跨越的依赖预算；D1 已于 2026-10-24 确认，实施中改变归属或入口须重新说明。
- [缺少模型动画 take 导出] -> 首版交付组件/Graph 工作流，使用确定的 `.clip` fixture；导入功能只有独立 change 明确格式/子资产契约后才算可用。
- [256 骨/四权重与 TRS 限制] -> 明确错误与 UI 诊断；扩大格式另行升版，不能截断或变形近似。
- [保存/加载原子性与 manifest 扩展] -> source UUID、子产品映射、产品依赖整体验证，故障注入测试，检查失败不得报告 cook/package 成功；在用实例正常退休。
- [自定义材质和静态拾取不适用] -> 明确 capability/Unsupported；颜色/阴影共用蒙皮，选择用保守 bounds，禁止错误静态退化。
- [Metal 尚未实机验证] -> 单列平台验收；不得将 Windows 结果表述为全平台完成。

## Migration Plan

1. 确认 D1 模块/装载、首版范围和 schema 版本政策；只批准文档不自动视为批准所有实现。
2. 原地修复资产正确性与 duration；实现 generated catalog / checked flat 产品发布，再将 Mesh v5 内嵌 skin 改为独立 AuroraSkin 新版本产品，读取入口按版本拒绝旧布局并要求 recook。新的 AuroraSkin 格式也明确拒绝历史已删除 Skin 资产的旧版本，不能只因类型字符串相同而接受。
3. 建立新 plugin/provider 与 SkeletalMeshComponent；先完成 single clip、双实例和颜色/阴影闭环。
4. 接入共享预览宿主；再切换 Graph 空 handler 到新 owner，发布完整源 schema 与产品格式。
5. 完成 Graph 运行时与编辑器，更新用户说明/示例；产品与代码版本一起验证。

旧 Mesh source UUID 保留，新 Skin 子 UUID 持久化。首次迁移已有随机 Skeleton 子 UUID 时仅在旧产品与唯一 rig 可核验对应时写入 manifest 复用；丢失/歧义时报告需显式重绑，不承诺恢复不可知的旧身份。现有 World 尚无本次组件，不改写静态组件类型。回滚代码时同步回滚源格式变更并重建匹配版本产品。删除旧 Graph 空实现仅在新 owner 就绪后发生；不维护两个同名 handler。提交包含 specs 与实现；commit/archive 均另行确认。

## Open Questions

已确认项（2026-10-24 `apply`）：D1 的新增 targets/入口与 plugin 归属；首版按上述边界暂不做模型动画 take 导出、复杂图和角色 Root Motion；256 骨/四权重上限。

技术验证门（不能靠批准替代）：Assimp fixture 的模型/mesh/bone 空间与 helper 节点映射；现有 FrameContext 的 structured buffer palette transport；manifest 子产品映射与故障恢复；Metal 原生验收。每项均进入 tasks，有实测结果后更新设计，不把未知标记为已支持。

## Delivery boundary — 2026-10-26

按用户最新要求，剩余工作分为材质/Shader 前置提交与完整动画提交。前置提交包含通用反射 owner、V/P/S 选择、四组资源、材质 schema、共用编辑器宿主和 SKINNED Shader 输入/布局；本 change 的 Skin/Skeleton 资产、姿态/边界快照、Scene palette 上传、World、Graph、角色预览及打包保持一个完整提交。依赖前置的通用 definition/session 与 Shader 契约后，本批一次性移交旧 Content Graph handler、引入 Animation.GraphAssets、注册 Graph/Preview 类型并更新所有 World/Builder/Editor 装载依赖；Skin/Skeleton、导入、运行时、Graph 编辑器、预览与打包不再分拆。
