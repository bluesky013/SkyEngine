## MODIFIED Requirements

### Requirement: RenderViewport 三阶段生命周期契约

RenderViewport SHALL 由 FrameScheduler 驱动 Begin（状态/重建）、Acquire（取得输出）、Release（安全呈现）三阶段，暴露成功获取的 image、extent、format、名称和提交所需同步信息；全局 frame slot/fence SHALL 由 scheduler 管理。失败 SHALL 只取消该输出。成功 acquire 后 SHALL 显式完成正常提交或错误收尾，Release 不得等待尚未安排 signal 的同步对象。

#### Scenario: Begin 失败取消单输出
- **WHEN** 一个 viewport Begin 失败
- **THEN** 该输出不建图、不 present，其它有效窗口与离屏输出继续处理

#### Scenario: Acquire 后 backbuffer 有效
- **WHEN** Begin 和 Acquire 成功
- **THEN** 输出 lease 含非空 image 及实际 extent/format，可供 scheduler 导入图

### Requirement: ClientViewport 持有 SwapChain 与 per-image sema

ClientViewport SHALL 封装 SwapChain；acquire 同步对象 SHALL 在消费它的 GPU 提交完成后复用，Vulkan render-done/present-wait semaphore SHALL 按 acquired image 管理或使用可证明 presentation 完成的替代协议。viewport SHALL 记录 image index 与 output generation，不得将本地 acquire 次数等同于全局 slot 或 image index。等待策略 SHALL 由后端能力和宿主策略明确规定，不将无限等待写死为公共契约。

#### Scenario: Acquire 返回有效索引
- **WHEN** ClientViewport 获取到 image index
- **THEN** lease 返回该 image 和正确同步对象，present-wait semaphore 不因另一个 frame slot 的 graphics fence 完成而提前复用

#### Scenario: 取消帧的 Release 为 no-op
- **WHEN** Begin 或 Acquire 失败且没有持有成功获取的 image
- **THEN** Release 不调用 Present，不等待未 signal 的对象

### Requirement: SwapChainStatus 自查与重建

ClientViewport SHALL 根据 OK/SUBOPTIMAL、OUT_OF_DATE、LOST 处理继续、重建或输出暂停；零 extent SHALL 跳过 acquire。surface 尺寸 SHALL 由宿主/平台决定。resize/close SHALL 使旧输出 generation 失效，并在旧 GPU 与 presentation 使用退休后回收资源；graphics fence SHALL 不被当作通用 presentation 完成证明。

#### Scenario: OUT_OF_DATE 触发重建
- **WHEN** 窗口尺寸变化使 swapchain OUT_OF_DATE
- **THEN** 安全退休旧资源并重建，后续 lease 使用新 generation 和实际 extent

#### Scenario: 重建失败取消 present
- **WHEN** surface LOST 或重建失败
- **THEN** 暂停该输出并报告状态，其它输出与生命周期命令继续处理

### Requirement: 全局 inflight frame 共享

FrameScheduler SHALL 复用 DeviceFrameContext 持有的唯一 GPU slot ring，通过显式 DeviceFrameLease 驱动构建和提交，替换旧隐式 Wait/Reset/自增协议；scheduler SHALL 不另持 native fence/command ring 或可独立变化的 slot 状态副本。同一 Frame RDG 的所有 viewport SHALL 使用同一 slot，但每个 View 的 dynamic buffer、UBO 和 descriptor 分配 SHALL 不重叠。slot SHALL 在上一次提交完成后复用；fence SHALL 只在确定提交时 Reset；同一 fence 不得无完成协议地用于多个独立提交。

#### Scenario: 多 viewport 同帧共享 dynamic buffer 稳定
- **WHEN** 主窗与离屏 View 在同一 slot 建图并提交
- **THEN** 它们使用不同的常量/descriptor 区域，且这些区域在提交完成前均保活

#### Scenario: 两个 View 的常量预算
- **WHEN** 同一 lease 内连续请求多个 View 的 UBO slice，最后一个请求超出配置预算
- **THEN** 已返回范围的 offset/内容保持稳定，容量不足显式失败，不重置本 slot 或借用其它在途 slot；完整 set 0 resolve 仍须在并行 encode 前完成

#### Scenario: 没有 GPU 工作
- **WHEN** 所有输出暂停且没有上传等 GPU 工作
- **THEN** 不 Reset 提交 fence；下一次获取 slot 不会等待本不存在的提交

#### Scenario: 部分窗口跳帧与上传帧
- **WHEN** 某 viewport 暂停，且下一次仅执行 upload-only 帧
- **THEN** 所有本次 GPU 分配仍使用所取得的同一个 lease，不按 viewport frame count 或独立 allocator 自增来选择分区

### Requirement: SceneView（aurora）

SceneView SHALL 提供 frustum、view、projection、viewProjection 和 camera position。FrustumCulling SHALL 使用该 View 的相机；对应常量 SHALL 写入该 View/FrameSlot 的独立区域。

#### Scenario: frustum 剔除
- **WHEN** entity worldBounds 完全位于该 View 的 frustum 外
- **THEN** 该 View 不收集该实体，不影响另一 View 的收集结果

### Requirement: RenderScene（aurora）

RenderScene SHALL 使用 EntityRegistry 与 SoA 组件池，提供 CreateEntity/DestroyEntity/Add/Get/Pool/View。Renderer 的 ViewStore SHALL 管理 View 生命周期，View 不进入场景 ECS。Bounds、WorldInfo、Light、MainLight 和 Skin 组件 SHALL 保留现有语义，并增加绘制所需 MeshBinding、MaterialBinding 和 RenderFlags。WorldInfo SHALL 存完整 world 矩阵；Light SHALL 保存 type/color/intensity/range/cone，位置和方向从 WorldInfo 派生；MainLight SHALL 标记方向光而不由 RenderScene 缓存。DestroyEntity SHALL 移除所有组件池成员。

#### Scenario: ECS 组件挂载与销毁
- **WHEN** entity 挂载 Light、WorldInfo 和 Bounds 后被销毁
- **THEN** 所有对应池移除该 entity，后续 View 遍历不产生残留实体

#### Scenario: WorldInfo 与光参数
- **WHEN** 更新完整 world 矩阵及 point/spot 的 range/cone
- **THEN** 矩阵和光参数可正确读取，光位置/方向从同一 entity 的矩阵派生
