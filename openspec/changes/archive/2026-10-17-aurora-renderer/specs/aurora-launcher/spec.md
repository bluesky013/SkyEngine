范围：下列新 Renderer 宿主条款适用于已迁移的 Windows 路径；未迁移平台保持完整旧模式并通过平台能力门拒绝未实现的新协议。

## MODIFIED Requirements

### Requirement: 模块初始化 aurora 设备

AuroraModule::Init SHALL 解析 RHI API 并通过 IRenderer 初始化 provider；Device、FrameSlots 和 command 资源 SHALL 由渲染端持有。初始化 SHALL 返回明确成功/失败结果，失败时不进入 GPU 帧循环；Shutdown SHALL 安全清理已创建部分。

#### Scenario: 指定 RHI API
- **WHEN** 启动参数包含 -r vulkan 或 -r dx12
- **THEN** Renderer 使用对应 API 初始化设备，AuroraModule 不再自行创建另一套设备

#### Scenario: 初始化失败安全
- **WHEN** provider 或设备初始化失败
- **THEN** 错误反馈给宿主，Tick 不发起 GPU 操作，Shutdown 可安全调用

### Requirement: 模块用平台窗口创建 viewport

AuroraModule::Start SHALL 优先从 ISystemNotify 取得主窗口原生 handle，并通过 Renderer 提交窗口描述创建 Viewport。平台窗口创建、事件和销毁 SHALL 留在宿主线程；原生窗口 SHALL 保活至 Renderer 返回销毁确认。

#### Scenario: 创建 swapchain viewport
- **WHEN** 模块 Start 且宿主提供有效窗口 handle
- **THEN** Renderer 创建关联该窗口的 viewport；宿主不直接持有或操作 swapchain

#### Scenario: 原生关闭请求与应用析构
- **WHEN** Windows 宿主收到 WM_CLOSE 或开始退出应用
- **THEN** 渲染中的原生窗口延迟销毁，宿主在渲染退休期间继续处理平台消息；应用在窗口成员析构前完成模块 Shutdown，不能仅把退出放在基类析构函数

#### Scenario: 选择整套宿主模式
- **WHEN** 已迁移平台启动 launcher，或尚未迁移平台启动原宿主
- **THEN** 在初始化时选择唯一一套帧循环；旧宿主不得借用新 Renderer 的 Device 另建 command/frame 资源

### Requirement: Tick 驱动最小出帧闭环

AuroraModule::Tick SHALL 收集状态/命令并提交 FramePacket，由 Renderer 处理 acquire、RDG、submit 与 present。无场景内容的有效窗口 SHALL 支持清屏；有效场景 SHALL 走实际 pipeline。零尺寸 SHALL 不 acquire，单窗口失败 SHALL 不阻断其它输出或命令；成功 acquire 后的失败 SHALL 安全收尾，不直接 Release 一个会等待未 signal semaphore 的 image。

#### Scenario: 出帧并呈现
- **WHEN** viewport 可用且提交 packet
- **THEN** Renderer 完成真实绘制或空场景清屏并呈现，宿主 Tick 不直接编码 GPU 命令

#### Scenario: packet 背压与尺寸变化
- **WHEN** TrySubmit 返回 BUSY 且窗口继续 resize
- **THEN** 保留未接纳 packet 的身份和数据，接纳后再用后续 packet 提交最新尺寸，不修改排队帧快照

#### Scenario: acquire 失败取消单输出
- **WHEN** 一个 viewport Begin/Acquire 失败
- **THEN** 该输出不 present，其余输出继续工作且 lifecycle 命令仍生效

#### Scenario: 零尺寸不 acquire
- **WHEN** surface extent 为零
- **THEN** Renderer 跳过该输出 acquire，恢复尺寸后可正常继续

#### Scenario: 已获取图像的失败收尾
- **WHEN** acquire 成功后建图或提交失败
- **THEN** Renderer 走有效收尾提交或后端错误退休路径，不产生等待未 signal 对象的 Present
