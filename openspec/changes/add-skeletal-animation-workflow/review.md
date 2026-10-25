# Design review record

2026-10-24；包含 Godot 式绑定确认后的整体严格复核。下文“修正”均指设计、spec 与验收任务已更新，不表示产品实现完成。

## Architecture status

2026-10-25 整理后的当前状态：用户批准唯一 metadata owner 方案，已实现 Aurora.Metadata、Content.Metadata、Animation.GraphAssets 与进程 definition/runtime session 分离。重复反射入口及 Once/scratchNode 已移除；Framework 205/205 通过，保留模块失败重试只注册一次的严格预期。真实 cook/runtime/editor 双重载及连续 Graph 窗口创建 2/2 通过；此前反射阻塞已解除。详细契约与证据见 `refactor-process-owned-reflection`。

Shader 现按确认的 V/P/S 输入和 backend lowering 契约实施，详见 `refactor-aurora-shader-source-techniques`；旧变体偏离结论见下方历史 review，不代表当前仍在使用该实现。Godot 式 Mesh/Skin 独立、组件选择骨架实例保持不变。剩余验收仍包括双角色 World/PIE、生产阴影及 Metal 硬件；既有分层债务和这些未完成验收没有因本次 cleanup 自动消失。未 commit/archive。

以下保留 2026-10-24 起的设计发现与逐次修复记录；以本节和末尾最新验证为当前状态。

### Latest strict review: three variant input domains

- 依据：`openspec/specs/aurora-shader-cache/spec.md` 的“变体 key 组装”已要求 P | V | S；`ShaderVariantAssembler` 已有对应输入。当前生产 `ShaderTechnique.cpp::CompileStage` 仅取材质 variant 并以 `ContentHash()` 请求 shader，未调用 assembler。
- 本次新增偏差：`SceneAssetSource::MaterialPayload` 用 `Skinned` 文件名前缀及固定 1/2 usageVariant 区分几何；`Skinned*.slang` 直接定义 `AURORA_SKINNED`，没有按三域组合。既有提前编译边界不是本次偏离的理由，应一起修正触及调用链。
- 既有 assembler 的 `overrides` 同时覆盖 Pipeline 值和 Vertex 派生值，违反本轮明确的 owner 约束；不能只接上该函数就宣称通过。`ShaderUsage` 的 hash 重算与无源元数据也必须纳入回归。
- 设计复核遗漏了上述主 spec/生产调用链差距。先前任务 4.6 的勾选和独立 Skinned 编译证据不足，现已撤销其完成状态。未修改另一份未批准的 shader source techniques change。
- 独立有效的修复：奇数 U16 index count 上传补齐到四字节但保持实际绘制范围；实际 shader 顶点输入反射及 semantic/index 匹配。后者解决 NORMAL 位于 POSITION 与 JOINTS 之间时的 Vulkan 类型错配，属于输入布局验证，不是变体模型的替代品。
- 顶点布局回归已修正：手工 shader fixture 补齐 vertexInputs 反射；`HostSceneFixture` 的 mesh/material 从 2D 改为 float3 POSITION，以匹配生产 Unlit/Lit/Shadow 的顶点 ABI；编辑器宿主测试补上 `Aurora.Render` 静态 lease；Mesh 资源 revision 改回 mesh snapshot revision，避免材质 invalidate 改变 mesh 身份。`StructuredBuffer` 只读 palette 已支持，故拒绝用例改为 `RWStructuredBuffer`。
- 最新验证：`Aurora.Renderer.GpuSubmissionTest` 951 项（942 通过、9 项平台跳过、0 失败），其中 IndexedRaster 48 + VertexInputResolution 1 + MaterialVariantSelection 1；`Aurora.ShaderTest` 49/49、`Aurora.PipelineTest` 48/48、`Aurora.AdaptorTest` 48/48、usage/cache/binding 22/22、ResourceTest 37/37。重建 Develop `ShaderCookTool` 后 relocated products 用例 ShaderDistribution 1/1 + GPU 3/3 通过（index v6）。原生角色/阴影验收在三域修正后继续，尚未完成。
- 2026-10-24 D6a 首轮实现：`ShaderVariantAssembler` 改为一域一 owner，Pipeline/Material 不能写入 Vertex 派生 key，未知/重复/越界值显式失败；`CompileShaderTechnique` 接受已组合的宏与 specialization。删除 `SkinnedLit/SkinnedUnlit/SkinnedShadow` 源，Lit/Unlit/Shadow 通过 `SKINNED` 顶点变体区分静态/蒙皮；`SceneAssetSource` 用 assembler 组合变体，材质 `usageVariant` 改为布局指纹而非固定 1/2。SKINNED 0/1 的 runtime 与 offline cook/products-only hash 已对齐（relocated 用例 3/3，`ShaderDistribution` 1/1）。`Aurora.PipelineTest` 47/47（含 owner 分离、跨域拒绝、同材质跨几何/管线输入分离），全量 Debug 构建通过。
- 2026-10-24 D6a 第二轮：`SceneRenderFeature` 从冻结 View/Pass 计算非默认 `SHADOWS`/`IBL` 值；`SceneAssetSource` 为每个材质 technique 预编译 SKINNED × SHADOWS × IBL 组合并保存 non-default Pipeline 选择键；`ResolveDraws` 用冻结 Tag/View 与 draw skinning 逐 draw 选择对应布局。旧单程序材质保持 pipeline-agnostic。`resources.json` 枚举同一组合，`ShaderCookTool` 加载 pipeline schema 校验并 cook 全部组合；products-only 默认管线组合仍可命中。验证：GPU 951 项（942 通过、9 跳过、0 失败）、Pipeline 48/48、Shader 49/49、Adaptor 48/48、Core usage/cache/binding 22/22、relocated 1/1 + 3/3。仍缺：组合矩阵/非法输入/同帧共存/旧资源退休专项测试，以及原生角色/阴影和 Metal 验收。
- 2026-10-24 usage 与组合验证：`ShaderUsageStore` 作为跨 DLL Singleton 接入 `ShaderResolver`，记录请求中已组合的 `variantHash`/key words/dump；`AuroraModule --aurora-shader-usage <path>` 在 Init 开启、Shutdown flush。新增 `ShaderVariantAssemblerTest.PipelineVertexMaterialMatrix`（P×V×S 8 组 hash/位独立）和 `MaterialVariantSelection.SelectsExactPipelineAndVertexVariant`（同 material 多 Pass/vertex 布局选择、legacy pipeline-agnostic）。验证：assemble 9/9、`ShaderUsageTest` 5/5、GPU 951 项 0 失败；`MaterialVariantSelection` 另覆盖旧/新 revision 身份；全量 Debug 构建通过。4.12 勾选，原生项目/Metal 验收由 9.3/9.4 跟踪。
- 2026-10-25 打包验收：`test_animation_package.py` 3/3（World 显式 Mesh/Skin/Skeleton roots、legacy 诊断、products-only 闭包/损坏产品/类型错配）；修复 `AssetTool` 脚本式参数缺少 cxxopts program slot；`docs/features/asset-pipeline.md` 增补 roots/audit 指南。9.1/9.2 勾选。
- 2026-10-25 集成验收：`AnimationCharacter.skyproj` 有界运行 Vulkan/DX12 均 exit 0（RTX 3080，Animation.World/Builder/Editor 装载）。自动化通过：Framework 203、EditorCore 179、EditorShell 143、EditorRender 50、AnimationTest 64、Animation.Editor 6、Animation.World 10、Animation.Graph 14、Animation.Builder 4、Animation.Module 1、Content.Adaptor 7、Content.Builder 1、Cook 51、Renderer.Products 12、SandboxProject 13、Shader 49、Pipeline 48、Adaptor 48、GPU 951、package 3/3。逐项双角色 UI 播放/参数切换/重导入/PIE 与 Metal 仍待人工/平台验收；2.5 远端修复待 push 后 rebase。

## Strict review findings and disposition

本轮发现并修正 12 组设计问题（7 组 P1、5 组 P2）。P1 需要在对应实施阶段作为阻断验收项；OpenSpec 语法校验通过不能替代这些检查。

| 级别 / 边界 | 证据与触发条件 | 最小修正与验收位置 | 状态 |
|---|---|---|---|
| P1 / 资源归属 | 原 D6 仍要求 inverse bind 随 Mesh；`engine/aurora/core/include/aurora/resource/Mesh.h` 持单一 Skin。同 Mesh 两种绑定会互相覆盖或不得不复制几何 | D2/D6：独立 Skin，无必需 Skeleton UUID，组件显式三元组，Named/Indexed 解析；tasks 4.1/4.2/4.9 | 设计已修 |
| P1 / 原始导入 | `engine/aurora/cook/mesh/src/MeshSource.cpp:35` 按骨名保留第一份 offset，`:187` 先 LimitBoneWeights / OptimizeGraph；不同绑定会丢失，五权重错误不可检测 | D4：原始校验在破坏性预处理前，每绑定正规化，rig 与 slot 独立；tasks 1.2/1.4/1.12 | 设计已修 |
| P1 / 裁剪数据 | 原 D7 的 joint-local bounds 固定使用一套 IB；换 Skin 后仍共用此界会错误剔除 | D7：Mesh 保存 slot mesh-space AABB，各实例用 P_s 变换；tasks 4.7/4.8 | 设计已修 |
| P1 / 产品原子性 | `engine/framework/src/asset/AssetManager.cpp:499` SaveAsset 返回 void，`:517` 覆盖 UUID 文件，`:539` 提前标 LOADED；先存子产品不足以保住旧完整版本 | D4a / asset-product-loading：显式 Result、不可变组 generation、单清单提交、revision pin 与故障注入；tasks 1.6/1.14 | 设计已修，实施技术门 |
| P1 / 子资产寻址 | 现有 `engine/framework/src/asset/AssetDataBase.cpp` 注册的是源文件；新 Skin/Skeleton 没有独立源，Graph 存在性校验和按需 cook 不能直接套用源路径 | D4 / asset-source-identity：child type/owner catalog、owner cook 合并、child 终态通知；tasks 1.5/1.11 | 设计已修 |
| P1 / 热重载责任 | 原 D3 承诺 animation instance 同时验证 mesh/rig/clip/graph，违反动画不依赖渲染数据的边界；不同层先后 reload 会混版 | D3：Animation 只绑定 rig/clip/graph，Aurora 验证完整渲染元组并可冻结旧快照；tasks 3.3/3.5/4.4 | 设计已修 |
| P1 / 主规范冲突 | `openspec/specs/content/spec.md` 仍给 Content Graph 所有权；aurora-mesh、aurora-mesh-cook、aurora-adaptor 仍要求 Mesh 引用 Skin/Skeleton；editor-asset-viewer 仍要求 Skin 不打开 | 补齐相应 MODIFIED/RENAMED delta，保留未触及材质/静态行为；全量 MODIFIED 标题和合成后的 requirement 审计 | 设计已修 |
| P2 / Graph 控制 | 原 D9 没有说明 Blend2 状态用哪个 Clip 算 exit time；Graph loop/Seek/Stop、大 delta 和零 rate 不完整；`engine/animation/src/plan/AnimationPlanRuntime.cpp:93` 当前无条件推进 Clip bindings | D3/D9：显式 clock、tick 起点判定、活动范围、停止重置、Seek Unsupported、reference 通道回填；tasks 1.15/7.3/7.7 | 设计已修 |
| P2 / 文档与产品边界 | 原 D10 在 Save 前要求图可执行，会阻止保存未完成工作；草稿直接建预览与 asset-product-loading 的仅产品加载原则冲突 | D10：可保存 schema-valid 草稿，cook 独立失败，v1 只预览最后成功产品；tasks 8.4/8.7 | 设计已修 |
| P2 / 编辑扩展生命周期 | `engine/sandbox/core/include/editor/core/asset/AssetPreviewProvider.h:22` 只有 bool，不能表示两个预览的关闭/resize/在途所有权；动画编辑控制缺显式接口 | D1/D10：preview session + IAnimationInstanceControl，generation / lease / drain；tasks 2.6/5.1 | 设计已修 |
| P2 / 法线切线空间 | `engine/aurora/cook/mesh/src/MeshAssembler.cpp:74` 将切线也按 normalTransform 处理，非均匀变换结果错误 | D4 / aurora-mesh-cook：切线线性变换再正交化，镜像绕序/handedness 同步；task 1.3 | 设计已修 |
| P2 / 迁移承诺 | 现有每 cook 随机 Skeleton UUID，旧产品缺失时无法推导原 UUID；新 AuroraSkin 名称与历史删除类型相同不代表格式兼容 | Migration / aurora-cook：可证实时种入原身份，否则明确重绑；旧格式拒绝并 recook；tasks 1.5/1.13 | 设计已修 |

复核后的最关键实施顺序：先 generated catalog 与可靠产品发布，再 Skin / 槽空间正确性与单 Clip 生产渲染，最后 Graph 和 UI。不能先用一个真实角色“能动”绕过前面的失败、重载和共享实例场景。

## Evidence and corrections

| 边界/问题 | 当前证据 | 本设计最小纠正 |
|---|---|---|
| 生产 mesh 不支持 skin | `engine/aurora/adaptor/src/renderer/SceneAssetSource.cpp` 的 ConvertSceneMesh 明确 UNSUPPORTED | 扩展现有转换/packet/Pass，保留静态路径行为 |
| Actor 组件缺失 | `engine/aurora/adaptor/include/aurora/adaptor/components` 只有 StaticMesh 等；Animation 无 World 接入 | Aurora 增 SkeletalMeshComponent，plugin 增 AnimationComponent/system |
| Graph 不含图 | `engine/content/adaptor/include/content/adaptor/assets/AnimationAsset.h` 的 AnimationAssetData 只有 version；builder RequestGraph 只存 version | Graph 文档、控制定义、依赖、实例化与唯一注册 owner |
| 预览非渲染实现 | `engine/sandbox/shell/src/AssetViewerWidget.cpp` 显示 preview reserved；IAssetPreviewProvider 只有 bool 查询 | 扩展共享宿主实际 scene/view/output 生命周期 |
| Skin 混合共享/实例数据 | `engine/aurora/core/include/aurora/resource/Skin.h` 同持 inverseBind 与 boneMatrices | 绑定不可变，palette 每实例/每帧独立 |
| Plan 不是天然可共享资源 | `engine/animation/include/animation/plan/AnimationPlan.h` 有 mutable posePool，Runtime bindings 含 node pointers | 不可变 Graph 定义共享，Plan 与 control 对象每实例独立 |
| Root Motion 语义有限 | `engine/animation/src/plan/AnimationPlan.cpp` 用 bind inverse * sampled，按 ClipSample 写同一 output | 首版组件不应用角色位移，不把它宣传成跨帧混合 delta |
| 多状态机不成立 | `AnimationPlan.h` 只有 activeState，Runtime 循环覆盖该字段 | v1 只支持顶层单层状态机，显式拒绝嵌套，不提前宣称通用图 |
| 稀疏时长需纠正表述 | `AnimationClip.cpp::GetFrameCount` 已按最后 key，而非加载器的 key count；Compress 会删除尾部重复 key | 保存压缩前权威长度；不是简单把 key count 改成最后 key |
| 骨架导入不完整 | `MeshSource.cpp` 仅保留 boneTable 内节点；MeshAssembler 烘焙节点变换，IB 未对应统一 | 祖先闭包、空间公式、bind 恒等检查、明确 unsupported |
| 子资产身份不稳定 | `AuroraMeshBuilder.cpp` 每 cook Uuid::Create | manifest 子产品持久映射，失败不发布半套依赖 |
| 索引边界 | MeshAssembler JOINTS uint8 打包，AnimationTypes ANIM_MAX_BONE=256 | v1 明确 256 上限，校验后编码 |
| 历史异常测试 | `AnimationPlanTest.cpp` 分配计数 override 包含 throw bad_alloc | 不照搬，若触及先提出符合规则的测试钩子方案并确认 |

## Specification reconciliation

- 代码与已归档 editor-asset-views 将 Skin 合入 Mesh v5；用户现已明确选择 Godot 方式，因此本次有意重新引入独立 AuroraSkin，但实例而非 Mesh 持绑定，且不恢复历史可变 palette / 格式。同步修改 aurora-cook、aurora-mesh-cook、aurora-adaptor，不能仅沿用旧主 spec 的 Skin UUID 字段。
- `aurora-skinning` 明确让 Skin 持 boneMatrices；改为实例调色板属于真实行为变更，提供 MODIFIED delta，不只在设计里自相矛盾。
- `aurora-skinning` 的 SetSkin/GetSkin/HasSkin 与 `aurora-mesh` 的旧 Skeleton 引用均经 RENAMED + MODIFIED 明确移除；JOINTS 指向独立输入槽。
- `content` 的旧 Graph handler/builder 条文同步改为 Clip-only；新 animation-graph-assets 不是对主规范的隐式覆盖。
- `editor-asset-viewer` 的 placeholder-only seam 与 Skin 一律隐藏场景改成 session / 反射 DATA 摘要；没有强制新增 `.skin` 源编辑器。
- `asset-source-identity` / `asset-product-loading` 的新增行为显式给出 delta；运行与打包不读取源文件，generated catalog 只提供身份/owner 元数据。
- `animation` 的 duration requirement 使用最后 key。显式 sourceFrameCount 改变优先级，保留无元数据回退场景并补压缩不变场景。
- `animation` 已有 ACL 能力条文；本 change 不删除它，也不把独立 API 已存在等同于 Content/Plan 已使用 ACL。
- 主 spec 与 archive 不在本轮直接修改；只写当前 change delta。
- 未触及的旧 meshlet/preset 历史描述仍是已有规范债务，不恢复代码已删除的 meshlet 功能，也不扩张本次为全仓 spec 清理。

## Related change ownership

| 真实存在的 change | 本次关系 |
|---|---|
| `aurora-scene-bridge` | 复用已经实现的唯一 producer、身份/退休协议；其未完成 clang-tidy 项仍归原 change |
| `aurora-world-editor` | 复用 World/Outliner/Inspector 与提取时点；动画对象和 Preview World 是本次扩展 |
| `refactor-aurora-pipeline-resource-ownership` | 复用 Tag/Pass/resource ABI 与资源注册；仅补 skin Batch 输入，不接管其原生验收 |
| `refactor-aurora-shader-source-techniques` | 当前设计明确待重新评估；不是本次前置。不依赖未落地 source attribute/variant 语法，按实施时已批准的材质接口接入 |
| `aurora-cook-schema-layering` | 保持为既有 debt record，不以它为已完成前置；本次不新增 Graph builder -> adaptor 依赖 |
| `animation-events` | Notify/marker/状态事件仍归其所有，不在本次另建事件分发器；Graph UI 首版无事件节点 |
| `audio-animation-bridge` | 保持事件到音频桥独立；本次不加入音频/骨骼挂点实现 |

这些 change 全部已核实存在。模型动画 take 导出、完整 Root Motion、骨骼挂点、精确蒙皮拾取/轮廓尚无本轮创建的 follow-up change；条件为本次空间/资产/Pose 契约稳定后另行起完整设计，不引用虚构 change 名。

## Review checklist

- 生命周期：请求、实例、World、packet、GPU lease 各有代际/所有权，取消不等于已收齐。
- 架构：Aurora 不依赖动画 plugin；接口在消费方；Graph builder 不加载 renderer/World。
- 正确性：同 Mesh 不同 Skin/rig、同 Skin 不同 Pose 均隔离；重导入可验证身份稳定，颜色/阴影/bounds 同一 Pose/binding revision。
- 编辑：Graph 草稿/已保存 source/cooked product/运行预览版本分开展示，失败不伪装成功。
- 可测试：spec 场景与 tasks 覆盖矩阵一致；设计阶段只验证文档结构和引用，实施进度及测试证据另列如下。

## Historical design-stage validation

`openspec validate add-skeletal-animation-workflow --strict` 通过；status 的 proposal/design/specs/tasks 均 complete，仅表示 artifacts 齐全。共 13 份 capability delta、57 条 requirement、133 个 scenario、72 项未勾选实施任务。额外审计 MODIFIED/RENAMED 标题与主 spec、proposal capability 覆盖、任务编号、相对链接和行尾空白，结果无错误。没有修改产品代码、运行本 change 的 C++ 构建/GPU 验收、commit 或 archive。

## Implementation evidence (2026-10-24)

- 实施并通过测试：Skeleton 外部数据校验与布局签名，Clip v3 压缩前时长、源 v2 兼容及坏产品诊断，虚图/Plan 播放控制，部分通道参考姿势回填，非均匀缩放下的模型矩阵。
- 独立不可变 Skin、Named/Indexed 解析与参考姿势校验已实现；Mesh 资源不再持 Skin，实例各自拥有 palette。AuroraSkin v2 handler、二进制往返及旧格式拒绝已测试。精确 generation/revision 缓存已验证；Mesh v6、模型 Skin/Skeleton 子产品和原子组发布已接入。生产 renderer 的 palette 尚未接入。
- 本轮 Debug 重编译测试：AnimationTest 63、Content.AdaptorTest 6、Aurora.CoreTest 筛选 9、Aurora.AdaptorTest 筛选 24、Aurora.CookTest MeshCook 筛选 9，合计 111 项通过。证据在 `cmake-build-debug/animation-final-*.xml` / `.log`。
- Framework 产品组 staging/完整成员校验/原子单清单发布、请求 ticket、跨进程锁和 pinned snapshot 已实现。NativeFileSystem 的原子替换不再先删目标。Framework 相关 48 项测试通过，覆盖子写入/读回/flush/commit 失败、强制结束写进程、陈旧请求、首次迁移失败、旧 bulk、异步依赖 barrier 与原有资产/索引回归。证据：`cmake-build-debug/animation-products-tests.xml`。
- Clip builder 已使用新 build asset 和显式产品组保存结果；端到端测试 1 项通过，覆盖两次 recook 的不同姿势、旧 snapshot 保持、压缩和发布失败。证据：`cmake-build-debug/animation-clip-builder-tests.xml`。Graph 接入仍未完成，task 1.14 不勾选。
- SandboxModule 曾因运行中的 SandboxEditor 锁文件而链接失败；用户已关闭编辑器，之后整体模块重链成功（`animation-mesh-runtime-build.log`），覆盖 cook、Content、Aurora runtime/editor、SandboxModule 和 SandboxEditor。无 GPU、真实项目或 Metal 验收证据。未 commit / archive。

- 新一轮资产回归：Framework 62、EditorAssetCatalog 8、Content.BuilderTest 1、Aurora Mesh cook 16、Aurora adaptor 23 项通过。证据为 `cmake-build-debug/animation-mesh-{framework,picker,clip,adaptor}-tests.xml` 与 `animation-mesh-tests.xml`。Framework 包含真实 worker 请求关联、迟到回调、缺失 child、原生产品组故障和只读 manifest 复用；adaptor 使用实际 suite 名称覆盖 SkeletonAsset、SkinAsset、SceneBridgeTest、SceneMeshConversion。
- Mesh v6 只保存 geometry、Skin 输入槽/模型空间签名和 mesh-space 槽 bounds；不含 IB bulk 或 Skeleton UUID。每个原始绑定先正规化 IB，保留 helper 祖先，拒绝第五个正 influence、负/零/非有限权重和错误参考姿势。非均匀/镜像切线使用线性变换、正交化与绕序修正；optimizer 按 submesh 重排后重验，避免跨材质交换三角形。
- 真实 `two-bindings.gltf` 验证同一几何对应两份不同 IB 的 Skin/rig，发布结果为一份 Mesh + 两份 Skin + 两份 Skeleton；recook UUID 稳定，多个 bundle generation 隔离，commit 失败保留旧完整产品。五权重和各类错误输入以真实 glTF 经过导入入口验证，不再依赖手工构造 Assimp scene。
- 独立 `.skeleton` builder 同样使用新快照与单成员产品组；测试验证参考姿势改变、失败校验及失败发布不修改已经加载的旧 rig。Clip/Plan 部分通道切换的 10 项 AnimationValidation 测试与 10 项 SkinBinding/MeshResource 测试已通过（`animation-reference-refill-tests.xml`、`animation-binding-cache-tests.xml`）。
- 尚未完成：旧随机 Skeleton UUID 的可核验迁移、Graph 的产品/依赖接管、World 组件、生产 GPU skinning、共享预览与 Graph 编辑器。对应 tasks 保持未勾选；本次不是完整 feature 验收。

## Design-stage implementation gates (see later evidence for current progress)

- D1 新 targets / 装载入口与首版范围已获 2026-10-24 `apply` 确认；后续变更该方案时再按根 AGENTS.md 说明。提交与 archive 仍为独立确认点。
- 产品组 staging 的 native IO 错误、原子替换、进程恢复、跨进程同 owner/bundle 排序和旧版本生命周期须由 Framework 故障注入证明。
- Assimp 必须能保留 fixture 的原始 mesh-binding/rig 关联；不能恢复的源明确拒绝，不能宣称任意 glTF/FBX 多 rig 场景都已支持。
- palette 的实际 structured-buffer ABI、GPU normal/tangent 与颜色/阴影一致性须 Vulkan/DX12 原生验证；Metal 实机项保持独立。
- 既有 Aurora cook schema 分层债务继续记录在 `aurora-cook-schema-layering`，不迁移目录或借此让新业务 builder 链 adaptor。设计不会把这项历史风险伪装为已解决。


### 2026-10-24 Clip provider / World implementation evidence

- Content/Aurora publish consumer-owned Clip/Skeleton source interfaces through AssetManager. Requests and ready snapshots retain exact product generation/revision and real ModuleManager leases; replacement keeps destructor code alive.
- Animation.World is registered as an independent plugin adaptor. Clip instances own players/poses/matrices; World ticks finish evaluation before pose extraction. Main-thread pose/control interfaces reject stale World/Actor generations and exact rig-version mismatches. Graph mode currently reports Unsupported pending its implementation.
- Component config is reflected and serializable; runtime time/pose/generation are excluded. Edit Worlds do not advance playback. KeepInPose/LockToReference never write Actor transforms.
- Product publication serials trigger candidate requests without worker waits. Same-binding failure retains an explicitly marked previous snapshot; explicit binding changes reject the previous generation immediately. World retains external module leases through virtual subsystem destruction.
- Verified: Content provider/asset tests 7; Aurora provider/Skeleton/Skin tests 5; Animation.WorldTest 7; Framework World/component/product/catalog regression 37. Animation.ModuleTest loads real Content.Module, Aurora.Render (provider-only), Animation.World DLLs, unregisters services, destroys ModuleManager, then polls retained requests and destroys the drained World successfully.
- Remaining: Graph schema/runtime/editor, production skeletal rendering and GPU validation, preview sessions, packaging and migration checks. Provider-only module testing does not validate GPU rendering or editor interaction.


### 2026-10-24 Graph definition / asset implementation evidence

- Graph core and Framework asset targets are separate. Stable document/node/parameter/state/transition IDs, typed conditions, explicit exit clocks, disjoint state pose trees and separate editor layout are validated. Incomplete schema-valid drafts can be serialized; executable validation remains stricter.
- Version-2 cooked Graph retains normalized definitions, topology, source-node IDs and dependencies without editor layout. Bounded parsing rejects legacy/truncated/inconsistent products; old empty JSON sources become editable empty drafts with the source identity supplied by the caller.
- Animation.Builder validates generated-child catalog types and publishes one product group. Wrong-type/missing dependencies and failed commits preserve the previous group. AssetTool registers the builder when its target is present. Content no longer owns the empty Graph handler/builder/view.
- Verified Animation.GraphTest 7/7, Animation.BuilderTest 4/4, Content.BuilderTest 1/1 and Animation.ModuleTest 1/1. Built Animation.Builder, Content.Views and AssetTool. Graph instance evaluation and Graph editing UI are not yet implemented.

## 2026-10-24 Graph 实例与 World 接入验证

GraphInstance 复用 AnimationPlan/AnimationClipNode，按实例持参数快照、每状态 Plan 和时钟；tick 起点判定、非回绕 exit clock、不可抢占 crossfade、Stop/Seek/Pause 边界已有回归。World 从 Graph 产品请求全部 Clip，完整求值后原子发布；同绑定 reload 保留同 ID 同类型参数（状态/时钟回到入口），移除的参数忽略，类型不兼容保留旧完整版本并诊断。组件 loop 只影响 Clip 模式。

验证：Animation.GraphTest 14/14（animation-graph-runtime-tests.xml）；Animation.WorldTest 10/10（animation-graph-world-tests.xml）；真实 DLL Animation.ModuleTest 1/1（animation-graph-module-tests.xml）。World Graph 测试使用真实 Graph 产品组和 fake Clip/rig provider；产品依赖打包另由 Builder 测试覆盖，完整 products-only 角色验收仍未完成。Graph 编辑器和 Preview World 尚未接入，7.5/7.6 保持未完成。

### 2026-10-24 — 生产蒙皮传输与资产元组验证

- SkinningBatch 采用只读 StructuredBuffer<float4>，每槽按 P / inverse-transpose(P) 两矩阵上传；沿原 frame lease 分配 storage arena、显式图访问、原生 descriptor 和完成退休，不复用可变 Mesh 数据。整数 U_RGBA8 JOINTS 已贯通验证与三后端格式映射。
- Windows RTX 3080：GPU readback 3/3（Vulkan dynamic/render-pass、DX12）验证同几何双实例、镜像 normal、tangent、两帧在途及录制后写入拒绝；IndexedRaster 全套 48/48。共享 Pass / 内置颜色与阴影 shader 的 SPIR-V、MSL、DXIL 编译与反射 12/12。Metal 仅交叉编译，原生 9.4 仍未完成。
- 修复 GPU 测试揭示的 DX12 storage descriptor 未计入堆分配，以及 MSL structured buffer 同时暴露 buffer/texture 类别造成的反射漏项。新增内置 Skinned shader 的 schema sidecar，实际产品入口可解析。
- SceneAssetSource 在发布前校验材质 palette 契约、顶点有限值及同 owner 产品组 generation；跨提交点的混合组不发布，下次重取，已发布旧元组仍由 scene bridge 保持。生产资源源测试与 bridge 回归 17/17（含无 Animation provider 的 ReferencePose、静态材质拒绝、混合组重试）。
- CPU prepare 保留 owned pose、使用其动态 bounds，并禁止 skinned draw 使用其它版本的 fallback mesh/material；ResourceTest 37/37。完整角色项目、生产阴影 GPU 对照、预览与 Graph UI 尚未完成，相关验收任务仍未勾选。


### 2026-10-24 — Graph 编辑与共享预览 host

- 通用 GraphCanvas 已实现 typed pin 命中/连接、框选、拖动单次提交、缩放和平移；GraphDocument 提供稳定 ID 的增删/复制/属性命令、草稿校验与原子保存。编辑器属性通过 ReflectedFormView 生成；文档内节点/状态/参数引用复用注入局部 catalog 的选择控件。
- Animation.Editor 接入通用资产文档会话。Save 在 tick 执行，成功后才排队 cook；关闭和切换 dirty 文档提供 Save/Discard/Cancel，旧控件、文档、模块 lease 按顺序释放。失败加载不能覆盖源；失败保存保留 dirty。
- 已验证 GraphCanvas 5/5、Animation.EditorTest 4/4、资产窗口/历史路由 6/6。EditorRenderTest 50/50 含独立预览 scene/view/output、resize、陈旧 generation 拒绝及三项退休回执之后释放 World/lease。构建 Animation.Editor、SandboxModule、SandboxEditor 成功。
- Preview World、反射选择 Mesh/Skin/Skeleton、骨骼树/transform、timeline 和临时参数覆盖已接入，正在进行真实 DLL 与 UI 集成验证；5.2–5.5、7.5、8.5–8.7 保留未勾选。当前标准 asset viewer 仍是单窗口，多工具窗口验收尚未完成。

### 2026-10-24 — 多文档窗口、独立预览与组件默认绑定

- Animation.Editor 的生产 Preview World 通过 ComponentFactory/反射创建组件，通过消费方接口控制实例；真实 DLL 测试覆盖 provider 注册/撤销、参数表刷新后的 Undo/Redo、action snapshot 与 World 的模块 lease。预览只读成功产品，缺产品不触发 source cook。ReferencePose 禁用播放，合法设置和 Reload 可恢复错误状态。
- 每份 Graph 使用独立原生工具窗口、UI context、action scope、文档历史和 Preview World。修复真实窗口发现的快捷键提前返回：Ctrl+S/Z/Y 先按当前 surface scope 分发。native close 延后到文档决定及 renderer 退休之后。Shell 对双文档、保存失败、Cancel 和 action 隔离的回归 8/8；Animation.EditorTest 6/6；真实模块测试 1/1。
- Windows Vulkan 原生 UI 已验证：打开两份 Graph，第一份拖动 Clip，第二份新增 Output，各自 Ctrl+Z/Y/S，第二窗口关闭 dirty 后 Cancel 再保存关闭，第一份历史与内容仍保持；保存草稿重开保持节点身份。测试项目在 ignored build 目录。尚未完成完整 Idle/Walk 连线播放、双角色世界/PIE 与生产阴影验收，不能由这次 UI 测试替代。
- 无 editor 配置独立编译并通过 Animation.ModuleTest 1/1；再关闭动画插件并编译 Aurora.AdaptorTest，ReferencePose/生产材质/混合 generation/静态场景等 17/17 通过。实际模块清单与依赖图的最终审计仍待完成。证据：`cmake-build-debug/animation-noeditor/animation-{noeditor-module,plugin-off}-tests.xml`。
- 选择 SkeletalMeshComponent 的 Mesh 时，仅在 Skin/Skeleton 都为空且导入默认组合唯一时写入三项显式 UUID，并作为一次 undo command；保留用户已有绑定，多绑定/缺项拒绝自动选择。预览复用同一纯解析函数。World 编辑、删除撤销、保存重开与静态组件回归 4/4，绑定解析/缓存/CPU bounds 回归 8/8，编辑器 6/6。证据：`animation-component-defaults-tests.xml`、`animation-imported-binding-tests.xml`、`animation-editor-defaults-tests.xml`。
- 尚待：旧随机 Skeleton 身份迁移、World 显式资产根打包与完整 products-only 角色验收、原生生产阴影/完整项目与 PIE，以及 Metal 硬件验收。未 commit / archive。

### 2026-10-24 — Mesh v5 身份迁移验证

- AssetManager 新增离线产品头检查，读取指定 bundle 的已提交产品/组快照，复用有界 envelope/解压校验；不改缓存、不加载依赖、不调用 source/cook。输出头保有自己的数据，在后续 recook 后仍可读。
- 首次生成 child catalog 时检查 Mesh v5 的 section 范围、旧 Skeleton 引用及依赖，并读取旧 Skeleton 产品。仅当骨骼数量、名称/顺序、parent 与参考矩阵在新输入中唯一对应时，原子写入已验证旧 UUID；名称/路径或 helper 布局已变化时不猜测。已有 catalog 禁止自动改身份；源/其它 child 的 UUID 冲突明确拒绝。
- 实际旧 v5 envelope + glTF 重导入测试覆盖准确匹配、参考姿势不符、旧 rig 丢失、Mesh 头截断及再次 recook；源 UUID 保留，只有第一种保留旧 rig UUID。Aurora cook 9/9，Framework catalog/product-group 故障回归 19/19 通过。证据：`animation-legacy-migration-tests.xml`、`animation-migration-products-tests.xml`。
- 编辑器预览的 renderer 资产源也改为 ProductsOnly，覆盖 Mesh/Skin/Material 间接依赖；普通 World 的按需 cook 策略保留。完整 products-only 项目打包与图形验收仍待后续证据。

### 2026-10-25 — 修改后的 Graph 关闭确认

- 根因：dirty 文档进入等待决定状态，但 Save/Discard/Cancel 只在 File 菜单与状态行提示中暴露。共享 AssetViewer 现显示模态确认，保存失败保持打开，活动编辑可显式放弃，输入不透传底层文档；关闭与替换仍延后到 tick。
- EditorShellTest 全量 143/143 通过；补充保存中禁用与内容区指针断言后，关闭/多窗口回归 2/2 通过。SandboxModule/SandboxEditor 构建成功。
- 原生 UI 使用临时 Graph 副本：拖动节点、原生 X 触发确认、Esc 返回保留修改、再次关闭并 Save and Close，源文件 hash 变化且 Graph 窗口退休，主窗口保持运行。临时副本已移除，原 Locomotion.graph hash 未变。未 commit/archive。

### 2026-10-25 — Graph 可读性与人形预览验收

- Graph 内容适配宿主可用视口，画布与一个独立滚动的属性页并列保留；Graph / Selected node / Preview 页签复用 scoped actions 与 ToolBar。Mesh、Skin、Skeleton 排在预览配置首部。Clip/Output 节点显示资产名/状态名；修正节点尺寸、位置、绘制、命中之间的 DPI 重复缩放，并提供 Frame all。
- 补齐已有 Content.Builder.Module 的编辑器装载，以及命令行项目在 builder 注册后的目录刷新。Preview 继续只读成功产品；缺资源/加载中/失败在视口内提示。选择唯一导入绑定作为一次可撤销编辑；自动聚焦等待 Mesh bounds，并适配窄视口，不被先加载的 Skeleton 提前结束。
- EditorWidgetsTest 40/40、EditorShellTest 143/143、Animation.EditorTest 8/8 通过。新增回归覆盖 DPI 坐标/命中/Frame all、视口内属性分页、延迟 bounds 与窄视口取景。证据：`cmake-build-debug/graph-preview-{widgets,shell,editor}-tests.xml`。构建 SandboxModule、SandboxEditor、Animation.Editor、AssetTool 成功；Assimp 目标 DLL 被残留进程占用但与源 SHA256 一致，本次构建跳过重复 post-build 复制后单独部署资源和配置，未修改构建定义。
- Windows 原生 UI 实测：Skeleton 单独预览完整显示；选择 Character.gltf 自动填入 Skin，加载状态可见，Mesh 完成后自动完整取景；拖动旋转有效；Play 后临时 Moving=true 切到 Walk，活动 Clip 高亮、状态名和蒙皮/骨骼动作一致；选中 Walk 节点自动切到该 Clip 的反射属性，Graph 源仍为 Saved；播放期间关闭窗口正常。
- ignored 演示工程中的退化三角形替换为本地生成的简单人形（13 个 skin joints，含 Root 为 14 骨），Idle/Walk 各 5 个旋转通道；原三角形和 Clip 已保存在 `animation-character-project/original-triangle-fixture/`，生成脚本为 `cmake-build-debug/create_preview_mannequin.py`，Graph 源未改。AssetTool cook-all 报告的唯一失败为既有 `main.world` 无 builder，Mesh/Skin/Skeleton/Clip/Graph 产品成功；本次不据此宣称完整工程 cook 或 9.3 全部通过。
- 本次补齐预览可见性与布局验收，不替代缺资产/坏 cook 保留旧版本、多实例 World/PIE、生产阴影和 Metal 硬件等剩余验收。未 commit/archive。

### 2026-10-25 — 唯一反射 owner 和代码整理

- 新 metadata targets 由用户明确确认。runtime/cook/editor 依赖唯一 owner；窗口只 Bind；类型 UUID/名称和组件注册恢复严格检查。新/旧 manager、失败 Init 重试均复用 process definition，代码 lease 覆盖 metadata、handler 和 builder；共享 DLL 不会因某个 session 结束而提前 Detach。
- Framework 205、Animation Graph/World/Builder/Editor/Module 共 38、Content 8、EditorCore/Widgets/Shell/SandboxProject 共 375、Aurora Adaptor/Cook 共 118 项通过。原 metadata retry 失败没有通过修改预期绕过。
- Renderer 动态/静态生命周期各 6/7 项通过，包括 compiler future 的最终 DLL 释放。普通运行各两项 products 测试因环境未提供而跳过；随后实际 cook 并剥离源码/编译器，Core 1、Vulkan 2、DX12 2 全部通过，补齐动态发布路径验证。
- shader_headers/shader_project 各 10 项、动画 products-only 打包审计 3 项通过。动画演示工程有界运行 8 帧，实际日志确认全部模块和编辑器 shell 初始化成功。
- 本轮清理还合并材质 S 选项转换与生成资产 catalog 冲突检查；Framework 资产测试改用 build 目录 fixture，避免写脏 source assets.db。测试在 executable main 或 gtest Environment 注册类型，无 Once/static bool/存在检查吞重。
- 日志位于 ignored `cmake-build-debug/cleanup-review-2026-10-25/`；本节只覆盖列明范围，不冒领此前未完成的人工/Metal 验收。

### 2026-10-26 — 最后调用点与 GPU 回归收口

- Reflection Demo 的数据/注册归现有 EditorCore，由 SandboxModule 唯一拥有；窗口只 Bind。NetworkWorld 移除 static bool 反射守卫，Bullet fixture 移到测试入口。编辑器 376/376、NetworkWorld 5/5、Bullet 8/8、Animation.ModuleTest 2/2 通过；真实动画工程再次有界启动成功。
- Windows GPU 177 项首轮 174 通过，3 个后端的静态拾取断言失败。诊断确认最终 Tick 已将 scene revision 从 9 更新到 10；辅助等待函数超过期限后未再检查完成条件。修复辅助函数后保持全部拾取断言，3/3 定向复测通过。不得描述为完整 GPU suite 单次全绿。
- 全项目 Debug 增量构建、随后 GPU 测试定向重编、四个 change 的 strict 校验和新增代码规则扫描通过。提交清单见 ignored `cmake-build-debug/cleanup-review-2026-10-25/commit-plan.md`；暂存区保持原样，未 commit/archive。已有人工、Metal 等开放验收不变。
