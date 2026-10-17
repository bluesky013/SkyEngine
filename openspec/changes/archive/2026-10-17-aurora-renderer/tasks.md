# Aurora Renderer 验收任务

当前能力见 [status.md](status.md)，下一批及依赖见 [batches.md](batches.md)。本文件只维护验收 checklist；历史实施过程与日志见 [review.md](review.md)。

归档准备：本次只交付 Windows 已验收范围。37 个未完成条目保留原编号，具体剩余条件与移交位置见 [closeout.md](closeout.md)；不因移交而勾选。

## 使用规则

- 保留原 1.1–8.6 编号，避免历史 review/design 引用失效；新增 4.20 显式覆盖公开 pass kinds 的能力边界。
- 行末括号为负责批次；涉及多批或多平台的任务只有全部验收完成后才勾选。未勾选不表示完全没有代码，实际子集以 status 为准。
- B01–B09 已完成各自列明的当前子集；B10 Windows 三路径、B11 输出、B12 生产服务和 B13 owned UI/feature 合成已验收。B14 Windows 宿主及 P01 固定范围已验收；P02 Metal、完整资产材质和未完成的跨平台任务仍保持未勾选。
- 每批完成代码、必要测试和对应 change 产物后，才整理具体提交供用户确认；不自动 commit/archive，不单独提交同一 change 的 spec。
- B10 首批已实现 image 导入统一与完整 mip/layer copy/内容依赖；第二子批已接入 transient 声明、device pool 和 frame leases；第三子批已实现同图整对象 alias 与三套 compiler 的物理同步；第四子批已接通完整 RGBA8 2D image 上传/Resident，第五子批已验证 scalar Texture2D/sampler 与双在途 descriptor 保活，第六子批已接通类型反射、2D/2D-array 显式 mip/layer view、按暴露范围读取及 native view 退休。第七子批已补原生子资源状态/部分初始化导入，仍使用统一图边界 layout；第八子批已接局部矩形/显式行距与精确内容覆盖，第九子批已接跨图矩形内容导入及补齐后的读取，第十子批已接 RGBA8 image 分块/完成进度及有界部分 backing，第十一子批已接 buffer 部分字节与共用分块事务，第十二子批已接单 context/graphics queue 的显式资源状态账本，第十三子批已接 uploader 自动纳管、Resident 租约转交与安全回收，第十四子批已接逐 mip/layer 图边界和 accepted/completed 接续，第十五子批已接 RGBA8/D32 单 mip/layer/sample 附件 view、精确内容依赖与原生 view 退休，第十六子批已接 RGBA8 2/4/8× color MSAA 与 pass 内 resolve，第十七子批已接匹配 sample count 的 D32 MSAA depth、独立能力预检与串并行深度绘制；第十八子批完成 depth resolve 设计，连续收尾已实现三条原生路径、128 项 device PSO/RenderPass 缓存、GraphBuildArena/提交资源移交及公开 pass 能力边界。B10 固定范围完成，跨队列、其它格式/存储扩展和 Metal 不计入本次验收。
- 本 change 不包含原生后端目录搬迁/重命名；保留 `engine/aurora/rhi` 下的功能 diff，后续独立 change 再处理结构，不新增本批完成项。

## 里程碑与批次的关系

| 里程碑 | 目标边界 | 后续收口批次 |
|---|---|---|
| M0 | 接口和模块边界 | 已有基线；后续契约改动在所属批次同步消费者 |
| M1 | CPU 场景、PreparedFrame、Completion/取消 | B05/B08/B09/B12 |
| M2 | 唯一 GPU slot、分配与输出退休 | B04/B11/B12、P01/P02 |
| M3 | 各后端完整图编译与串行参考 | B01/B06/B07/B10、P01/P02 |
| M4 | 上传、真实绘制与多 View | B05–B08/B13 |
| M5 | 后端独立的并行编码 | B02/B03、P01/P02 |
| M6 | 游戏/编辑器迁移与完整生命周期 | B12–B14、适用平台验收 |

M0–M6 是验收维度，不是新的执行顺序；B14 Windows 已收口，后续 scene-bridge 与 P02 分别推进。

## 1. 接口与模块边界

- [x] 1.1 定义 IRenderer、Scene/Entity/View/Viewport IDs、描述与完成事件；明确 generation 和回收策略（基线）
- [x] 1.2 定义 FramePacket 的所有权、FIFO、Busy/backpressure、Init/Destroy/Shutdown 结果协议（基线）
- [x] 1.3 建立仅接口 target；实现归 Aurora.Core.Static，Framework 注册归 Aurora.Adaptor.Static，沿用 Aurora.Render 入口；检查接口不链接实现及静态模块兼容（结构纠正由 aurora-renderer-layout 验证）
- [x] 1.4 校准本机 3RD_PATH、SKY_DEVELOP、SKY_BUILD_TEST、SKY_BUILD_SANDBOX；记录可复现配置（基线）

## 2. 正确性与提交协议

- [x] 2.1 修复 EntityRegistry 删除清池；覆盖 View 不再访问旧 entity、generation 复用和迟到更新（基线）
- [ ] 2.2 修正 RDG executor 的 PSO/RG 顺序与布局切换重绑；覆盖 DX12 和 Vulkan 真正的 descriptor 使用（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 2.3 重构 frame fence 状态；覆盖零输出、全部 acquire 失败、恢复和提交失败（B11/B12 Windows 组件与生产服务已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 2.4 分离 acquire/present 同步资源复用规则，定义 resize/close 的 presentation retirement（B11/B12 Windows 已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 2.5 为 RHI Submit/完成查询补可观察的 Accepted/Rejected/Error 协议；故障注入验证 reset 后拒绝与 device lost 不无限等 fence（Windows 组件及 B12 服务已验；B14 Windows 宿主已验；P02 待验收）

## 3. Renderer 主体与生命周期

- [x] 3.1 实现主线程 writer、有界 packet 通道、渲染线程、错误事件和有序 shutdown（B12 Windows 三路径公共入口）
- [x] 3.2 实现 SceneStore/ViewStore/ViewportStore 与 generation 校验；保证每帧 scene 更新一次（B12）
- [x] 3.3 FrameScheduler 接入现有 DeviceFrameContext 的唯一 GPU slot ring；显式 lease 统一 command/UBO/descriptor/staging 与 GPU 引用退休，不另建宿主 command ring（B12 Windows）
- [ ] 3.4 将 acquire 移到调度器；RDG 导入 borrowed output，重复 Compile 不获取第二张图像（B11/B12 输出与 B13 owned UI 已验；B14 真实 UI/宿主已验；P02 物理旋转/非 Windows 待验收）
- [ ] 3.5 实现窗口/离屏输出、单输出失败隔离、已 acquire 后失败收尾和窗口销毁确认（B11/B12/B13 Windows 输出/合成已验；只跳过受影响输出，其它 pass/command buffers 和上传继续；B14 Windows 宿主已验；P02 待验收）
- [x] 3.6 定义 Core 通用 AccessDesc/SystemDesc/DependencyPlan；覆盖 owner/type/resource identity、Read/Write 和 phase/Before/After（基线）
- [x] 3.7 实现纯依赖编译与串行参考执行器，验证 RAW/WAR/WAW、稳定顺序、重复声明、环和诊断（基线）
- [x] 3.8 增加冻结前 preflight、预绑定 const/受限 write views；禁止 worker 触发池懒创建或结构变更（基线）
- [x] 3.9 实现 ScenePrepareScheduler/JobGroup，复用 Core TaskDispatcher 到专用 ThreadPool；补显式失败收齐并验证节点预算与空图/单 worker/大 fan-out 进展性（基线）
- [x] 3.10 实现 element-wise 分块、任务私有 scratch、稳定 merge 与 deferred structural commands；下游等待完整 system（首版结构命令覆盖已注册渲染实体的 create/update/destroy）（基线）
- [ ] 3.11 注册 Bounds/Light/Object 等 CPU prepare 及只读 View cull/LOD/extract；join 后在 coordinator 完成 GPU ResourceFinalize 与建图（B08/B12 当前子集已验；B13 frame feature 不等同于动态 CPU prepare feature 注册，更多 light/着色仍待后续扩展）
- [x] 3.12 覆盖显式失败传播、下游跳过、Scene 暂停/重建，以及下一 packet、销毁、arena reset、模块卸载前的 job join（B09/B12 已验 CPU 借用、Scene 恢复和 Renderer 模块卸载；B13 frame feature 与 B14 真实宿主 loader lease/依赖/退出均已验）
- [x] 3.13 固定 CompletionToken/JobGroup 状态、结果发布、订阅竞态和指定 executor 的 continuation；保持 kernel 不挂起，不实现协程 runtime（B09）
- [x] 3.14 实现持久 ECS 增量构建、输入/派生版本 stamp、冻结资源绑定快照与只含 CPU 数据的 draw packets；验证旧 mesh 回退与 bounds 一致（B05/B08：精确 Resident/回退版本、输入/派生 stamp 和 owned packets 已验证）
- [x] 3.15 分离 CancelRequested、kernels drain 与结果 consumer leases，覆盖取消后下一 packet、Scene destroy 和 arena reset（B09/B10 组件及 B12 生产组合已验证）
- [ ] 3.16 独立 DeviceFrameContext 领域头文件，提供 TryBeginSlot/分配/Compile/Dispatch/TryCollect/Submit/Discard/PollCompleted；校验 backend/context/slot generation/device epoch，宿主不直接驱动 RDG command buffers（B12 Windows 服务已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 3.17 统一 staging/DynamicBuffer/descriptor ring 的显式 slot 分区与容量检查；消除 renderer 路径 AdvanceFrame/Reset 自增，验证任意空帧/输出跳帧/upload-only（B04 的 Vulkan/DX12 新协议已完成；Windows 宿主 B14 已切换；Metal P02 待验收）
- [ ] 3.18 按 slot × worker × recording ordinal 建立安全 command 资源与预算；验证任务多于 worker 时不覆盖待提交结果，两帧在途不互相 reset；Metal child 按 parent 管理（B02 Vulkan 与 P01 当前 DX12 子集已验收；完整平台能力及 P02 待收口）
- [x] 3.19 分离 CpuPrepareFrame/GraphBuildArena 与 GPU slot 资源；验证非平凡对象析构、record borrowers 及 GPU 引用移交后再 reset（B09/B10 生命周期与 B12 owned FrameBuildContext、排队 resize/资源版本保活已验证）

## 4. RDG 与 Pipeline

- [ ] 4.1 实现显式 ViewRenderContext，迁移 Scene/Opaque/Fullscreen pass；逐 View 保存可变状态（B08/B12 解析桥、B10 Fullscreen、B13 FrameRenderTarget/FrameComposer 已验；B14 Windows 宿主已切换最小合成；Metal P02 待验收，HDR 不属于最小合成范围）
- [ ] 4.2 实现逐 pass set 0、逐 view/slot 常量；填完整 projection/camera position（B04 分配已完成；B07 scalar UBO/set 0 已验证、B08 projection/camera 已实际读回；P01/P02）
- [ ] 4.3 实现后端 Compile 的 descriptor resolve 或显式前置任务，发布不可变 bindings；审计 DX12 EnsureFrameCopy 等 bind 时共享写入，确保 RG 引用资源有显式 Read/Write（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 4.4 实现 transient pool frame leases 与导入资源状态；覆盖跨帧保活、同帧物理复用及重复导入（B10 Windows 固定资源范围已验；完整平台归 P01/P02）
- [x] 4.5 验证多 writer/read 的图依赖、失效输出剔除、真实输出 extent 与错误上报（B13 Windows 三路径）
- [ ] 4.6 实现 content version/producer、ReadWrite/WriteDiscard 与明确 roots；先内容剔除再推导存活 hazard/lifetime，覆盖中间读与 UI LOAD（B10 内容/alias 与 B13 生产 UI LOAD 已验；完整平台归 P01/P02）
- [ ] 4.7 闭合 CopyBlit 的 typed source/destination、owned regions、buffer copy/buffer-to-image emit 和对齐检查；其它 kind 不得静默无操作（B06；P01/P02）
- [x] 4.8 实现 PreparedFrame/FrameBuildInputs/FrozenFrameGraph/BackendCompiledFrame/RecordedFrame/SubmissionRecord 接缝；资源账本仅在 Accepted 推进，失败编译/录制不发布成功（B12 Windows 公共入口）
- [x] 4.9 定义并实现消费方 frame feature 注册/建图/停用/退休接口及 UI 逻辑纹理解析；覆盖过期 generation 和 module lease 退休（B13；真实宿主 loader 接线已由 6.3/B14 验证）
- [ ] 4.10 各后端自行完整 Compile；公共图只保留声明/Freeze 与可复用纯分析，context 持匹配 compiler，新路径不由公共 Compile/ExecutePasses 控制（B12 Windows 三条生产路径已验；P02 待实现）
- [ ] 4.11 定义 native pass/scope 与 logical pass 映射；后端分别决定合并/拆分、GPU 顺序、lifetime/backing/同步，顺序变化后重新验证 alias（B01 scope/映射与 B10 Windows 固定范围已验收；完整平台归 P01/P02）
- [ ] 4.12 分离 EncodeTaskGraph 和 SubmissionPlan，稳定 result index、CPU edges、Worker/Coordinator executor 约束与 opaque plan 单次消费；Custom 默认串行（B02/B03 与 P01 当前 Vulkan/DX12 分片、单次消费与失败收齐已验收；完整平台能力及 P02 待收口）
- [x] 4.13 实现 Vulkan 编译/dispatch：dynamic rendering inheritance、secondary chunk 录制、primary 同步与稳定组装；parallelNum=1 走同协议参考路径（B02）
- [x] 4.14 实现 DX12 编译/dispatch：独占 DIRECT list/allocator、pass 操作与资源状态、收齐后的列表顺序；验证拆分不重复 clear、不丢 LOAD/resolve（B10 已验 RGBA8/D32、MSAA/resolve 与 DIRECT lists；B11/B12 输出与 B14 生产宿主均已验，P01 固定范围收口）
- [ ] 4.15 实现 Metal 编译/dispatch：native scope 描述、parallel parent/有序 children 生命周期、autorelease/强持有；不支持并行的 scope 明确安全回退，需 Mac 平台验证（P02）
- [ ] 4.16 在各后端 FrameContext 适配 DeviceFrameDispatcher worker 子批次与 coordinator continuations；覆盖空图、单 worker、有界 fan-out、显式失败/取消 drain 和无 worker 阻塞等待（B02；P01/P02）
- [x] 4.17 Vulkan 双 compiler 串行基线：固定 context mode，各自生成 encoder/scope/PSO；typed 全图 upload、color CLEAR/LOAD/STORE、draw/readback、native 失败与计划保活验收（基线）
- [x] 4.18 扩展 Vulkan RenderPass compiler 的 pass fusion、多 subpass、attachment 兼容性及 secondary inheritance；独立验证 Dynamic Rendering scope 拆分、local-read 能力门与 secondary inheritance（B01 scope/映射与 B02 实际 secondary 已验收；当前单 RGBA8 color 子集，local read 明确拒绝）
- [ ] 4.19 扩展 raster recipe、bindings、depth/MSAA/resolve、subresources 与 PSO/scope cache，接入两套 encoder 的稳定并行组装；串行子集不等同于本项完成（B07/B10/B12 固定范围与 B13 UI sampled bindings 已验；完整资产材质不在最小范围，剩余平台归 P01/P02）
- [ ] 4.20 明确新图 IR 的公开 pass kind 能力范围；受支持 Compute/FullScreen/Custom 发出真实命令并通过读回验证，不支持者显式 Unsupported，Custom 遵守声明访问和 coordinator 约束（B10 Windows：Fullscreen 已读回，Compute/Custom 显式 Unsupported；跨平台归 P01/P02）

## 5. 真实出图

- [ ] 5.1 实现程序化 indexed mesh 和简单 Slang VS/FS，通过 owned packet 提交与上传（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 5.2 接入最小 ShaderResolver/PSO cache，key 覆盖 program/layout/vertex/state/attachment（B07 前置解析与 pass/subpass 内缓存、B10 Windows device 跨帧缓存已验；完整平台/生产条件待收口）
- [x] 5.3 实现 worker 的 CPU draw packets 提取与 coordinator 的 DrawItem 组装；覆盖裁剪、tag、排序和实际 vertex streams/index type（B08）
- [x] 5.4 验证同 Scene 两相机、不同尺寸窗口/离屏输出、第二 Scene 和连续变换/删除（B13 Windows 公共 Renderer 与既有状态回归）

## 6. 宿主对接

- [x] 6.1 AuroraModule 通过 IRenderer 转发，新增 RenderSceneProxy 生命周期 seam；不提前实现全部组件桥接（B14.1 Windows；未迁移平台保留整套旧宿主）
- [x] 6.2 编辑器生成拥有数据的 UI snapshot，通过渲染端扩展合成离屏图像；移除直接调用 shell 的渲染线程路径（B14）
- [x] 6.3 更新模块加载/卸载依赖，验证游戏和编辑器分别只创建一个 Renderer/Device（B14 已验证 runtime/editor 单实例、动态/静态模块退出、真实 feature loader lease 与消费者依赖）
- [ ] 6.4 在平台 probe 中确定 Metal 线程和 drawable 协议；未通过时明确保持未迁移状态（P02）
- [x] 6.5 完成迁移平台的旧宿主编排入口清理，确认已删除的空 Renderer/RenderDeviceExclusive 不再被依赖，同步 Aurora AGENTS 的生命周期说明（B14）

## 7. 跨批与平台验收

- [x] 7.1 CPU 测试覆盖 packet 顺序、generation、队列满、Scene/View 删除与失败退出（B12）
- [x] 7.2 RDG 测试覆盖双 view 常量、PSO/RG 顺序、资源保活、离屏到合成依赖；不能只检查 Compile 结构（B13 三模式像素读回）
- [x] 7.3 Vulkan/DX12 真出图 smoke/readback 覆盖两个相机、两个以上 inflight slot、不同 attachment format（Windows：B14 完整 GPU 中的双 View、双 slot、RGBA8/D32 实际读回已验；证据见 closeout.md）
- [ ] 7.4 生命周期测试覆盖最小化单窗/全部窗口、恢复、resize、close、资产迟到和 device/surface 错误（B11/B12 Windows 组件与公共服务已验，含配置注入/8 种纯映射；B14 owned 资产/真实宿主已验；P02 物理旋转与非 Windows 待验收）
- [ ] 7.5 按根 AGENTS 关闭编辑器后配置并构建相关 targets；运行相关测试和有界宿主验收，记录各平台结果（各批与最终收口）
- [x] 7.6 更新设计中的未决平台结论并执行 openspec validate aurora-renderer --strict --no-interactive（各批与最终收口）
- [x] 7.7 CPU 依赖测试覆盖同类型不同 Scene、跨 View 只读、共享资源别名、保守 query 冲突、结构冻结和声明越权（Core 依赖 9/9，FrozenWriteView/prepare 回归已通过；归档前核对见 closeout.md）
- [x] 7.8 对照串行与并行 prepare 的 bounds/lights/visibility/LOD/draw 输出；打乱 job 完成顺序并记录浮点容差与稳定排序策略（B08）
- [x] 7.9 用至少两个 worker 和受控同步验证无依赖 system/chunks 可并发；通过代表性 scene profiling 确定分块阈值和任务预算，记录 prepare/同步开销（B08 受控并发；B12 Release 8192 实体/双 View、1/4 workers、256/1024 行各 5 轮；生产 1024 行，阶段成本见 review）
- [x] 7.10 验证完成前后订阅、取消/完成竞争、coordinator 恢复线程与 shutdown 期间保活；确认资源未就绪不阻塞整个 SceneReady（B09）
- [ ] 7.11 验证同图首次上传后真实 draw/readback、无窗口 upload-only、预算延后、取消共享 ticket、热更新与过期 device epoch（B06/B07/B12 Windows 公共服务已验；P02 待验收）
- [ ] 7.12 验证慢/不可用输出期间控制流继续，prepare 后 resize 不沿用旧 ViewContext；记录后端可退出等待与呈现退休限制（B11/B12 Windows 配置竞态与公共排队 resize、最小化继续提交已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 7.13 对三个后端执行相同内容/剔除语义用例，并对串行/并行 dispatch 做实际 readback 对照；记录未验证的平台，不以公共 fallback 代替后端实现（B01/B02/B10；P01/P02）
- [ ] 7.14 打乱编码任务完成顺序，验证 GPU copy→draw→UI 同步、透明片段稳定组装、task 数大于 worker、同 slot 多结果和两帧在途（B02/B03 分片乱序/双 slot、B07 bindings、B13 copy→UI 与 97 draws 稳定顺序已验；完整平台仍待 P02）
- [ ] 7.15 故障注入 Compile/dispatch 部分失败、错误 context/epoch/slot、重复 Dispatch/Submit 与取消；确保 drain 前不 reset、失败不发布上传、coordinator-only 节点线程正确（B03 与 P01 的 Vulkan/DX12 native 错误/取消/退出已验收；B06 上传发布及完整平台能力待收口）

## 8. 资源请求、上传与退休

- [x] 8.1 定义 owned RenderResourcePayload、ResourceKey/Ticket/Subscriber/VersionLease；以程序化数据验证，不引入 Renderer 对 AssetManager 的依赖（B05/B06/B10 组件与 B12 owned mesh/material/image packet 组合）
- [x] 8.2 实现共享请求去重、generation/bindingRevision/device epoch 校验、取消订阅与错误/重试结果（B05/B06/B10 组件及 B12 公共取消/过期 epoch/热更新事件与显式 retry attempt）
- [ ] 8.3 实现 CpuReady/UploadQueued/UploadSubmitted/Resident 状态与 graph-local InGraph resolve；不把 create 成功或 Submit Accepted 当成 Resident（B06/B10/B12 Windows buffer/image 与生产组合已验；Metal P02）
- [ ] 8.4 实现 slot-owned staging、UploadBatch 的字节/创建数量/在途预算及显式持久上传 roots，支持 upload-only submission（B06/B10/B12 Windows 组件与生产预算/后台分块已验；Metal P02）
- [ ] 8.5 实现实际录制 receipts、提交接受后的状态登记、完成后的 Resident 发布和源 bytes/staging/GPU leases 分阶段释放（B06/B10/B12 Windows 组件与统一 Submit/Poll 已验；Metal P02）
- [ ] 8.6 实现精确版本 pins、冻结前旧版本选择与延迟释放；验证新版本失败不覆写旧在途 descriptor/mesh，未匹配状态的 legacy transfer 资源不得直接使用（B05–B10 组件与 B12 公共热更新/逐帧 layouts/pins 已验；Metal P02）
