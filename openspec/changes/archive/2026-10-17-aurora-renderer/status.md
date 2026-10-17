# Aurora Renderer 当前状态

Windows 已验收范围已归档，执行与剩余任务移交见 [closeout.md](closeout.md)。Metal 与扩展仍未完成；rebase 后验证见 [review.md](review.md)。

核对日期：2026-10-17；分支：`dev_refactor_rhi`。这是代码与验证状态的唯一摘要。实施顺序见 [batches.md](batches.md)，稳定验收编号见 [tasks.md](tasks.md)，详细日志与历史见 [review.md](review.md)。文档中的目标设计和 SHALL 不代表已实现。

## 归档后 rebase 验证（2026-10-17）

已合入 `origin/dev_refactor_rhi` 的 `9e52ed3f`。最终 Debug 构建通过；GPU 807 通过/3 预期跳过/0 失败，Core/Framework/编辑器合计 397/397，Aurora 13/13 组，真实编辑器 6/6。兼容修复与日志见 review 顶部；后续范围和未完成项不变。

## Review 范围约定

2026-10-17 用户明确拒绝重复的 renderer/core、renderer/adaptor、renderer/runtime 分层，并确认回归 Aurora 既有 core/adaptor/runtime。结构纠正归 [aurora-renderer-layout](../2026-10-17-aurora-renderer-layout/design.md)，保留独立 diff。Renderer 实现和测试迁入 core，Framework 注册并入 adaptor，使用 Aurora.Render 入口；公开 include 路径不变。原生后端路径和 B10 GPU 逻辑不变。结构验证结果见该 change 的 tasks/review。

结构纠正已验证：13/13 组接口/CPU/资源/模块与原 core/adaptor 测试，GPU 592/592、RHI 133/133；Sandbox 构建通过，未运行 UI。68 项移动映射、独立内容 diff 和具体边界见 [结构验收](../2026-10-17-aurora-renderer-layout/review.md)。本结果不增加 B10 功能完成项。

## B14：Windows 游戏/编辑器宿主迁移完成（2026-10-17）

Windows AuroraModule 与 EditorRenderer 均使用 RendererHost 创建/发布唯一 IRenderer。游戏宿主和编辑器分别由单一主线程 producer 驱动 packet；场景数据通过 SceneSource/RenderSceneProxy 追加 owned 命令，Renderer 执行 ECS prepare、资源上传、RDG、backend compile/encode 与提交。World 组件/资产自动提取仍归独立 scene-bridge。

编辑器已切换主窗、工具窗和场景离屏输出。主线程完成 UI 布局、FreeType 图集/图标更新后生成 owned 快照；精确图像版本、圆角 SDF、clip、alpha 和逻辑 ViewOutput 由渲染端 feature 处理。真实 Viewport 面板绑定宿主提供的逻辑离屏纹理。纹理更新/释放走统一 catalog/uploader 和帧 pins，CPU 图像上限 64 MiB，纹理/释放项有界；上传或 CPU 数据失败显式报告。

EditorApplication 在模块加载前选择 provider-only，SandboxModule 声明 provider/编译器/扩展依赖。真实 loader lease 延迟 DLL detach/unload，依赖代码随消费者代码保活；动态/静态入口均验证。依赖失败阻止消费者初始化并返回应用启动失败。工具窗口收到 ViewportDestroyed 后再解除监听和销毁 HWND；整个宿主在窗口成员析构前 drain。Windows 旧 GPU loop 编译排除，目录、target 布局保持原位。

验证合并覆盖 GPU **804 通过、3 跳过、0 未解决失败**；最后受影响 Renderer/UI/宿主 **94/94**。完整回归曾暴露三个后端模式的 CANCELLED 通知丢失，修复发布/回收顺序后已复验。接口/CPU/资源/模块 **13/13 组**、Framework **4/4**、EditorShell **33/33**；三模式 × hub/project **6/6** 真实启动正常退出，另有真实窗口 resize/close 与截图检查。Vulkan 同步验证无错误，DX12 用例检查 debug InfoQueue。证据见 review。

B14.1 的原 diff 保留；本轮独立差异为 `output/b14-finish-review.diff`，生产差异和五组 diff 同目录前缀。没有 commit 或 archive。Metal 新协议/宿主验证归 P02；物理旋转设备、完整 World bridge 和完整材质/光照不在本批完成范围。

## B13：多输出与 feature 合成（2026-10-17）

生产 Renderer 已接 FrameFeatureRegistry、FrameComposer 和 adaptor 的 UiRenderFeature。FramePacket 接受注册、启停、退休和 owned UI 快照；feature 的图/GPU/presentation 引用退出后才通知退休，重新启用不会恢复已停用的排队回调。ViewOutputRef 精确校验 viewport generation/revision，仅解析本帧已经生成的离屏内容。

同一图先共享上传与逐 View raster，再 UI 采样/LOAD/透明合成，最后输出/readback。不可用窗口和未产生绘制的 acquired output 各自跳过，不阻断共享 producer、其它输出或提交。UI 消费实际 output extent/transform，native shader 创建归 coordinator，几何上传使用现有 slot staging；没有新 target、第二套 ring 或目录移动。

当前专项 **87/87**（B12/B13 公共入口、feature registry、presentation owner、unused acquire 与映射）；其余 GPU 回归 **701 通过、3 跳过**，合并为 **788 通过、3 跳过、0 失败**。3 个跳过项是 DX12 不适用的 Vulkan surface 专项。接口/CPU/资源/模块 **13/13 组**；SandboxModule/SandboxEditor 构建、OpenSpec strict 和源文件规范审计通过。B13 独立 diff 为 `output/b13-review.diff`、`output/b13-production.diff` 和分组目录 `output/b13-review/`，批前 543 文件快照保留于 `output/b13-before/`。

B13 当时范围是 owned RGBA8 ViewOutput UI 合成；真实编辑器快照、普通纹理/字体 atlas、宿主切换和真实 loader lease 已由 B14 补齐。CPU 插件 prepare 注册、完整材质/HDR pipeline 未在本批宣称完成；Metal 新协议仍归 P02。97 个透明 draw 已覆盖多于 3 个 workers 的编码片段与多帧排队；更大快照仍受现有 descriptor/staging 预算约束。

## B12：生产 Renderer 已贯通（2026-10-17）

`IRendererProvider::Create` 现在返回可运行的 Renderer。主线程只提交 owned packet/PollEvents；唯一 coordinator 持有 DeviceSession、持久 stores、资源目录/uploader、FrameScheduler 和一个 DeviceFrameContext。Vulkan RenderPass、Dynamic Rendering 与 DX12 分别走自己的 compiler/encoder，CPU prepare 与 encode 使用独立 pool。

完整流程为 Apply → 资源订阅/CPU ready 与 BeginViewport → 冻结/并行 prepare → owned FrameBuildContext → slot → upload/output-pass acquire → Freeze → backend Compile/Dispatch → 统一 Submit → GPU 完成/Resident/FrameCompleted。输出不可用只省略该输出 pass；无窗口时分块上传仍自动推进。输出快照、mesh/material layout 与精确 pins 按帧保留，连续 resize/热更新不会替换排队帧的版本。

公共入口专项 **52/52**；与原有 GPU/RDG/输出回归去重后为 **753 通过、3 跳过、0 失败**。3 个跳过项是 DX12 不适用的 Vulkan 专项。RHI **133/133**，接口/CPU/资源/模块 **13/13 组**（资源/prepare 34、coordinator 18）；最终动态模块 **3/3**、静态模块 **4/4**。专项覆盖真实像素、upload-only、预算、共享取消、热更新、最小化继续、排队 resize/shutdown、Scene/DeviceLost 恢复、互斥与背压。SandboxModule/SandboxEditor 构建和 OpenSpec strict 验证通过；未运行编辑器 UI。完整证据见 [review.md](review.md)。

既有 Aurora.Render 增加 `--aurora-provider-only` 启动选择，避免先启动 legacy device/loop；宿主必须先关闭/销毁 Renderer 再卸载模块，后端 DLL 由 DeviceSession 保活。B12 当时尚未切换默认宿主；当前 Windows launcher 与编辑器已由 B14 接入。B13 负责 feature、多 View 合成及 UI 消费，Metal 新协议保持显式 Unsupported。旋转验证覆盖 8 种 clip/UI 坐标纯映射和既有查询注入，未在物理旋转设备上运行。

Release profiling：8192 实体、双 View、5 轮；4 workers 下 1024 行分块的 dirty/clean 总耗时中位数为 **18.428/17.865 ms**，256 行为 **19.405/18.789 ms**。生产采用 1024 行分块，保留 256 soft system 节点、4096 hard task 节点及超预算串行回退；数字是当前机器的测量，不作为跨机器性能保证。阶段数据见 [review.md](review.md)。

## B11：Windows 输出生命周期已完成（2026-10-17）

`FrameScheduler` 已接通输出快照、延迟 acquire、borrowed backbuffer 导图、显式提交同步、逐输出 present/retire 与关闭确认。CPU prepare 先完成，再获取可用 FrameContext slot，最后逐窗口进行 timeout=0 acquire；尺寸/revision 改变只跳过受影响输出。零输出或全部 Busy 可丢弃空 lease，有上传时仍走同一 context。最多 64 个活动输出，native presentation 记录按 image count 有界；不新增 GPU ring 或模块 target。

Vulkan RenderPass/Dynamic Rendering 共用已验证的 borrowed 输入边界，各自输出 encoder；新 checked WSI 路径要求 swapchain-maintenance1，支持未提交 acquire 的显式释放和独立 present fence。DX12 使用实际 backbuffer 格式、DO_NOT_WAIT Present 与 Present 后独立 queue fence；应用资源退休不等于屏幕扫描完成。正常 resize/close 不做全设备 WaitIdle，必须等 CPU 借用、GPU 和 presentation 责任结束。Rejected 保留重试；Indeterminate 隔离且不发正常关闭确认，终态销毁另行安全收尾。

验证：GPU 全量 **656/656**（既有 608 + 当时输出 48），最终 resize SurfaceLost 补充后重跑输出专项 **51/51**；合并覆盖 659 个不同用例。RHI **133/133**，CPU/接口/资源/模块 **13/13 组 CTest**，SandboxModule/SandboxEditor 构建通过，未运行编辑器 UI。GPU 两次最终日志的 Vulkan 同步验证错误均为零；DX12 原生窗口测试同时检查 debug InfoQueue。日志为 `output/b11-gpu-final.*`、`b11-output-final.*`、`b11-rhi.*`、`b11-cpu.log` 和 `b11-build-{final,last,close}.log`。首次全量发现并修复逐 cell PRESENT 非 borrowed 校验遗漏；同步验证另发现 B10 日志已有的 20 条 depth resolve access/stage 错误，已按 Vulkan 规范补齐。既有 Vulkan 初始化间歇 0xC0000005 本轮未复现，仍未归因，不宣称修复。

B12 已接正式主循环、唯一 completion poll owner 与 uploader/scheduler 组合；图像 ID 的 generation/tombstone 沿用 CpuSceneStore。B13 保留 prepare/slot 后、单图构建前 acquire，不增加第二次提交；将 acquire 再推迟到离屏 GPU 工作完成后属于未实施的跨提交扩展。Metal checked 输出协议留 P02。以上 Windows 组件验证不代表已切换游戏/编辑器宿主。

2026-10-17 B12 首子批已接入 Windows surface 组件：QuerySurface 显式区分 Ready/Busy/OutOfDate/SurfaceLost/Failed，保留错误和创建代次；BeginViewport 查询当前配置，acquire 后再次复核。Vulkan 比较同尺寸 transform 及其余创建配置，未指定 extent 时读取当前窗口尺寸；DX12 检查 HWND、设备状态和原生 descriptor。配置变化等待原输出责任退出后重建，并合并最新目标；查询失败可下帧重试，SurfaceLost 由宿主关闭并替换输出，DeviceLost 停止新获取/提交。Snapshot 仍是缓存读取，不能替代 BeginViewport。生产调用与 clip/UI 纯映射已在本页 B12 收口；物理旋转设备与非 Windows 验收仍待 P02。

B12 首子批验证：输出专项 **93 通过、3 跳过、0 失败**，跳过项仅为 DX12 不适用的 Vulkan 专项；RHI **133/133**，SandboxModule/SandboxEditor 构建通过。同步验证与 DX12 InfoQueue 无错误；未运行 UI、Android/Metal 或真实旋转映射。日志 `output/b12-surface-gpu-final.*`、`output/b12-surface-rhi.*`，独立 diff `output/b12-surface-review.diff`。本轮新增 36 个实际执行用例，未混计到历史全量验证。

2026-10-17 跳过策略确认与补验：仅省略 acquire 不可用的当前输出 pass，其它有效 pass/command buffers 正常提交，不使用 fake backbuffer。一个/全部输出 OutOfDate 与离屏 clear/readback、buffer copy 的三路径组件专项 **57/57** 通过；日志 `output/output-pass-skip-gpu.log`。B12 已把该行为接入生产主循环，不以成功输出数量为零提前取消有 GPU 工作的帧。

两项 OpenSpec strict、稳定编号/依赖/文档链接、30 个修改 C++ 文件的 ASCII/BOM/异常/RTTI 检查及 git diff --check 通过；合并用例计数见 `output/b11-verification.json`。

## B10 收尾：Windows 三路径固定范围已完成（2026-10-17）

D32 depth resolve 已接通显式 MINIMUM/MAXIMUM：Vulkan RenderPass 与 Dynamic Rendering 分别生成自己的 resolve scope/encoder，DX12 在 draw fragments 后生成 CommandList1 resolve task。能力预检先于 transient 分配，保留具体 native 错误；未支持设备明确 Unsupported。源 CLEAR/LOAD、STORE/DONT_CARE、目标非零 mip/layer、同时 color/depth resolve 与部分 sample coverage 已由真实深度消费和 RGBA8 读回验证。

设备级 PSO 与 Vulkan RenderPass 缓存各自最多 128 项，LRU 淘汰只移除 cache owner。PSO key 包含强持有的 shader 身份、vertex/fixed state、附件格式/sample、device epoch，以及传统 RenderPass 的单调 scope identity/subpass；不包含每帧 descriptor、UBO offset 或 framebuffer。RenderPass key 记录附件、子 pass、resolve 模式、初末 layout 和依赖，framebuffer/views 仍归帧。缓存淘汰后在途引用仍保活，worker 不查询或写入缓存。

GraphBuildArena 以明确容量、对齐分配和逆序析构管理非平凡对象。Freeze 将图根所有权转交冻结图，内部 vector 等容器继续使用普通 RAII；builder 不再额外保留旧图，Reset 拒绝仍有消费者的 generation。native 编码收齐后，资源 bindings、shader/sampler、payload 与 encoder 转交 slot；图回执保留到提交校验，Accepted/Indeterminate 后清理 CPU 图，Rejected 保留重试，GPU 引用继续按完成/隔离协议退休。

公开 Fullscreen 通过单个 generated triangle 复用 raster 编译路径并通过三模式读回；Compute/Custom 已有明确 IR 声明，Compile 返回 Unsupported，即使无 exports 也不作为空图成功。B12 已接生产 FrameBuildContext 与 Renderer 主循环；Metal、跨队列、storage/aspect/layered 扩展不属于本次固定范围。

验证：调试捕获下完整 GPU **608/608**，RHI **133/133**，接口/CPU/资源/模块与原 core/adaptor **13/13 组 CTest**；SandboxModule/SandboxEditor 构建通过，未计 UI 运行验收。日志 `output/b10-finish-{build-final,build-last,gpu-debug,rhi,cpu}.log`。一次非调试全量在 Vulkan 初始化期间出现 0xC0000005，最后用例在捕获下重复 20 轮、三路径共 60 次通过，尚未取得崩溃 dump，保留未归因风险。本次功能 diff 从结构纠正完成后取基线，独立于 `renderer-layout-review.diff`。未 commit/archive。

## 当前可运行边界

M0 接口、M1 CPU prepare、M2 GPU slot、M3 各后端 compiler 已由 B12 公共 IRenderer 连成生产链路。B14 已默认切换 Windows AuroraModule 与编辑器；非 Windows 宿主仍保持未迁移。provider-only 模式保留，已有 legacy device 占用时 Create 明确 NOT_READY。

| 模块 | 已实现 | 尚缺 |
|---|---|---|
| 接口与 target | IRenderer、真实 provider、owned packet/IDs、阶段数据、资源/Completion、feature 注册/启停/退休；复用原 core/adaptor/runtime 和 RHI 后端目录 | Metal 宿主能力门 |
| Core ECS / dispatcher | 清池/generation 退休；访问依赖纯编译；checked DAG、drain hook；公共 Completion 单次发布、有界订阅与指定线程交付；生产 CPU/GPU arena 生命周期组合 | 动态 CPU prepare feature 接入 |
| CPU coordinator | 有界 FIFO/背压、持久 stores、冻结借用、Completion 续接；生产 Runtime 接入、逐 Scene 暂停/恢复、GPU/输出退休及模块关闭重载；B14 游戏/编辑器退出事件泵 | 更多 CPU prepare feature |
| CPU prepare | 受限访问、system/chunk 并行；Object/Bounds/point-light 增量 prepare；View 裁剪/LOD/tag、稳定 owned packets、精确版本快照；Release profiling/1024 行分块 | 更多光类型/着色、CPU 插件 prepare 注册 |
| FrameContext | Vulkan/DX12 lease、command/descriptor/staging/UBO 分区与容量、Rejected 重试、Indeterminate 隔离、完成通知与保活；显式纳管资源的候选/Accepted/completed 状态账本、拥有租约的安全自动退休、逐 mip/layer 初末状态；输出同步 owner 随提交保活 | 跨队列、Metal 分区 |
| FrozenFrameGraph | buffer/image 导入、typed copy/raster、内容/范围验证、roots/死 pass 裁剪、Freeze；成功录制的图/pass 回执；vertex/index/UBO 读取、D32 内容语义；image 导入统一、逐 mip/layer 内容依赖与完整子资源 copy；transient 声明、裁剪后分配、device pool/frame leases、同图整对象 alias/物理同步；Texture2D/Texture2DArray 显式 mip/layer view 与按范围读取、shader 类型校验；部分初始化导入、物理子资源状态、局部矩形/显式行距、跨图矩形内容接续；RGBA8/D32 显式单 mip/layer 附件 view；RGBA8 2/4/8× color MSAA/resolve 与同采样 D32 depth；D32 MIN/MAX resolve、arena 与 CPU/GPU 生命周期分离 | 更多格式/存储 bindings与跨队列状态 |
| Vulkan | 两套独立 compiler/encoder；完整及局部 mip/layer upload/readback 与 raster；同 attachment 连续 LOAD 合并为多 subpass；logical/native 映射与两种 inheritance；实际 worker/slot secondary 并行与稳定 primary 组装；编码故障/取消/teardown 验证；indexed/depth/UBO/2D 与 array view/sampler、device PSO/RenderPass cache、子资源状态及 D32 resolve | shader local read、扩展格式/存储 |
| DX12 | 独立 compiler/encoder；buffer/完整及局部 mip/layer copy、显式行距、单 color raster/readback；实际 worker/slot/ordinal DIRECT lists、稳定提交；故障/取消/teardown 验证；indexed/depth/UBO/2D 与 array view/sampler、device PSO cache、子资源状态及 D32 resolve；checked acquire/present/退休 | 更多 view/存储 bindings |
| Metal | 保留直接宿主路径 | 新 lease/Compile/Dispatch 明确 Unsupported，未作 Mac 验证 |
| 资源目录/上传 | owned payload/metadata/mesh/material、ticket/订阅、冻结精确版本；buffer/RGBA8 image 分块上传、回执核验、lease staging/预算、提交/完成、InGraph/Resident；生产 packet 路由、候选接纳、共享取消与显式重试 | 其它格式、跨队列、独立资产/World bridge |
| 输出 / 宿主 | Windows native/offscreen、surface 查询、延迟 acquire/复核、失败隔离、FrameComposer/ViewOutput、owned UI 合成与 feature 退休；Windows 游戏/编辑器 packet 循环、真实 UI/图集/圆角、loader lease、RendererHost 和 SceneProxy；非空场景宿主验收 | World 自动提取、Metal 宿主 |

**CPU prepare、Vulkan raster 与 DX12 图编码已支持并行；Metal 新协议未实现。** Vulkan parallelNum=1 不建 worker pool，采用 inline 参考；大于 1 时按实际 worker/slot 分区录制 secondary，TryCollect 收齐后由 coordinator 组装 primary。Vulkan/DX12 当前 raster 支持自有 RGBA8 color 与可选 D32 depth，每附件显式选择单 mip/layer；RGBA8 color 支持设备可用的 2/4/8× MSAA 与单采样 resolve，D32 支持与 color 匹配的单 sample 或设备可用的 2/4/8× MSAA；linear/generated 或多 float stream 的 U16/U32 indexed draw，以及 set 0/1/2 scalar UBO/dynamic offsets、RGBA8 Texture2D/Texture2DArray 与独立普通 sampler。采样只读取显式 view 暴露的 mip/layer，要求这些内容全部已定义；类型反射在 native shader 创建前拒绝未支持声明，Compile 校验 shader/view 形态匹配；同 pass 采样 attachment 明确拒绝。RenderPass 只合并不带 resolve、连续存活、相同 color/depth attachments 及各自 view 且各附件前项 STORE/后项 LOAD 的 raster；copy、attachment 切换、重新 CLEAR 或丢弃存储会拆开 scope。Dynamic Rendering 保持逐 logical raster 的 scope 与显式 barrier。

B01–B03 已完成当前子集的 scope、inheritance、secondary/worker 编码和 native 故障/取消/退出验证。每段最多 32 draws，分片数受 maxRecordingsPerSlot 和 dispatcher 队列容量限制；超额显式失败；Vulkan copy-only 仍串行，DX12 可将 copy 与 raster fragment 分别录制为 DIRECT lists。`colorLocalRead` 明确要求 shader 读取已加载的 color attachment，声明必须使用 LOAD；当前 Vulkan 两套与 DX12 compiler 均明确 Unsupported，普通 LOAD/混合不受影响。stencil resolve、更多纹理 view/存储 bindings、其它 local-read 资源与通用资源访问尚未实现；B11 已支持 leased swapchain 导入。

DX12 parallelNum=1 使用同一计划 inline 录制一份 DIRECT list；大于 1 时，各任务从实际 worker 取得独立 allocator/list，TryCollect 收齐后按 task index 交付一次有序提交。每个 raster fragment 重新绑定 RTV/DSV、PSO/root signature、UBO、vertex/index、viewport/scissor，仅首片执行对应 CLEAR。紧密或显式行距的 image 数据通过计划持有的 GPU footprint scratch 做行重排，支持未满足原生 256/512 字节对齐的逻辑行/offset；无 pass 的导出仍生成必要最终状态转换。RTV/DSV、shader-visible heaps 与 footprint scratch 归当前 lease 的 slot 分区，按预算惰性创建；计划保留引用，只有退休后的新 lease 才重置游标。编译失败回滚本次所有 descriptor/scratch 分配；共享 scratch 的状态沿 native task 顺序连续推进。

图的声明顺序是当前 GPU 顺序。Vulkan buffer HOST 边界要求 coherent；DX12 default heap 的图边界支持 COMMON/RASTER_READ，上传资源仍固定 COMMON。资源声明/真实设备描述必须一致；未支持能力显式返回错误。两套 Vulkan compiler 共用纯分析和基础命令辅助，各自构建 native scopes/PSO 和 encoder。

B10 第十子批增加 AvailableFrameBufferBytes：仅在 lease 可分配期间查询对齐后可用容量，不创建 buffer 或推进游标；分块直接消费当前 staging 余量。

B04 已完成 Vulkan/DX12 当前子集：`WriteFrameBuffer(lease, kind, bytes, alignment)` 在 Compile 前复制 staging/逐 View UBO 数据；UBO 的 slice size 含设备对齐和清零 padding。成功 Compile 到编码/录制消费期间禁止继续分配；空图不占用 plan borrower。原生 descriptor 分配由 coordinator 使用后端入口，worker 仅消费已确定的分配结果。旧 DynamicBuffer/StagingBufferAllocator 和 DX12 EnsureFrameCopy 仅保留给直接宿主，B06/B07 必须使用新入口，生产切换归 B14；B07 已实现 Compile 的 UBO descriptor resolve 和 encoder 消费，Vulkan 失败编译释放本次 descriptor sets 并恢复预算。

B10 首批的 image copy 支持 2D RGBA8、单 sample、每 pass 一个完整 mip/layer，尺寸按 mip 缩减且至少为 1；非 2 次幂与非零紧密 buffer offset 已验证。相同 image 的兼容导入返回同一 handle，冲突声明拒绝。内容依赖按 mip/layer 独立计算，整图 export 要求全部有效；initialized=true 仍是全资源承诺，initializedRanges 支持声明部分 mip/layer 有效内容，两者互斥。Vulkan/DX12 图内按子资源转换，第十四子批已支持逐 mip/layer 初末 layout；最终转换不使未初始化内容变有效。第八子批已补单 mip/layer 局部矩形与显式行距；第九子批 initializedRegions 可声明已完成的 RGBA8 矩形，与完整 initializedRanges 取并集，initialized=true 与两者互斥。内容覆盖精确到 texel 行，buffer padding 不参与访问。多 layer 单次 copy 和 layered raster 仍未支持；第十五子批允许在多 mip/layer image 中选择单 cell 作为 raster attachment。

## 代码入口

B09：CompletionSource 由 producer 在 drain 后发布一次，ContinuationExecutor 由目的线程创建并 pump；Accepted 预留容量直到回调及捕获析构，Close 拒绝新订阅并保留已接受者。注册表使用跨 DLL Singleton，ID 不复用。PrepareJobGroup 在末任务收齐时归并/释放内部 contexts，再发布不可变结果；取消跳过未进入的 kernels，已有失败优先于取消。

CpuRenderCoordinator 在自己的 executor 上执行 CpuPrepareProgram.completed；RequestCancelPrepare 与 graceful RequestShutdown 分开。CPU-only 默认失败会关闭 coordinator，生产 Runtime 可识别失败所属 Scene，收齐后暂停它并继续其它 Scene；有效输入更新允许恢复。普通 token 不保留 ECS，捕获的 frozen read view 独立阻挡下一 packet。FrameBuildContext 仅持有 owned 产品/版本；模块卸载由宿主在 RendererStopped 和 Renderer 销毁后执行，不提供任意时刻自动卸载。

RasterShaderResolver 位于 Aurora core 的 renderer/graph，在 coordinator 复用现有 ShaderResolver/ShaderBuild，将 VS/FS 的 UBO/image/sampler reflection 合并并显式标记动态绑定。Compile 接收已解析的 ShaderPtr；worker 不执行解析/编译。每个 native pass/subpass 内按 shader 身份、vertex layout、blend/depth 状态有序复用 PSO；attachment/样本/传统 subpass 兼容性由缓存作用域固定。B10 已接设备级有界跨帧缓存，固定 key 与保活规则见本页收尾摘要。

`engine/aurora/core/src/renderer/resource/ResourceCatalog.{h,cpp}` 实现 B05。`Request` 读取实际 ECS 绑定；`Supply` 只在验证及容量检查通过后消费载荷；每个 catalog/attempt 的 ticket 隔离迟到结果。`Freeze` 按同 product/usageVariant/device epoch 选择不高于 desired revision 的最高 Resident，metadata 与 backing 来自同一个版本。CPU 快照不携带 backing，coordinator/submission 独立保留 pins。

`ResourceUploader` 已接通 buffer 和 RGBA8 2D image：`Prepare` 按 ticket 顺序和剩余预算接纳分块 bytes，使用本 lease staging 和跨块保留的自有 GPU_ONLY backing，为全部 copy 设置 sideEffect root；image 按 layer 再 mip/row 紧密排列，完成前缀按内容范围导入。`ResolveInGraph` 只返回本批次/图内 buffer/image handle。各 native compiler 编码成功收齐后才生成 RecordedFrame 回执；提交核验全部 copy、源/目标/范围及固定最终状态，并拒绝其它存活 pass 写入该版本。Accepted 登记精确 SubmissionToken，PollCompleted 成功后才调用 `AdoptResident`。

Rejected 保留原录制和 CPU payload；Compile/Encode 失败或取消须收齐、丢弃未提交录制，再 `Discard` 回到 CpuReady。Indeterminate/完成错误隔离该 owner 和 in-flight 计费，原生 context 保活至安全 teardown。上传 owner 是 context completion 的唯一 poller，同时把非上传完成交还调用方。源 bytes、lease staging 和 Resident backing 分别退休。

当前上传自有完整 buffer（COMMON）及完整 RGBA8 2D image（GENERAL，DX12 COMMON），不接收状态未知的 legacy transfer 目标。B12 通过 ResourceCpuReady 接纳 owned bytes 和可选 mesh/material 数据；IO/解码/Slang 编译由生产者完成并在主线程归包，Renderer 不依赖 AssetManager。公共最小绘制支持静态 indexed mesh、set 0/1/2 scalar UBO 和 D32；B13 通过 frame feature 接入 ViewOutput Texture2D/sampler 与 UI indexed draw。完整材质资产链路不在当前最小范围。

| 职责 | 入口 |
|---|---|
| 公共 Renderer | `engine/aurora/core/include/aurora/renderer/IRenderer.h` |
| 生产创建 / 帧组合 | `engine/aurora/core/src/renderer/RendererProvider.cpp`、`RendererRuntime.cpp` |
| CPU coordinator | `engine/aurora/core/src/renderer/frame/CpuRenderCoordinator.cpp` |
| CPU 提取 / GPU 解析 | engine/aurora/core/src/renderer/prepare/PreparedScene.cpp、engine/aurora/core/src/renderer/graph/PreparedRaster.cpp |
| 公共 Completion | engine/core/include/core/async/Completion.h、engine/core/src/async/Completion.cpp |
| prepare 调度 | `engine/aurora/core/src/renderer/prepare/ScenePrepareScheduler.cpp` |
| 冻结受限访问 | `engine/aurora/core/src/renderer/scene/FrozenCpuFrame.{h,cpp}` |
| 版本目录 / 上传发布 | `engine/aurora/core/src/renderer/resource/ResourceCatalog.cpp`、`ResourceUploader.cpp` |
| 图构建 / 纯分析 | `engine/aurora/core/src/renderer/graph/FrameGraphBuilder.cpp`、`engine/aurora/rhi/interface/src/rdg/FrameGraphAnalysis.cpp` |
| lease 分配接口 | `engine/aurora/rhi/interface/include/aurora/rdg/DeviceFrameContext.h` |
| slot、buffer 分区与提交保活 | `engine/aurora/rhi/common/FrameContext.cpp` |
| DX12 编译 / 执行 | `engine/aurora/rhi/dx12/src/rdg/D3D12FrameCompiler.{h,cpp}`、`D3D12DeviceFrameContext.cpp` |
| Vulkan 编译 / 执行 | `engine/aurora/rhi/vulkan/src/rdg/Vulkan{RenderPass,DynamicRendering}Compiler.cpp`、`VulkanDeviceFrameContext.cpp` |

## 错误与生命周期约束

- 本轮涉及的 Aurora、Core ECS/TaskDispatcher 路径已移除 throw/try/catch；标准库异常暂不处理，原生 API 和业务错误仍显式返回。
- Init/Bind 返回错误，ECS Add 返回组件指针或 nullptr，kernel 使用 context.Fail/checked result；失败不发布本 system 候选，依赖跳过，独立工作收齐。
- 输入 ReadView 在 kernel 结束后仍可读取并保活；派生读写限于活跃 invocation 的执行线程。保存 view 不等于延长派生访问权限。
- CPU jobs/borrowers、GPU completion、presentation retirement 是不同回收条件；不能用取消回执、Submit Accepted 或 graphics fence 代替其它条件。
- Vulkan/DX12 错误汇总以 DeviceLost 优先，同级按 task index 保留首个原生原因；teardown 先请求取消并收齐 CPU，再安全收尾 GPU、释放 worker pools。Instance/Device 必须活到所有 context/录制退出，动态模块由 Instance 持有。

## 验收记录与工作区

当前验收以本页 B14、B13 及 B12/B11/B10 收尾结果为准。各子批与 P01 阶段的证据集中保留在 [review.md](review.md)；历史数字与当时的待办不代表当前状态。

工作区包含此前主框架、B10 收尾、B11 及独立结构纠正的未提交实现和 spec。B11 从批前 535 个文件快照生成 `output/b11-review.diff`（代码与文档）、`output/b11-production.diff`（生产 C++），文件清单为 `output/b11-files.json`。B10 和结构移动仍分别保留 `output/b10-finish-review.diff`、`output/renderer-layout-review.diff`。未 commit/archive。

B12 生产组合独立产物为 `output/b12-runtime-review.diff`、`output/b12-runtime-production.diff`、按职责分组的 `output/b12-runtime-review/` 及 `output/b12-runtime-files.json`；基于本批前快照，不要求从整个未提交工作区筛选 B12。前一 surface 子批仍保留 `output/b12-surface-review.diff`。

B14.1 独立产物为 `output/b14-review.diff`、`output/b14-production.diff`、`output/b14-review/` 和 `output/b14-files.json`，基于 B14 开始前 2153 个文件的快照。没有 commit/archive。

B14.2/B14.3 独立产物为 `output/b14-finish-review.diff`、`output/b14-finish-production.diff` 和 `output/b14-finish-review/`，基于本轮前 3654 文件快照。Windows 宿主验收已完成；后续为独立 scene-bridge 与 P02 Metal。跨平台/扩展能力仍按 [tasks.md](tasks.md) 保留未完成状态。
