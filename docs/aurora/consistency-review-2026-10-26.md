# 代码、规范与验收记录一致性复核

日期：2026-10-26。状态：已修正本轮确认的过期契约；未 commit、未 archive，未改变原暂存区。

此前“文档和规范已同步”的结论不成立。上一轮主要核对改动文件及 strict 格式，没有完整检查主 spec、其他 active change 的覆盖关系和实际合并结果。此次纠正该结论，不把格式通过、代码存在或旧平台结果当作完整验收。

## 核查范围与方法

- 对 `docs/`、全部主 spec、37 个非 archive change 和 `.opencode/skills/` 做全量文件/契约检索；首轮 395 份文档，补充 delta 后 400 份、42,360 行。这个数量是检索范围，不表示逐行代码证明。
- 对反射 owner、Skin/骨架/Graph、四组资源、typed V/P/S、Material schema、目标 ABI、缓存/发布、编辑预览及其模块装载链逐条对照当前声明、实现、相关测试和任务状态。
- 为全部当前改动测试建立索引：126 个测试相关路径、882 条 C++/Python 测试声明；核对已有 28 份 XML 的实际结果。没有把索引数量当作本轮重跑或全部场景覆盖数量。
- 使用 OpenSpec 自带的 `findSpecUpdates/buildUpdatedSpec` 只读生成合并结果，未调用 archive 或写主 spec 的 CLI 操作。主 spec 的定向编辑与 delta 重设作为可审阅的本地修改保留。
- archived changes 保留历史。早期设计/测试记录明确标注已被后续契约接替；现行规范和待实施方案不再继续要求相反行为。

本地完整盘点、逐文件修改备份、测试索引和校验日志在 `cmake-build-debug/cleanup-review-2026-10-25/`：`full-consistency-inventory.json`、`full-consistency-candidates.json`、`consistency-edited-files.json`、`testcase-consistency-inventory.json`、`before-consistency/`、`spec-merge-preview.json`。这些验证材料不进入功能提交。

## 已修正的契约

| 问题 | 当前契约与代码依据 | 同步范围 |
|---|---|---|
| AuroraReflection 幂等、多次调用安全；Cook Init 再注册；共享 Once | `ModuleManager.cpp` 的 process definition、`AuroraMetadata.cpp`、`ContentMetadata.cpp` 是唯一入口；runtime/session/window 只消费；严格 UUID/name 检查及代码 lease 保留 | aurora-adaptor、aurora-cook、content 主 spec；process-owned-reflection delta；规则仍禁止 Once/once_flag/查表吞重 |
| Content 仍拥有空 Graph 资产、handler、builder | Content 仅负责 Clip；`plugins/animation` 的 GraphAssets/Builder/Editor 唯一负责 Graph | content 主 spec、动画 delta、metadata 依赖契约 |
| Mesh 持有 Skin/Skeleton，Skin 持可变 boneMatrices | `Mesh.h` 只有几何/influence；`Skin.h` 是不可变 inverse-bind 绑定；SkeletalMeshComponent 显式选择 Mesh/Skin/Skeleton；palette 属于实例/帧 | aurora-mesh、aurora-mesh-cook、aurora-skinning、aurora-adaptor、aurora-cook 及对应 delta |
| Global/Pass/Batch = 0/1/2，UI/SceneSurface 仍被写作 set 2 | Global 0、Pass 1、Material 2、Batch 3；SceneSurface/palette/UI 逐 draw 资源归 Batch；View 归 Pass | shader/resource-binding/rhi-core/rdg/ui-render 主 spec；共享头、pipeline ownership、lighting、compute、platform parity 等相关 active change |
| `.h` 由 Slang 直接消费、CMake 嵌入源码，CPU offset 必须等于 GPU | 权威 `.h` 给 C++，持久 `.slangh` 给 shader；CPU schema 经 UniformUploadPlan 编码到所选 target ABI | aurora-shader/resource-binding 主 spec；shared-pass-headers、pipeline ownership、target artifacts 的 delta/design |
| 所有变体受 128-bit 限制；只存 uint32；Material 等同第三域 | typed V/P/S；Material/custom Batch/fullscreen/compute 提供 S；完整选择独立于保留的 packed helper，stage 投影与 native lowering 独立 | shader/shader-cache 主 spec、source-techniques delta、ShaderVariant.h 注释 |
| 未完成 Slang change 再引入调用方 macro/runtime 分类和 DX12 root constants | 撤回该旧方案；仅保留 compiler pool/session/module 复用及 storage 校验后续工作；消费已确认的 typed V/P/S 和 target lowering | refactor-aurora-slang-usage 的 proposal/design/tasks/两个 delta；原 61 项勾选状态保持 |
| Material 仍是单 ShaderRef、裸字节属性和手工映射；实例以值不同判断覆盖 | 显式 stage programs；完整 Material group 声明、唯一可选 cbuffer 字段和 S 选项生成 Key；canonical typed values；旧 aliases 受检迁移；显式等值覆盖仍是覆盖 | aurora-material/adaptor/cook 主 spec，source-techniques/target-artifacts delta，shader migration 文档 |
| 无源码仅检查 blob 存在，允许原子 rename 失败后直接覆盖 | DEVELOPMENT/PRODUCTS 明确分离；完整 selection、environment、producer、ArtifactID 校验；原子发布失败返回错误 | aurora-shader-cache 主 spec、target artifacts/source techniques delta |
| Metal 由资源顺序猜槽位、空 entry 猜名、数组 binding+element、动态 offset 不足时退回 | artifact nativeEntry；逐 stage/category/native index；不支持的 descriptor arrays/push-constant mapping 明确拒绝；参数事务失败原子性 | aurora-rhi-metal/shader 主 spec、target artifacts delta、Aurora 领域规则 |
| ResourceGroup 可发布缺少活跃资源的首次版本；Shader 生命周期表述自相矛盾 | 首次事务满足全部活跃输入；后续更新继承；Shader/plan/layout 在使用者存续期间保活 | aurora-shader/resource-binding 主 spec 与 target artifacts delta |
| GlobalRenderResources.UpdateView 及视图相机进入 Global | 旧 helper 随无消费者 RenderGraph 删除；相机属于具体 Pass；生产图使用不可变 frame 快照 | resource-binding 主 spec 和 target artifacts delta |
| 23 项 backlog 仍被称为当前状态、Renderer 仍被称为未完成 hub | 初次按 37 个目录重建；旧图清理及提交拆分后更新到 40 个目录、任务计数和归档边界 | consolidate-active-changes |
| 部分主 spec 仍直接放 ADDED delta；已移除 ImGui renderer 仍被要求实现 | 五份 console/terminal/CVar 主 spec 补正确结构；imgui-console 明确退役并指向现行 console 能力 | command-shell、console-module、console-ui、cvar-registry、system-terminal、imgui-console |
| 根规则的构建 cache 路径与实际 cache 不符 | 使用已有 `3RD_PATH=D:/Code/Engine/sky_3rd/Win32`，保留既有 generator | AGENTS.md；未修改 CMake cache |

当前版本依据：Mesh 6、Skin 2、Skeleton 1、Material 4、MaterialInstance 2、shader cache 10、usage 3。历史 v2/v5/v6/v9 测试记录未伪装成新版本验收。

## 测试证据与开放任务

本轮只修改文档/规范及两处过期代码注释，没有重新运行原生构建或 GPU 测试。下表是核实过的既有 XML；相关实现的测试结果仍属于此前执行。

| 范围 | 已有结果 | 不能推出的结论 |
|---|---|---|
| Framework | 205/205 | 不能证明所有平台文件替换/进程中断行为 |
| Animation Graph/World/Builder/Editor/Module | 14/10/4/8/2，均无失败 | 不能代替完整多角色、重导入、PIE 人工交互 |
| Content Adaptor/Builder | 7/1，均无失败 | 不能仅据此关闭 Graph plugin 缺失的全部组合验收 |
| Aurora Adaptor/Cook | 66/52，均无失败 | 不能代替各后端完整颜色/法线/阴影 oracle |
| EditorCore/Widgets/Shell/SandboxProject | 179/40/144/13，均无失败 | 不能代替所有原生窗口、DPI 和设备场景 |
| GPU 最终两次记录 | 首轮 177 项中 174 通过、3 失败；修复等待辅助函数后定向 3/3 | 不是一次完整 suite 全绿；未运行本轮 native Metal |

动画未勾选的复合任务逐项保持开放：

| 任务 | 已有实现/相关证据 | 仍需闭环 |
|---|---|---|
| 0.2 | mesh cook fixtures、SkinBindingTest、GPU 骨骼测试 | 将变换/helper/错序/五权重与 CPU 法线/bounds oracle 全矩阵逐项对应 |
| 0.3 | 当前四组、typed V/P/S 与 metadata 接口已同步；撤回旧 DSL 描述 | 跨 change 全部交界场景验收 |
| 1.14 | GraphDocument 的保存失败/dirty 保留；Framework 产品组事务与 revision 测试 | Clip/Graph 压缩、写失败及加载跨提交点组合 |
| 2.1、2.4 | 实际 Graph/GraphAssets/World/Builder/Editor targets、配置及 ModuleTest | 完整关闭插件/无 editor/provider 缺失构建和装载矩阵 |
| 4.4 | SkeletalSceneAssets 的跨提交点不混代；World/bridge 快照测试 | BUSY、先 reload rig 与冻结旧组合的完整关联证据 |
| 4.5 | palette structured buffer、精确 range、生产图事务测试 | 删除/reload/双在途及 Metal 原生退休矩阵 |
| 4.7、4.8 | Skin bounds、GPU 动画与阴影、拾取相关测试 | 各 fixture 的 CPU/GPU 位置、normal/tangent、裁剪 oracle 全矩阵 |
| 4.10 | ProductionMaterialAndReferenceBindingNeedNoAnimationProvider | 缺插件的真实模块组合、显式换绑定及旧帧退休 |
| 5.3、5.4 | Preview settings、自动取景、timeline/action 和 GraphEditor 布局测试 | 完整骨架树/诊断/播放操作 UI 验收 |
| 5.5、5.6 | preview session/关闭、Skin reflected DATA provider | 多工具窗/编辑 World/PIE 隔离及 generated Skin 全入口验证 |
| 6.5 | Content 已移除 Graph；GraphAssets 唯一注册；真实 ModuleTest | 所有缺 plugin/装载顺序负例的汇总验收 |
| 7.5、7.6 | World Graph 绑定、热重载、独立实例及 Graph 采样测试 | golden pose/稳定预分配/source-free/Preview 的完整组合证据 |
| 8.5、8.6、8.7 | 活动状态/revision、dirty 关闭、保存重开和预览探针 | node/pin 失败定位、完整图编辑流程、最后成功产品与草稿组合 |
| 9.3、9.4、9.5、9.6 | Windows 有界启动、自动化结果、本次一致性复核 | 完整人工 World/PIE、native Metal、场景验收和最终提交审批 |

target-shader-artifacts 的 5.4/5.5/5.6/5.9/5.10/6.7 保留未勾选。5.6 已有 Windows 相关证据，但尚未把每项要求与确切 debug-layer/原生运行逐条收齐；没有只因后续测试总数增加就代为关闭。source-techniques 的 Metal 6.4 同样保持开放。

## 旧入口处置与剩余计划

1. **旧 RenderGraph 执行路径已退役**：后续调用审计确认只有旧模板和专属测试消费，用户要求无使用即删除。`remove-unused-legacy-rendergraph` 移除了该执行器、三后端 adapter 与模板，保留共享声明/现代 frame compiler。此前绑定顺序、单 stream、固定 U32 的缺口随入口删除；并非宣称旧入口已迁移或降低生产图契约。验证记录见该 change。
2. **五个无 delta 的计划**：animation-events、audio-animation-bridge、audio-world-integration、aurora-cook-schema-layering、aurora-navigation-integration。它们是未实施/债务记录；本轮没有为让 strict 变绿而编造实现规范或勾选完成。
3. **设备/人工验收**：新四组和 palette 的 Metal 原生、移动/Linux 以及完整 World/PIE 交互仍开放。历史 Metal、Windows MSL 生成和当前 Windows GPU 结果不能互相替代。

## 校验与提交/归档边界

- 原一致性审计全库 strict 为 **206/211**；加入旧图清理后为 **207/212**；提取资产事务与 Animation 算法前置 change 后为 **209/214**。剩余 5 项仍是上面的无 delta 计划。174 份主 spec 均通过格式检查；格式通过不代表代码行为全部实现。
- 原 delta 只读合并为 **61/61**；加入旧图清理后为 **66/66**；两个前置 change 提取后为 **69/69**；补齐动画 metadata owner 的 module-registration delta 后为 **70/70**。已重设同步到主 spec 的 ADDED/RENAMED/REMOVED 操作，避免重复添加或对已消失标题执行删除/重命名。
- 新增错误路径规则扫描：异常/RTTI、共享 static、诊断残留、相对 include、非 ASCII C++、重复代码块均为 0；`git diff --check` 通过。Git index 与清理开始时保存的快照一致。
- `refactor-process-owned-reflection`、`aurora-world-editor`、`fix-sandbox-project-creation` 可进入归档范围审查；仍需用户明确确认，并满足实现与 spec 同批提交。旧 shared-pass/pipeline-ownership 还须明确历史范围；动画和当前两项 shader change 的开放验收阻止整体完成归档。
- 原 529 路径“大第一批”方案不再作为最终提交清单。旧图清理、资产事务和动画采样修复已获用户确认并提交为 `35562404`、`ce2d9087`、`60a16242`。用户最新指定剩余仅两个功能提交：metadata/Framework 随 shader/material 前置，animation/editor 完整后置；共享文件按逻辑块制作并验证中间快照。剩余两批尚未获 commit 批准；没有 push 或 archive。

## 提交拆分补充

`refactor-asset-product-transactions` 与 `fix-animation-sampling-validation` 分别承接通用资产事务和 Animation 运行时算法；动画 workflow 保留其资产、World、编辑器与渲染集成验收。旧图清理、资产事务、动画算法已在隔离的连续中间快照构建并测试：273 + 204 + 64 项通过，旧图快照另有 1 项缺少 package fixture 而跳过。前三批正式提交后，其余 staged/unstaged 内容均保留。

后续两批的精确 message、文件清单、tree 和补丁位于 `cmake-build-debug/cleanup-review-2026-10-25/split-v4/`。材质/Shader 前置快照独立构建；动画提交恢复此前已经集成验证、经 515 个非文档路径 SHA-256 核对保持一致的完整实现。具体新旧验证证据见 `openspec/changes/refactor-aurora-shader-source-techniques/verification.md`。14 个无关研究文档/console spec 路径保留在工作区，不混入这两个功能提交；因此隔离提交快照仍包含原有 console spec 格式债务，而完整工作区已修正这些文档。主工作区实现与本轮开始时的暂存区均未改动。
