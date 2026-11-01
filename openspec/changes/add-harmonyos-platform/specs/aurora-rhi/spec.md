## ADDED Requirements

### Requirement: Explicit shader enum boundaries

Contiguous shader cache enums SHALL define an exclusive END sentinel without changing existing serialized values. Binary and JSON readers and associated validators SHALL use that boundary instead of the last concrete enumerator. Narrow enum types SHALL be range-checked before casting. Valid enum ranges SHALL NOT replace supported-subset or bit-mask validation.

#### Scenario: Sentinel or out-of-range serialized value
- **WHEN** a shader artifact or index contains END or a larger raw enum value
- **THEN** decoding SHALL fail without replacing the caller's valid output

#### Scenario: Existing platform and vertex type compatibility
- **WHEN** existing Linux/HarmonyOS artifacts or supported numeric vertex scalar types are decoded
- **THEN** their existing values SHALL remain accepted, and BOOL vertex input SHALL remain rejected

### Requirement: OHOS surface in the existing Vulkan backend

Aurora SHALL create an OHOS Vulkan surface inside the existing `Aurora.Vulkan` backend using `VK_OHOS_surface`, `vkCreateSurfaceOHOS` and the window's live `OHNativeWindow`. Headers and native declarations SHALL be compatible with the selected SDK. Required extensions, function pointers and queue presentation support SHALL be checked explicitly.

#### Scenario: Supported native window
- **WHEN** an OHOS window and all required instance/device capabilities are available
- **THEN** the existing Vulkan backend SHALL create its surface and swapchain from that window and present through the normal RendererHost path

#### Scenario: Unsupported surface extension or entry point
- **WHEN** the loader omits the OHOS surface extension or its creation function
- **THEN** initialization SHALL fail with a diagnostic identifying the missing capability and release partially created objects

### Requirement: Preserve frame capability and retirement contracts

OHOS SHALL meet the selected Vulkan compiler and checked SwapChain capability requirements, including applicable synchronization and presentation-retirement support. Unsupported capabilities SHALL be reported explicitly; the platform SHALL NOT silently switch to the legacy frame protocol or claim support from SDK header availability.

#### Scenario: Optional swapchain maintenance extension absent
- **WHEN** the device supports core swapchain and renderer synchronization but omits maintenance1
- **THEN** the existing Vulkan backend SHALL select its checked core WSI compatibility path without enabling unavailable extensions, preserving per-image semaphore ownership, deferred generation retirement and explicit shutdown limitations; OHOS-specific Surface code SHALL NOT implement a second renderer

#### Scenario: Required device feature absent
- **WHEN** the physical device lacks a feature required by the selected frame/presentation path
- **THEN** startup SHALL report the missing feature and SHALL NOT enter a frame loop that cannot safely retire submissions

#### Scenario: Resize or surface loss with work in flight
- **WHEN** resize, backgrounding or surface destruction occurs with GPU or presentation work in flight
- **THEN** the host SHALL stop acquiring the old output, preserve submitted resource lifetimes until safe retirement, and resume only on the current generation

#### Scenario: No drawable acquired
- **WHEN** a frame cannot acquire a usable output
- **THEN** the frame SHALL follow the existing skip/discard protocol without waiting on an unsignaled fence or presenting an invalid image

### Requirement: Real device rendering evidence

HarmonyOS Vulkan acceptance SHALL include actual device capability output, rendered content and lifecycle results. Cross-compilation, ELF inspection and generic desktop tests SHALL be recorded separately from device acceptance.

#### Scenario: SDK-only validation
- **WHEN** only SDK compilation and native library linking have been executed
- **THEN** the record SHALL mark Vulkan runtime, presentation and device lifecycle acceptance as unverified

## MODIFIED Requirements

### Requirement: SwapChain 显式获取与呈现退休

SwapChain SHALL 为 checked 帧提供有界 acquire、可重试 present、未呈现图像处置、resize/close 和明确的帧退休结果，保留具体原生错误。直接宿主协议与 checked 协议 SHALL 互斥。帧退休后仍被 presentation 使用的资源 SHALL 由后端继续持有；graphics completion SHALL 不单独充当 presentation 退休证明，超时不表示成功或安全释放。

RHI SHALL 提供不 acquire 的显式 surface 查询结果，保留查询错误并区分 surface 有效性、暂停、所选配置需要更新/重建与设备错误。后端 SHALL 根据最新能力与输出策略比较已应用配置，检查 extent、transform 及其它相关创建约束；不得仅检查尺寸或在查询失败时返回旧尺寸作为成功结果。renderer SHALL 不依赖 native caps 结构；更新与销毁仍受在途责任约束。
查询结果 SHALL 包含规范化的 extent/format/transform 和成功创建代次；可用结果表示已应用配置，需要重建的结果表示待应用配置，失败结果 SHALL 不作为准备快照。创建代次 SHALL 不随重复查询增加。

#### Scenario: Vulkan transform 变化但尺寸不变
- **WHEN** 策略选择 preTransform=currentTransform，查询得到新的 currentTransform 且 extent 与当前 swapchain 相同
- **THEN** 后端仍报告配置需要重建，校验所选 transform 的支持性，并向 renderer 提供一致的输出映射；能力枚举顺序变化或不影响所选配置的新增能力不单独触发重建

#### Scenario: surface 查询失败与未指定尺寸
- **WHEN** 原生 surface 查询失败，或 Vulkan currentExtent 为未指定值
- **THEN** 查询失败保留原生错误；未指定尺寸按平台 drawable 像素尺寸和能力范围解析，不能把未指定值当真实 extent，也不能用旧 swapchain 尺寸隐藏变化

#### Scenario: acquire 返回 Suboptimal
- **WHEN** 原生 acquire 成功取得图像但报告 Suboptimal
- **THEN** 保留该图像和同步的完整责任并安排或立即进行配置复核，不丢弃成功结果；相同有效配置不因重复 Suboptimal 每帧无条件重建，OutOfDate 与 SurfaceLost 继续走各自错误路径

#### Scenario: Vulkan 正常呈现与未提交释放
- **WHEN** 使用支持 swapchain-maintenance1 的 Vulkan 设备获取输出
- **THEN** 获取信号、消费提交、presentation fence 和 CPU 借用分别管理；正常复用等待全部责任完成。未提交图像在 acquire fence 确认后显式释放，不等待从未入队的 render-done signal

#### Scenario: Vulkan optional maintenance capability is absent
- **WHEN** core swapchain and required renderer synchronization are available but maintenance1 dependencies, feature or release command are absent
- **THEN** the checked protocol SHALL select the core WSI compatibility path, omit unsupported extension names/features/present structures, and keep present wait semaphores per swapchain image. GPU completion and CPU borrower exit MAY retire the application frame while native presentation resources remain owned by its swapchain generation; semaphore reuse SHALL follow reacquisition synchronization for that same image

#### Scenario: Core WSI discard and resize
- **WHEN** an acquired frame is abandoned without maintenance1, or an output needs a replacement swapchain
- **THEN** an abandoned image SHALL invalidate its generation after acquire completion without waiting on an unsignaled render semaphore or presenting undefined contents. Once application borrowers retire, replacement SHALL retain old swapchains and presentation semaphores until a signaled acquire fence proves a present on a successor swapchain completed. Outstanding generations SHALL be bounded and capacity exhaustion SHALL be an explicit error

#### Scenario: Core WSI final close
- **WHEN** an unextended swapchain closes with no future acquisition available as presentation evidence
- **THEN** the backend SHALL check a serialized presentation-queue drain before teardown, preserve native failure results, and document this as the core WSI shutdown compatibility convention rather than a present-fence or display-completion guarantee. The maintenance1 path SHALL continue to use its independent presentation fences

#### Scenario: 原生呈现可重试与已排队错误
- **WHEN** Vulkan present 返回 OOM、OutOfDate 或 SurfaceLost，或 DXGI Present 返回 WAS_STILL_DRAWING
- **THEN** 已知未入队的失败保留同帧重试；可能已排队的呈现保留同步和图像责任直到安全退休，错误不被当作完整成功或无限 Pending

#### Scenario: DX12 呈现资源交还
- **WHEN** graphics completion 已确认并成功进行 DO_NOT_WAIT Present
- **THEN** 记录独立的 Present 后 queue fence 和对应 native buffer 所有权条件；它证明应用资源可退休，不宣称屏幕扫描完成。信号失败保留不确定责任，resize/close 不能提前释放仍在途 buffer

#### Scenario: 输出借用跨 lease 或绕过同步
- **WHEN** borrowed output 被编译到另一 DeviceFrameLease，或提交未携带其输出同步所有者
- **THEN** 显式拒绝且不排队；borrowed image 不转换为会删除 swapchain wrapper 的拥有型引用。Accepted/Indeterminate 保留同步 owner 至 GPU 安全收尾，Rejected 保留调用方录制

#### Scenario: 正常关闭与终态退出
- **WHEN** 输出请求 resize/close 或发生无法恢复的提交/退休错误
- **THEN** 正常路径在该输出 CPU/GPU 借用清空后按所选呈现路径处置剩余责任，不等待整个设备；未完成的 presentation 资源不得因帧退休提前释放。终态隔离不伪报正常确认，销毁保持所选路径的 native 收尾及 Device/module/window 寿命
