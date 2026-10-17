# Windows 主线归档与提交记录

核对日期：2026-10-17。用户已确认归档与提交，并要求随后 rebase origin。两个 change 已归档，8 个主 spec 已同步，代码已接到 `origin/dev_refactor_rhi` 的 `9e52ed3f`；rebase 合并与最终验证见 [review.md](review.md)。

## 归档范围

已将 `aurora-renderer` 按 Windows B01–B14/P01 已验收范围部分归档，并归档完成的 `aurora-renderer-layout`。主 change 为 **41/78**：原 40 个未勾选项中，7.3、7.6、7.7 已由现有证据满足；其余 **37 项保留未勾选**，35 项移交平台对等，2 项移交扩展。移交不是完成，也不宣称完整 Metal/HDR/资产管线已交付。

- 7.3：B14 GPU XML 覆盖同 Scene 双相机、RGBA8/D32 绘制和两个在途 slot；含 `B13TwoCamerasSecondSceneAndUiLoadUseSeparateConstants`、`TwoSlotsKeepCommandsAndOwnedPrepareProductsUntilCompletion` 和 IndexedRaster 实际读回。
- 7.6：平台未决结论已在 status/design 和接收 change 中保留，主 change strict 校验通过；本次另验证同步后的主 spec 预览。
- 7.7：`EcsDependencyTest` 归档前重跑 9/9；同类型不同 owner、Read/Read、共享 resource alias、类型级保守冲突，与既有 FrozenWriteView 越权/冻结、ParallelPrepare 跨 Scene 与多 View 回归共同覆盖。

## 后续工作与依赖

| Change | 状态 | 下一步与解除条件 |
|---|---|---|
| aurora-renderer | active，待确认部分归档 | 交付 Windows 主线，37 项余项按下表保留追踪 |
| aurora-renderer-layout | active，8/8，待确认归档 | Foundation 结构纠正；独立结构 diff 保留 |
| [aurora-scene-bridge](../../aurora-scene-bridge/proposal.md) | active，可开始 | 优先 S01→S02→S03：World/Actor/component/asset → owned packet；Windows 主线前置已解除 |
| [aurora-renderer-platform-parity](../../aurora-renderer-platform-parity/proposal.md) | blocked-on-prereq | Mac/Metal GPU 与原生设备环境；probe→协议→绘制→输出→对应宿主 |
| [aurora-renderer-extensions](../../aurora-renderer-extensions/proposal.md) | debt-record，未排期 | 先确定消费者与验收，处理 CPU prepare 注册、旧模板/HDR；扩展格式/Compute/storage/跨队列另定范围 |
| terrain-aurora-render / vegetation-aurora-render | blocked-on-prereq（完整交付） | B13 feature 可消费；compute/indirect/完整材质仍缺，不能按 Renderer 完成自动解除全部条件 |
| aurora-navigation-integration | active（设计仍待补） | Renderer 基础门解除，完成 debug geometry 接口与适配设计 |
| aurora-metal-bindless-descriptor-heap / aurora-cook-schema-layering | debt-record | 不属于本次平台首版或归档前置 |

依赖：Windows Renderer → scene-bridge S01→S02→S03 → 真实 World 场景；平台对等独立推进，只有对应平台能力门通过后才切宿主。CPU system 与 GPU frame feature 不强制同构。原生后端目录重命名仍须后续独立评估，不随本次提交实施。

## 原任务逐项移交

| 原编号 | 处理 | 原验收内容 |
|---|---|---|
| 2.2 | 平台对等，同编号 | 修正 RDG executor 的 PSO/RG 顺序与布局切换重绑；覆盖 DX12 和 Vulkan 真正的 descriptor 使用（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口） |
| 2.3 | 平台对等，同编号 | 重构 frame fence 状态；覆盖零输出、全部 acquire 失败、恢复和提交失败（B11/B12 Windows 组件与生产服务已验；B14 Windows 宿主已验；P02 待验收） |
| 2.4 | 平台对等，同编号 | 分离 acquire/present 同步资源复用规则，定义 resize/close 的 presentation retirement（B11/B12 Windows 已验；B14 Windows 宿主已验；P02 待验收） |
| 2.5 | 平台对等，同编号 | 为 RHI Submit/完成查询补可观察的 Accepted/Rejected/Error 协议；故障注入验证 reset 后拒绝与 device lost 不无限等 fence（Windows 组件及 B12 服务已验；B14 Windows 宿主已验；P02 待验收） |
| 3.4 | 平台对等，同编号 | 将 acquire 移到调度器；RDG 导入 borrowed output，重复 Compile 不获取第二张图像（B11/B12 输出与 B13 owned UI 已验；B14 真实 UI/宿主已验；P02 物理旋转/非 Windows 待验收） |
| 3.5 | 平台对等，同编号 | 实现窗口/离屏输出、单输出失败隔离、已 acquire 后失败收尾和窗口销毁确认（B11/B12/B13 Windows 输出/合成已验；只跳过受影响输出，其它 pass/command buffers 和上传继续；B14 Windows 宿主已验；P02 待验收） |
| 3.11 | 扩展，同编号 | 注册 Bounds/Light/Object 等 CPU prepare 及只读 View cull/LOD/extract；join 后在 coordinator 完成 GPU ResourceFinalize 与建图（B08/B12 当前子集已验；B13 frame feature 不等同于动态 CPU prepare feature 注册，更多 light/着色仍待后续扩展） |
| 3.16 | 平台对等，同编号 | 独立 DeviceFrameContext 领域头文件，提供 TryBeginSlot/分配/Compile/Dispatch/TryCollect/Submit/Discard/PollCompleted；校验 backend/context/slot generation/device epoch，宿主不直接驱动 RDG command buffers（B12 Windows 服务已验；B14 Windows 宿主已验；P02 待验收） |
| 3.17 | 平台对等，同编号 | 统一 staging/DynamicBuffer/descriptor ring 的显式 slot 分区与容量检查；消除 renderer 路径 AdvanceFrame/Reset 自增，验证任意空帧/输出跳帧/upload-only（B04 的 Vulkan/DX12 新协议已完成；Windows 宿主 B14 已切换；Metal P02 待验收） |
| 3.18 | 平台对等，同编号 | 按 slot × worker × recording ordinal 建立安全 command 资源与预算；验证任务多于 worker 时不覆盖待提交结果，两帧在途不互相 reset；Metal child 按 parent 管理（B02 Vulkan 与 P01 当前 DX12 子集已验收；完整平台能力及 P02 待收口） |
| 4.1 | 扩展，同编号 | 实现显式 ViewRenderContext，迁移 Scene/Opaque/Fullscreen pass；逐 View 保存可变状态（B08/B12 解析桥、B10 Fullscreen、B13 FrameRenderTarget/FrameComposer 已验；B14 Windows 宿主已切换最小合成；Metal P02 待验收，HDR 不属于最小合成范围） |
| 4.2 | 平台对等，同编号 | 实现逐 pass set 0、逐 view/slot 常量；填完整 projection/camera position（B04 分配已完成；B07 scalar UBO/set 0 已验证、B08 projection/camera 已实际读回；P01/P02） |
| 4.3 | 平台对等，同编号 | 实现后端 Compile 的 descriptor resolve 或显式前置任务，发布不可变 bindings；审计 DX12 EnsureFrameCopy 等 bind 时共享写入，确保 RG 引用资源有显式 Read/Write（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口） |
| 4.4 | 平台对等，同编号 | 实现 transient pool frame leases 与导入资源状态；覆盖跨帧保活、同帧物理复用及重复导入（B10 Windows 固定资源范围已验；完整平台归 P01/P02） |
| 4.6 | 平台对等，同编号 | 实现 content version/producer、ReadWrite/WriteDiscard 与明确 roots；先内容剔除再推导存活 hazard/lifetime，覆盖中间读与 UI LOAD（B10 内容/alias 与 B13 生产 UI LOAD 已验；完整平台归 P01/P02） |
| 4.7 | 平台对等，同编号 | 闭合 CopyBlit 的 typed source/destination、owned regions、buffer copy/buffer-to-image emit 和对齐检查；其它 kind 不得静默无操作（B06；P01/P02） |
| 4.10 | 平台对等，同编号 | 各后端自行完整 Compile；公共图只保留声明/Freeze 与可复用纯分析，context 持匹配 compiler，新路径不由公共 Compile/ExecutePasses 控制（B12 Windows 三条生产路径已验；P02 待实现） |
| 4.11 | 平台对等，同编号 | 定义 native pass/scope 与 logical pass 映射；后端分别决定合并/拆分、GPU 顺序、lifetime/backing/同步，顺序变化后重新验证 alias（B01 scope/映射与 B10 Windows 固定范围已验收；完整平台归 P01/P02） |
| 4.12 | 平台对等，同编号 | 分离 EncodeTaskGraph 和 SubmissionPlan，稳定 result index、CPU edges、Worker/Coordinator executor 约束与 opaque plan 单次消费；Custom 默认串行（B02/B03 与 P01 当前 Vulkan/DX12 分片、单次消费与失败收齐已验收；完整平台能力及 P02 待收口） |
| 4.15 | 平台对等，同编号 | 实现 Metal 编译/dispatch：native scope 描述、parallel parent/有序 children 生命周期、autorelease/强持有；不支持并行的 scope 明确安全回退，需 Mac 平台验证（P02） |
| 4.16 | 平台对等，同编号 | 在各后端 FrameContext 适配 DeviceFrameDispatcher worker 子批次与 coordinator continuations；覆盖空图、单 worker、有界 fan-out、显式失败/取消 drain 和无 worker 阻塞等待（B02；P01/P02） |
| 4.19 | 平台对等，同编号 | 扩展 raster recipe、bindings、depth/MSAA/resolve、subresources 与 PSO/scope cache，接入两套 encoder 的稳定并行组装；串行子集不等同于本项完成（B07/B10/B12 固定范围与 B13 UI sampled bindings 已验；完整资产材质不在最小范围，剩余平台归 P01/P02） |
| 4.20 | 平台对等，同编号 | 明确新图 IR 的公开 pass kind 能力范围；受支持 Compute/FullScreen/Custom 发出真实命令并通过读回验证，不支持者显式 Unsupported，Custom 遵守声明访问和 coordinator 约束（B10 Windows：Fullscreen 已读回，Compute/Custom 显式 Unsupported；跨平台归 P01/P02） |
| 5.1 | 平台对等，同编号 | 实现程序化 indexed mesh 和简单 Slang VS/FS，通过 owned packet 提交与上传（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口） |
| 5.2 | 平台对等，同编号 | 接入最小 ShaderResolver/PSO cache，key 覆盖 program/layout/vertex/state/attachment（B07 前置解析与 pass/subpass 内缓存、B10 Windows device 跨帧缓存已验；完整平台/生产条件待收口） |
| 6.4 | 平台对等，同编号 | 在平台 probe 中确定 Metal 线程和 drawable 协议；未通过时明确保持未迁移状态（P02） |
| 7.3 | 已核对完成 | Vulkan/DX12 真出图 smoke/readback 覆盖两个相机、两个以上 inflight slot、不同 attachment format（B07/B13；P01/P02） |
| 7.4 | 平台对等，同编号 | 生命周期测试覆盖最小化单窗/全部窗口、恢复、resize、close、资产迟到和 device/surface 错误（B11/B12 Windows 组件与公共服务已验，含配置注入/8 种纯映射；B14 owned 资产/真实宿主已验；P02 物理旋转与非 Windows 待验收） |
| 7.5 | 平台对等，同编号 | 按根 AGENTS 关闭编辑器后配置并构建相关 targets；运行相关测试和有界宿主验收，记录各平台结果（各批与最终收口） |
| 7.6 | 已核对完成 | 更新设计中的未决平台结论并执行 openspec validate aurora-renderer --strict --no-interactive（各批与最终收口） |
| 7.7 | 已核对完成 | CPU 依赖测试覆盖同类型不同 Scene、跨 View 只读、共享资源别名、保守 query 冲突、结构冻结和声明越权（基线；B08/B09 收口） |
| 7.11 | 平台对等，同编号 | 验证同图首次上传后真实 draw/readback、无窗口 upload-only、预算延后、取消共享 ticket、热更新与过期 device epoch（B06/B07/B12 Windows 公共服务已验；P02 待验收） |
| 7.12 | 平台对等，同编号 | 验证慢/不可用输出期间控制流继续，prepare 后 resize 不沿用旧 ViewContext；记录后端可退出等待与呈现退休限制（B11/B12 Windows 配置竞态与公共排队 resize、最小化继续提交已验；B14 Windows 宿主已验；P02 待验收） |
| 7.13 | 平台对等，同编号 | 对三个后端执行相同内容/剔除语义用例，并对串行/并行 dispatch 做实际 readback 对照；记录未验证的平台，不以公共 fallback 代替后端实现（B01/B02/B10；P01/P02） |
| 7.14 | 平台对等，同编号 | 打乱编码任务完成顺序，验证 GPU copy→draw→UI 同步、透明片段稳定组装、task 数大于 worker、同 slot 多结果和两帧在途（B02/B03 分片乱序/双 slot、B07 bindings、B13 copy→UI 与 97 draws 稳定顺序已验；完整平台仍待 P02） |
| 7.15 | 平台对等，同编号 | 故障注入 Compile/dispatch 部分失败、错误 context/epoch/slot、重复 Dispatch/Submit 与取消；确保 drain 前不 reset、失败不发布上传、coordinator-only 节点线程正确（B03 与 P01 的 Vulkan/DX12 native 错误/取消/退出已验收；B06 上传发布及完整平台能力待收口） |
| 8.3 | 平台对等，同编号 | 实现 CpuReady/UploadQueued/UploadSubmitted/Resident 状态与 graph-local InGraph resolve；不把 create 成功或 Submit Accepted 当成 Resident（B06/B10/B12 Windows buffer/image 与生产组合已验；Metal P02） |
| 8.4 | 平台对等，同编号 | 实现 slot-owned staging、UploadBatch 的字节/创建数量/在途预算及显式持久上传 roots，支持 upload-only submission（B06/B10/B12 Windows 组件与生产预算/后台分块已验；Metal P02） |
| 8.5 | 平台对等，同编号 | 实现实际录制 receipts、提交接受后的状态登记、完成后的 Resident 发布和源 bytes/staging/GPU leases 分阶段释放（B06/B10/B12 Windows 组件与统一 Submit/Poll 已验；Metal P02） |
| 8.6 | 平台对等，同编号 | 实现精确版本 pins、冻结前旧版本选择与延迟释放；验证新版本失败不覆写旧在途 descriptor/mesh，未匹配状态的 legacy transfer 资源不得直接使用（B05–B10 组件与 B12 公共热更新/逐帧 layouts/pins 已验；Metal P02） |


## Spec 同步结果

已按 layout → renderer 顺序同步，覆盖 8 个 capability：async-threadpool、aurora-adaptor、aurora-launcher、aurora-pipeline、aurora-rdg、aurora-renderer、aurora-rhi-core、core-ecs。

归档前校准：原 pipeline 的 11 项旧模板/HDR MODIFIED 目标移入 extensions，主 change 改为 2 项已实现的新 Renderer 最小管线/输出契约，并保留确已移除的 RenderPrimitive 条款。RHI/RDG 明确未支持后端返回 Unsupported；不把 Metal 目标细则同步成已实现声明。layout 同步修正 adaptor/runtime/editor 的实际 target 名和编辑扩展不另建 Renderer 的规则。

已归档到 `2026-10-17-aurora-renderer-layout` 与 `2026-10-17-aurora-renderer`，修正相邻 change 的相对链接并补齐主 spec 的 Purpose。执行前预览与 review 清单保存在 `output/aurora-archive-preview/`、`output/aurora-closeout-review.md`；它们是确认时的历史快照。

## 归档前验证与限制

最近代码验证：B14 GPU 基线与修正专项合并 804 通过/3 个 Vulkan-only 用例在 DX12 跳过/0 未解决失败；最终受影响用例 94/94；Aurora 13/13 组，Framework 4/4，EditorShell 33/33；真实编辑器三模式 × hub/project 6/6；截图/resize/close 已验。见 review 与 `output/b14-finish-verification.json`。本轮只有文档与归档准备变更，补跑通用依赖 9/9，不重复计入 GPU 数字。

保留风险：曾有 Vulkan 初始化 0xC0000005 未取得根因，后续复验通过不能宣称修复；Metal/物理旋转、完整 World 资产提取、HDR/完整材质均未验收。本次不处理其它 change 的 archive，包括已提交的 fix-sandbox-project-creation。

## 已确认并执行的操作

1. 将两个 change 按上述范围 archive，允许 renderer 保留 37 个已移交的未完成勾选项；同步且验证 8 个主 specs。
2. 使用一个提交纳入本次完整代码状态、两个归档、主 specs、后续 tracking changes 和相关规则/依赖文档。结构 diff 与功能 diff 仍独立供 review，不重写工作区历史来拆出一个临时结构状态。
3. 提交 message：`[feat]: integrate Aurora renderer and Windows hosts`。确认时的路径与 hash 清单在 `output/aurora-closeout-files.json`，rebase 后清单在 `output/aurora-rebase-files.json`；不提交 output/ 下的测试工程、截图、日志和预览。

用户在完整范围、message 和文件清单展示后明确回复“确认，然后 rebase origin”。执行结果：实现与 spec 同一提交；未提交 output 缓存；rebase 前状态保存在 `codex/aurora-before-origin-rebase-20261017`，没有 push。
