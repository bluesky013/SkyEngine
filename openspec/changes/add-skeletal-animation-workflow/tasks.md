Status: applying；2026-10-24 用户以 `apply` 确认严格 review 后的 D1 模块、装载和首版范围。只勾选已实施并验证的任务；提交与 archive 仍需单独确认。

## 0. Review 与实施门

- [x] 0.1 向用户确认 design D1 的新增 plugin、target、装载入口与职责；记录明确确认后才修改构建/注册。确认首版 `.clip` 输入、256 骨/四权重和排除项。（2026-10-24：用户在严格 review 后回复 `apply`。）
- [ ] 0.2 固定带变换 mesh、helper、两骨动画、同 Mesh 同 Skin 双实例、同 Mesh 不同 Skin/rig、错序 rig、五权重原始输入 fixture；建立 CPU LBS/法线/bounds oracle。
- [ ] 0.3 核验与 scene-bridge、pipeline resource ownership、world-editor 的现行接口；按已确认的 typed V/P/S 与 canonical Material schema 设计核对；原 source-technique DSL 已撤回；更新 review.md 的交界。

## 1. 资产与播放基础

- [x] 1.1 为骨架加载/创建加入数量、名称/路径唯一、parent bounds/cycle、参考姿势长度、有限 TRS 和布局签名校验，显式错误替换触及的坏资产断言路径。
- [x] 1.2 在原始模型数据上验证 influence，禁止 LimitBoneWeights 提前裁剪和 OptimizeGraph/OptimizeMeshes 提前破坏身份；构建 helper 祖先闭包及 parent-first rig，保留独立 Skin 槽。
- [x] 1.3 按 D4 统一顶点、G(bind)、IB 空间及切线线性变换/正交化；验证非均匀/镜像 mesh、Actor 变换、不可逆/shear 与 bind 恒等条件。
- [x] 1.4 加入 256 rig 关节/256 Skin 槽/四正 influence、槽索引、归一化与零和失败检查；输出 mesh-space 槽 AABB；优化后重验所有流和 bounds。
- [x] 1.5 为源 manifest 增加稳定 Skin/Skeleton 子产品映射和默认组合；覆盖 recook、源移动/复制、子路径重命名/移除、多 bundle；旧随机 Skeleton 身份仅在可核验时复用。
- [x] 1.6 在 Framework 实现显式 Result 的产品组 staging/commit、单次清单发布与 pinned revision；注入子文件/flush/commit/进程中断和陈旧请求失败，验证旧组仍完整可读。（Windows 原生故障与进程终止测试通过；非 Windows 原生运行另行验收。）
- [x] 1.7 在 Clip source/product/runtime 保存压缩前 sourceFrameCount，保留旧 v2 source 读取；覆盖常量 clip、静止尾段、稀疏 key 和无元数据回退。
- [x] 1.8 校验 Clip time/key 数量、时间顺序、有限值、显式长度和骨骼通道；产品升版，旧产品给出 recook 诊断。
- [x] 1.9 统一虚图/Plan 路径的 Play/Pause/Stop/Seek/loop/rate 控制；覆盖 paused init、循环多圈、零时长、结束边界和非法速率。
- [x] 1.10 回归 AnimationTest、Content.AdaptorTest、Aurora.CookTest 与骨架加载测试；若触及遗留 throwing 分配钩子，先列出替代方案并单独确认，不新增 throw/catch。
- [x] 1.11 实现 generated child catalog/type/owner 查询、picker 与 Graph 依赖登记；child 按需 cook 合并到 owner，逐 child 完成通知；覆盖删除 child、无产品与双请求不挂起。
- [x] 1.12 输出独立版本化 AuroraSkin，Mesh 移除 skin bulk / skeleton 引用；每原始绑定正规化后再判断共享几何，禁止 GlobalBoneIndex 按名称吞掉不同 offset。
- [x] 1.13 加入 Mesh v5 与历史 AuroraSkin 旧格式 recook 诊断；保留 source UUID 和确定的迁移映射，不把缺失旧子身份自动指向新 rig。
- [ ] 1.14 Clip/Graph 保存接显式 staging 成功结果；新 revision 使用新 asset snapshot/bulk stream，不覆盖在用对象；覆盖压缩/写入失败与加载跨提交点。
- [x] 1.15 采样先填完整 reference pose 再写已有 TRS 通道；覆盖部分 clip、状态切换、helper 与复用工作区残留值。

## 2. 动画 feature 模块与消费方接口

- [ ] 2.1 按已批准 D1 建立 Graph core/assets、World adaptor、Builder 和 Editor 的独立 target/开关；检查完整依赖图无 engine -> plugin、core -> adaptor/editor/builder。
- [x] 2.2 在 engine/animation 定义 Clip/Skeleton source 接口及状态/诊断数据；在现有 Content/Aurora provider 中实现，沿 AssetManager 请求而非另建 loader。
- [x] 2.3 在 Aurora renderer 接口定义 ISkeletalPoseSource；由 World adaptor 实现，明确 main-thread 查询、World/Actor generation 和 owned result。
- [ ] 2.4 配置 game/editor/cook 模块入口和实际依赖，验证插件关闭、无 editor 构建、provider 缺失以及合法装载顺序。
- [x] 2.5 为共享注册表采用 Singleton；每份反射数据由唯一 owner 在进程内注册一次，禁止 Once/查表跳过/scratchNode 吞重。实施 provider 注册/撤销和真实模块 lease/drain，验证关闭 World 后卸载不留回调和借用。（2026-10-25 唯一 owner 方案获批并实施；Framework 205/205、真实模块重载/窗口回归 2/2 通过，详见 refactor-process-owned-reflection。）
- [x] 2.6 定义 Animation.Editor 消费的 IAnimationInstanceControl，World adaptor 实现播放/参数/调试状态与 stale generation 返回；编辑器不直接访问 node/player。

## 3. 单 Clip 组件与 World 实例

- [x] 3.1 实现反射 AnimationComponent 配置和 World 序列化，注册进组件菜单；所有配置通过通用 Inspector 编辑与撤销。
- [x] 3.2 实现 AnimationWorldSystem 的 attach/detach/reset/start/stop，组件只提交意图；保证每实例每 World tick 求值一次并在返回前收齐。
- [x] 3.3 实现非阻塞 rig/clip/graph 绑定、generation 与签名校验；保持动画系统不加载 Mesh/Skin，组件 loop 只作用 Clip 模式。
- [x] 3.4 实现每实例 player/Plan/Pose/矩阵存储及 KeepInPose/LockToReference；禁止可变 Plan/Pose 共享，禁止写 Actor transform。
- [x] 3.5 实现同绑定 reload 原子替换与失败保留旧完整版本；显式换绑定不保留伪装成新资源的旧角色。
- [x] 3.6 测试双实例独立时间/参数、组件插入顺序、seek/pause、过期回调、删除/撤销/World reset、无 renderer 的 provider 驱动求值。

## 4. Aurora 骨骼网格与生产蒙皮

- [x] 4.1 新增 SkeletalMeshComponent 的 mesh/skin/skeleton、ReferencePose/SameActorAnimation、材质覆盖与反射/序列化；导入默认组合转显式引用，保持静态组件回归。
- [x] 4.2 注册独立 AuroraSkin handler，移除 Mesh::SetSkin/GetSkin/HasSkin 与 Skin 的 boneMatrices；保留 SkinnedMesh tag，验证同 Mesh 不同 Skin/rig 及同 Skin 多实例隔离。
- [x] 4.3 扩展 SceneAssetSource 的几何转换以接收整数 JOINTS/WEIGHTS，Skin 单独加载并在实例绑定阶段解析；不再由 CreateSkinFromMeshAsset 隐式挂接，明确格式/材质诊断。
- [ ] 4.4 扩展命令/RenderSceneBridge，提交 mesh/skin/rig/resolved binding/world/Pose/bounds 精确元组；验证 BUSY 原内容重试、代际拒绝、rig 先 reload 时旧快照显式冻结。
- [ ] 4.5 在现有 frame lease/上传/资源声明链增加 Batch palette structured buffer；验证 descriptor 范围、对齐、骨数、两帧在途和删除/reload 退休。
- [x] 4.6 更新权威共享 Pass 声明与导出，增加内置颜色/阴影 deformation helper 与 prepare 期选择；禁止依赖未批准 shader DSL。（独立 Skinned 源已撤销，颜色/阴影统一为 Lit/Unlit/Shadow 的 SKINNED 顶点变体；BuildView 按冻结 Pass/View 的 Pipeline 值和 draw 的骨骼状态选择已编译变体。GPU 回归 950 项 0 失败。）
- [ ] 4.7 用每槽 P_s 变换 mesh-space 槽 AABB，覆盖换 Skin 和 helper 重排；静态 geometry snapshot/picking/highlight 返回 Unsupported 或明确 bounds-only。
- [ ] 4.8 用 CPU oracle 与 Vulkan/DX12 GPU readback 验证 bind pose、动画位置、normal/tangent、阴影和动态裁剪；覆盖共享 mesh 双实例。
- [x] 4.9 实现 Named/Indexed Skin 解析和 ResolvedSkinBinding 缓存键；验证缺骨/歧义、同数错序、错误参考姿势、slot 契约不符与同 rig UUID 不同 revision。
- [ ] 4.10 验证无动画 plugin 的 ReferencePose、SameActorAnimation 缺 provider 诊断、显式换 Skin 不显示伪新绑定，以及旧提交帧正常退休。
- [x] 4.11 按 D6a 接通 Vertex / Pipeline / Shader-local 三域变体（Material 是 S 输入方之一）；撤销 Skinned 源路径分叉和固定 1/2 usageVariant，明确 owner 并拒绝跨域覆盖；选择点取得冻结 draw/material/Pass/View 全部输入，同材质跨几何和 View 不互相污染。（assembler 一域一 owner、跨域拒绝；材质按全部 P 组合 × 当前 V 预编译并带 non-default Pipeline 选择键；`ResolveDraws` 用冻结 Tag/View 与 draw skinning 逐 draw 选择，同材质静态/蒙皮和不同 Pass 不互相污染。Pipeline 组合测试与 GPU 950 项通过。）
- [x] 4.12 统一三域组合的 runtime、usage、cache、cook、products-only 身份；覆盖各输入独立变化、组合变化、非法输入、颜色/阴影相同变形域、同帧共存与旧资源退休。shader 输入反射测试不能替代此项。（runtime/offline cook/products-only 按 SKINNED × SHADOWS × IBL 组合对齐，relocated 1/1 + 3/3 通过；resolver 接 `ShaderUsageStore`，`--aurora-shader-usage <path>` 开启并在 Shutdown flush；新增 P×V×S 8 组合矩阵、owner/非法输入、同材质按 Pass/vertex 选择、旧/新 revision 身份测试，现有 frozen-material hot-update 测试覆盖退休保留。原生平台验收由 9.3/9.4 跟踪。）

## 5. 角色与 Clip 预览

- [x] 5.1 扩展 EditorCore 预览 session 创建/更新/resize/关闭/status 契约及注册卸载，shell 复用 RendererHost 的独立 scene/view/output 与唯一 producer。
- [x] 5.2 Animation.Editor 建立独立 Preview World，通过注册/接口添加生产组件，不依赖 World adaptor 实现头或第二套 skinning。
- [ ] 5.3 增加独立 Mesh/Skin/Skeleton 选择、绑定诊断、Skeleton tree、bone transform、debug bounds 与播放 action，统一 UiTheme/UiSkin。
- [ ] 5.4 增加 Clip timeline、暂停 scrub、loop/rate；编辑配置用 ReflectedFormView，加载失败可见。
- [ ] 5.5 验证多工具窗、编辑 World/PIE/预览隔离、调整视口、关闭时在途输出退休，以及 Outliner/bounds 选择不冒充精确蒙皮拾取。
- [ ] 5.6 在 Aurora 编辑扩展注册 Skin 只读反射 DATA 摘要、owner reimport 入口，验证没有独立 .skin source 也可查看和选择。

## 6. Graph 资产与 cook

- [x] 6.1 实现版本化 Graph schema：稳定节点/参数/state/transition ID、skeleton UUID、typed pins、根输出、独立 editor layout。
- [x] 6.2 实现 Clip/Blend2/Output 与 bool/float 描述符、纯结构校验；区分 pose 环和合法 state 循环，拒绝共享有状态节点及嵌套/多状态机。
- [x] 6.3 实现规范化 Graph cooked 定义与序列化，包含控制数据、依赖及 node 诊断映射；往返后无需 source 运行。
- [x] 6.4 实现 plugin Graph builder 的 catalog 依赖校验/登记和失败不发布；不加载 Content.Adaptor、World 或 renderer。
- [ ] 6.5 在原 Content 文件移除空 Graph handler、`.graph` builder 分支/扩展和 view 注册；由 plugin 唯一接管，验证装载顺序及缺 plugin 时明确不可用。
- [x] 6.6 实现旧空 Graph source 的编辑迁移、旧产品 recook 诊断、source UUID 保持；覆盖无输出/坏节点/缺资产的负例。

## 7. Graph 实例与状态控制

- [x] 7.1 实现 Graph product 到资产请求到实例绑定，兼容性验证后构造每实例控制对象并 LowerToPlan；不在 tick 重新编译。
- [x] 7.2 实现 bool/float 参数快照、typed SetParameter、Blend2 权重和 source-node debug 映射；验证无效参数保持旧值。
- [x] 7.3 实现入口、AND 条件、显式 timeSourceNodeId 与不回绕 exit clock、priority/tie-break、tick 起点判定；拒绝 self-transition，inactive 不推进，reentry 重置。
- [x] 7.4 实现零时长切换及不可抢占 crossfade，两个 state 的活动范围和 Pose 混合；覆盖 T 到达边界、大 delta 与零 delta。
- [ ] 7.5 把 Graph 模式接入 AnimationComponent、热重载和 Preview World；失败保留旧完整版本并显示对应 revision。
- [ ] 7.6 验证双实例不同状态、计划与基础采样的 golden pose、稳定实例预分配复用、无 source 产品运行和参数/图错误诊断。
- [x] 7.7 验证 Graph Stop 重置入口/时钟但保留参数、Graph Seek Unsupported、Pause/rate=0 冻结转换、超长 delta 只完成一次过渡、Blend2 多时长 exit clock 与部分通道回填。

## 8. Graph 编辑器

- [x] 8.1 在 Sandbox widgets 实现无动画语义的 GraphCanvas：缩放/平移、节点/typed pin 命中、连线、框选和拖动；接口只暴露通用图描述。
- [x] 8.2 在 plugin 增 GraphDocument、命令及增删节点/连线/参数/状态/过渡；drag 为单次 undo transaction，复制生成新身份。
- [x] 8.3 节点属性和过渡设置通过描述符/反射对象接 ReflectedFormView；菜单/工具栏/快捷键经 ActionRegistry，不写逐类型 shell 表单。
- [x] 8.4 接入文档/AssetDataBase；允许保存 schema-valid 未完成草稿，原子写源一次、延后 RequestCook；执行语义错误只阻止 cook，不丢失编辑工作。
- [ ] 8.5 显示活动 state、transition progress、参数调试覆盖、node/pin 诊断与 preview revision；默认值编辑和临时调试值明确区分。（2026-10-25：原生 UI 已验证可读节点名、固定画布/属性页、人形自动取景及临时 Moving 参数切换 Walk、活动 Clip 高亮和 state/revision；node/pin 失败定位等剩余场景仍待验收。）
- [ ] 8.6 验证创建 Idle/Walk 图、连接、参数条件、undo/redo、保存重开、缺资产/cook 失败、关闭 dirty 文档和 UI dispatch 生命周期。（2026-10-25：补齐共享宿主的可见关闭确认，覆盖 Save/Discard/Cancel、保存失败、active edit 放弃、模态输入和多窗口隔离；原生窗口拖动后关闭、Esc 取消和保存关闭已验证。其余整图编辑场景仍待完整验收。）
- [ ] 8.7 首版 Graph 预览只从最后成功产品加载；验证未保存草稿的结构诊断、旧预览 revision 提示，以及运行时不读取 source。

## 9. 集成与验收

- [x] 9.1 更新打包/模块清单和指南，从 World 的 Mesh/Skin/Skeleton 显式根收集依赖与组清单；仅 products 环境验证 clip/graph/material/shader 解析，不依赖 Mesh -> rig 边。（`test_animation_package.py` 3/3：显式 roots、legacy 诊断、products-only 审计/损坏产品/类型错配；修正 AssetTool 脚本式参数缺 program slot；`docs/features/asset-pipeline.md` 增补 roots/audit 说明。）
- [x] 9.2 按仓库顺序关闭编辑器、必要时重配现有 cmake-build-debug、构建所有修改 targets；编辑器 UI 修改构建 SandboxModule，执行相关测试和静态检查。（全量 Debug 构建通过；Develop `ShaderCookTool`/`AssetTool` 重建；GPU 951 项 0 失败、Pipeline 48、Shader 49、Adaptor 48、core usage/cache/binding 22、Resource 37；仓库无独立 lint 命令，格式由 IDE clang-format 维护。）
- [ ] 9.3 Windows Vulkan/DX12 真实项目验收：添加双角色、播放不同动画、切参数、保存重开、重导入、reload、撤销删除、Preview/PIE 切换及退出。（已执行 `AnimationCharacter.skyproj` 有界运行：Vulkan/DX12 均 exit 0，RTX 3080 设备创建、Animation.World/Builder/Editor 模块装载；自动化覆盖沙盒世界 add/edit/undo/delete/reopen、skin defaults、graph 编辑/保存重开、preview session、products-only 与 GPU 蒙皮/阴影。逐项双角色 UI 播放、参数切换、重导入和 PIE 人工交互仍待执行，不能以自动化结果代替。）
- [ ] 9.4 执行 Metal 原生颜色/阴影/palette/退休验收；无设备时保留未勾选并明确平台限制，不用 Windows 或 mock 结果代替。（当前 Win32 环境无 Metal 设备；保留未勾选。）
- [ ] 9.5 验收每一份 spec 的场景，记录数据、GPU 和编辑器证据，删除临时诊断；关联 change 的遗留任务不冒领完成。（D6a/usage/打包/编辑器自动化证据已入 review.md；9.3/9.4 人工验收尚未收口；2.5 已在本地按批准的唯一 owner 方案完成。）
- [ ] 9.6 review 最终 diff、版本迁移和任务状态；提交前给出 message + 文件清单等待用户确认，archive 前另行展示状态并等待确认。（2026-10-25 反射阻塞已解决并保留严格测试；分批提交清单已准备，见 cmake-build-debug/cleanup-review-2026-10-25/commit-plan.md。仍需用户确认 commit/archive，不把剩余人工验收标为完成。）


## 2026-10-26 一致性核查

主 spec 已同步已有的 Mesh/Skin/Content/编辑预览契约，delta 已按当前主 spec 重设操作；这不等于归档或完成验收。逐项开放任务与代码/测试证据见 `docs/aurora/consistency-review-2026-10-26.md`。0.2/1.14/2.4/4.4–4.10/5.x/6.5/7.5–7.6/8.5–8.7 的复合验收仍保留原勾选状态，不以存在实现或相关测试代替完整场景证据。当前版本是 Mesh 6 / Skin 2 / Skeleton 1 / Material 4 / MaterialInstance 2 / shader cache 10 / usage 3；此前测试计数及版本说明是对应时点记录。
