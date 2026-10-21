# 平台对等待办

Status: blocked-on-prereq（缺少 Mac/原生设备验收环境；未排期）。每项保留 aurora-renderer 的原编号；已有 Windows 子集不重复计为这里的完成。

- [ ] 2.2 修正 RDG executor 的 PSO/RG 顺序与布局切换重绑；覆盖 DX12 和 Vulkan 真正的 descriptor 使用（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 2.3 重构 frame fence 状态；覆盖零输出、全部 acquire 失败、恢复和提交失败（B11/B12 Windows 组件与生产服务已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 2.4 分离 acquire/present 同步资源复用规则，定义 resize/close 的 presentation retirement（B11/B12 Windows 已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 2.5 为 RHI Submit/完成查询补可观察的 Accepted/Rejected/Error 协议；故障注入验证 reset 后拒绝与 device lost 不无限等 fence（Windows 组件及 B12 服务已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 3.4 将 acquire 移到调度器；RDG 导入 borrowed output，重复 Compile 不获取第二张图像（B11/B12 输出与 B13 owned UI 已验；B14 真实 UI/宿主已验；P02 物理旋转/非 Windows 待验收）
- [ ] 3.5 实现窗口/离屏输出、单输出失败隔离、已 acquire 后失败收尾和窗口销毁确认（B11/B12/B13 Windows 输出/合成已验；只跳过受影响输出，其它 pass/command buffers 和上传继续；B14 Windows 宿主已验；P02 待验收）
- [ ] 3.16 独立 DeviceFrameContext 领域头文件，提供 TryBeginSlot/分配/Compile/Dispatch/TryCollect/Submit/Discard/PollCompleted；校验 backend/context/slot generation/device epoch，宿主不直接驱动 RDG command buffers（B12 Windows 服务已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 3.17 统一 staging/DynamicBuffer/descriptor ring 的显式 slot 分区与容量检查；消除 renderer 路径 AdvanceFrame/Reset 自增，验证任意空帧/输出跳帧/upload-only（B04 的 Vulkan/DX12 新协议已完成；Windows 宿主 B14 已切换；Metal P02 待验收）
- [ ] 3.18 按 slot × worker × recording ordinal 建立安全 command 资源与预算；验证任务多于 worker 时不覆盖待提交结果，两帧在途不互相 reset；Metal child 按 parent 管理（B02 Vulkan 与 P01 当前 DX12 子集已验收；完整平台能力及 P02 待收口）
- [ ] 4.2 实现逐 pass set 0、逐 view/slot 常量；填完整 projection/camera position（B04 分配已完成；B07 scalar UBO/set 0 已验证、B08 projection/camera 已实际读回；P01/P02）
- [ ] 4.3 实现后端 Compile 的 descriptor resolve 或显式前置任务，发布不可变 bindings；审计 DX12 EnsureFrameCopy 等 bind 时共享写入，确保 RG 引用资源有显式 Read/Write（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 4.4 实现 transient pool frame leases 与导入资源状态；覆盖跨帧保活、同帧物理复用及重复导入（B10 Windows 固定资源范围已验；完整平台归 P01/P02）
- [ ] 4.6 实现 content version/producer、ReadWrite/WriteDiscard 与明确 roots；先内容剔除再推导存活 hazard/lifetime，覆盖中间读与 UI LOAD（B10 内容/alias 与 B13 生产 UI LOAD 已验；完整平台归 P01/P02）
- [ ] 4.7 闭合 CopyBlit 的 typed source/destination、owned regions、buffer copy/buffer-to-image emit 和对齐检查；其它 kind 不得静默无操作（B06；P01/P02）
- [ ] 4.10 各后端自行完整 Compile；公共图只保留声明/Freeze 与可复用纯分析，context 持匹配 compiler，新路径不由公共 Compile/ExecutePasses 控制（B12 Windows 三条生产路径已验；P02 待实现）
- [ ] 4.11 定义 native pass/scope 与 logical pass 映射；后端分别决定合并/拆分、GPU 顺序、lifetime/backing/同步，顺序变化后重新验证 alias（B01 scope/映射与 B10 Windows 固定范围已验收；完整平台归 P01/P02）
- [ ] 4.12 分离 EncodeTaskGraph 和 SubmissionPlan，稳定 result index、CPU edges、Worker/Coordinator executor 约束与 opaque plan 单次消费；Custom 默认串行（B02/B03 与 P01 当前 Vulkan/DX12 分片、单次消费与失败收齐已验收；完整平台能力及 P02 待收口）
- [ ] 4.15 实现 Metal 编译/dispatch：native scope 描述、parallel parent/有序 children 生命周期、autorelease/强持有；不支持并行的 scope 明确安全回退，需 Mac 平台验证（P02）
- [ ] 4.16 在各后端 FrameContext 适配 DeviceFrameDispatcher worker 子批次与 coordinator continuations；覆盖空图、单 worker、有界 fan-out、显式失败/取消 drain 和无 worker 阻塞等待（B02；P01/P02）
- [ ] 4.19 扩展 raster recipe、bindings、depth/MSAA/resolve、subresources 与 PSO/scope cache，接入两套 encoder 的稳定并行组装；串行子集不等同于本项完成（B07/B10/B12 固定范围与 B13 UI sampled bindings 已验；完整资产材质不在最小范围，剩余平台归 P01/P02）
- [ ] 4.20 明确新图 IR 的公开 pass kind 能力范围；受支持 FullScreen 发出真实命令并通过读回验证，不支持者显式 Unsupported（B10 Windows：Fullscreen 已读回；Compute/Custom 的 IR、后端实现及 Windows 验收归 `aurora-compute-custom-passes` 的阶段交付。本项承接其未完成的 6.3/6.4/10.11：macOS 编译、能力错误码与真实设备读回，覆盖 buffer alignment reflection、动态偏移、线程组限制、storage view、只读 storage image/buffer、Custom 失败后的 discard/复用及 shader/sampler/callback 退休保活；验收记录回填 P02，并引用阶段归档）
- [ ] 5.1 实现程序化 indexed mesh 和简单 Slang VS/FS，通过 owned packet 提交与上传（B07 当前 Vulkan/DX12 子集已验证；完整平台/生产条件仍待收口）
- [ ] 5.2 接入最小 ShaderResolver/PSO cache，key 覆盖 program/layout/vertex/state/attachment（B07 前置解析与 pass/subpass 内缓存、B10 Windows device 跨帧缓存已验；完整平台/生产条件待收口）
- [ ] 6.4 在平台 probe 中确定 Metal 线程和 drawable 协议；未通过时明确保持未迁移状态（P02）
- [ ] 7.4 生命周期测试覆盖最小化单窗/全部窗口、恢复、resize、close、资产迟到和 device/surface 错误（B11/B12 Windows 组件与公共服务已验，含配置注入/8 种纯映射；B14 owned 资产/真实宿主已验；P02 物理旋转与非 Windows 待验收）
- [ ] 7.5 按根 AGENTS 关闭编辑器后配置并构建相关 targets；运行相关测试和有界宿主验收，记录各平台结果（各批与最终收口）
- [ ] 7.11 验证同图首次上传后真实 draw/readback、无窗口 upload-only、预算延后、取消共享 ticket、热更新与过期 device epoch（B06/B07/B12 Windows 公共服务已验；P02 待验收）
- [ ] 7.12 验证慢/不可用输出期间控制流继续，prepare 后 resize 不沿用旧 ViewContext；记录后端可退出等待与呈现退休限制（B11/B12 Windows 配置竞态与公共排队 resize、最小化继续提交已验；B14 Windows 宿主已验；P02 待验收）
- [ ] 7.13 对三个后端执行相同内容/剔除语义用例，并对串行/并行 dispatch 做实际 readback 对照；记录未验证的平台，不以公共 fallback 代替后端实现（B01/B02/B10；P01/P02）
- [ ] 7.14 打乱编码任务完成顺序，验证 GPU copy→draw→UI 同步、透明片段稳定组装、task 数大于 worker、同 slot 多结果和两帧在途（B02/B03 分片乱序/双 slot、B07 bindings、B13 copy→UI 与 97 draws 稳定顺序已验；完整平台仍待 P02）
- [ ] 7.15 故障注入 Compile/dispatch 部分失败、错误 context/epoch/slot、重复 Dispatch/Submit 与取消；确保 drain 前不 reset、失败不发布上传、coordinator-only 节点线程正确（B03 与 P01 的 Vulkan/DX12 native 错误/取消/退出已验收；B06 上传发布及完整平台能力待收口）
- [ ] 8.3 实现 CpuReady/UploadQueued/UploadSubmitted/Resident 状态与 graph-local InGraph resolve；不把 create 成功或 Submit Accepted 当成 Resident（B06/B10/B12 Windows buffer/image 与生产组合已验；Metal P02）
- [ ] 8.4 实现 slot-owned staging、UploadBatch 的字节/创建数量/在途预算及显式持久上传 roots，支持 upload-only submission（B06/B10/B12 Windows 组件与生产预算/后台分块已验；Metal P02）
- [ ] 8.5 实现实际录制 receipts、提交接受后的状态登记、完成后的 Resident 发布和源 bytes/staging/GPU leases 分阶段释放（B06/B10/B12 Windows 组件与统一 Submit/Poll 已验；Metal P02）
- [ ] 8.6 实现精确版本 pins、冻结前旧版本选择与延迟释放；验证新版本失败不覆写旧在途 descriptor/mesh，未匹配状态的 legacy transfer 资源不得直接使用（B05–B10 组件与 B12 公共热更新/逐帧 layouts/pins 已验；Metal P02）
- [ ] 8.11 验证 `fix-aurora-pass-resource-contract` 的生成 Pass 接入：macOS 编译及 MSL 真实布局/字段偏移、Global/Pass/Batch 与 native buffer index 映射、有/无 View、不同 Pass 布局、两 View/双在途帧、实际 uniform 字节范围、显式场景注册及模块退休；iOS/Android 交叉编译使用指定 host ShaderHeaderTool 并做实机读回。Windows 结果不能代替本项，旧 8.9/8.10 的宿主证据也不覆盖此次 ABI 变更。

实施顺序：native probe → slot/提交 → compiler/encode → 资源/绘制 → 输出/宿主 → 故障与实机验收。
- [x] 8.7 在 Metal/non-Windows 原生宿主 capability gate 通过后，把 `AuroraModule` 的 `Init/Start/Tick/Shutdown` 统一到 `RendererHost` + `IRendererProvider`；删除 `#if !defined(_WIN32)` 宿主分叉。
- [x] 8.8 删除 `InitLegacy/StartLegacy/TickLegacy`、`legacy` 标志、`mDevice`/`mFrameContext`/`mCommandPool`/`mCommandBuffer`/`mViewport` 及仅 legacy 使用的 include；确认 `AuroraModule.h` 不再声明平台分叉成员。
- [x] 8.9 增加非 Windows 宿主生命周期测试：初始化、提交、drain、shutdown 与 close deferral；记录真实 Cocoa/CAMetalLayer 窗口证据。resize/失败注入仍由 7.4 跟踪。
- [x] 8.10 全量构建并跑相关测试，确认 `Aurora.Renderer.ModuleTest`/`StaticModuleTest` 在目标平台通过后再移除 legacy 分支。

## Host unification verification

- `AuroraModule` now uses `RendererHost` + `IRendererProvider` on every platform; the non-Windows `InitLegacy/StartLegacy/TickLegacy` path, `legacy` flag and direct device/viewport members were removed.
- Added the Cocoa native host case to `RendererModuleTest.NativeLauncher...`: `Platform::Init`, real `CocoaWindow`/CAMetalLayer, module load, Start/Tick, frame completion, close deferral and module unload all passed on macOS.
- `Aurora.Renderer.ModuleTest`: 5/5 passed on macOS (including the native host case).
- `Aurora.RHITest`: 142/142 passed; full `ctest`: 49/49 passed on the clean run (`FrameworkTest` showed a pre-existing concurrent asset-load flake and passed alone).
- Windows-specific `HWND`/`IsWindow` checks remain under `_WIN32`; iOS/Android real-device host validation remains tracked by their platform changes.
