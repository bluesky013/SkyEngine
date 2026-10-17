## MODIFIED Requirements

### Requirement: ScenePass (HDR)

ScenePass SHALL 从显式 ViewRenderContext 获取 scene/view、frame slot、extent 和 bindings，为该 View 声明 HDR color（RGBA16_SFLOAT，含 RENDER_TARGET、SAMPLED 及所需 transfer usage）和 D32 depth。attachments SHALL 以 CLEAR load，并建立 FRONT_TO_BACK 的 opaque queue。输出 handles SHALL 通过当帧结果传递，不得保存后用于另一 graph；不同 View 不得通过修改同一个 pass 的共享 extent 切换状态。

#### Scenario: ScenePass 构建 HDR RDG 节点
- **WHEN** 用实际 extent 为 1280x720 的 ViewRenderContext 建图
- **THEN** 生成对应尺寸的 HDR/depth attachments 与 opaque queue，返回仅属于当前图的 handles

### Requirement: TextureToScreenPass

TextureToScreenPass SHALL 从显式当帧上下文取得 input/output handles，声明输入 HDR 的 SRV read 和输出 color attachment（DONT_CARE、STORE，全屏覆盖）。PSO SHALL 匹配输出格式，descriptor SHALL 在 backing 解析后就绪；不得跨 View 共用可变输入绑定。

#### Scenario: TextureToScreen 构建 fullscreen 节点
- **WHEN** 提供有效 input HDR 与 output handle 建图
- **THEN** 产生 FULLSCREEN pass，输出 attachment 为指定 output，且包含 HDR SRV read 依赖

#### Scenario: 输入 HDR texture 从 RTV 转 SRV
- **WHEN** ScenePass 的 HDR 结果被 TextureToScreenPass 读取
- **THEN** RDG 推导先写后读依赖和 RTV 到 SRV 的 barrier

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

### Requirement: pass 收集链路

逐 View CPU Collector SHALL 在 SceneReady 后以冻结的 Bounds dense 数据驱动，通过只读 sparse 查询获取有效 entity 的 WorldInfo、MeshBinding、MaterialBinding 与 flags，按当前 View 裁剪、LOD、technique tag 和排序产生私有 draw packets。其访问 SHALL 纳入 CPU prepare DAG；分块结果 SHALL 稳定归并。SceneRasterPassTemplate SHALL 在 coordinator 建图时将已准备的数据解析为 DrawItems 并加入 builder，worker 不得并发修改 builder 或 GPU cache。未就绪 binding SHALL 跳过并可诊断；空 tag SHALL 使用默认 binding。DrawItems SHALL 不作为持久 ECS 组件保存。

#### Scenario: dense 遍历收集
- **WHEN** Collect 遍历场景
- **THEN** 连续扫描 Bounds，通过 entity 索引查询其它组件，仅收集当前 View 可见且资源就绪的 items

#### Scenario: tag 与排序
- **WHEN** 收集 tag 为 opaque 的 queue
- **THEN** 仅收集匹配 binding，并按当前 View 的深度做 FRONT_TO_BACK 排序

#### Scenario: 并行 View 提取与串行建图
- **WHEN** 两个 View 同时读取同一 Scene 并各自分块提取
- **THEN** worker 只写本任务结果，稳定归并后由 coordinator 添加到各自 pass，不发生共享 builder/vector 写入

#### Scenario: 分块完成次序与产品生命周期
- **WHEN** 同一冻结 Scene/View 的 chunk 完成顺序改变，或合并后的产品被保留到场景删除之后
- **THEN** 可见实体/LOD 和稳定顺序一致，矩阵/bounds/light 按声明容差一致；owned 产品保持有效且不延长 ECS 冻结

#### Scenario: 精确 CPU packet 解析为实际 geometry 和常量
- **WHEN** coordinator 使用已准备的精确 mesh/material 版本解析 indexed draw
- **THEN** stream、index type/LOD 范围和 shader 对应该版本；每 View 的 projection/view/camera、每对象 world 使用当前 lease 的独立常量范围，找不到该版本时明确拒绝而非换成更新版本

### Requirement: PipelinePass 模板基类

PipelinePass SHALL 将设备级不可变 pipeline 定义/PSO 与逐 View 可变状态分离。BuildRDG SHALL 显式接收 ViewRenderContext；不得轮流修改共享 mScene/mView/extent 以生成多个 View。SceneRasterPassTemplate SHALL 基于上下文提供默认 Collect。持久 PSO SHALL 通过 device cache 复用；变化的 RG/UBO SHALL 按 View/FrameSlot 分配；RDG handles SHALL 不跨帧存储。

#### Scenario: 场景不变时持久资源复用
- **WHEN** 两个 View 使用相同 program、layout、几何及 attachment 配置
- **THEN** 可以命中同一 PSO，仍使用各自独立的 camera bindings

#### Scenario: pass 使用显式场景与视图
- **WHEN** 使用 View A 的上下文构图后再使用 View B 的上下文构图
- **THEN** 各自 items、attachments 和 bindings 保留对应上下文，不因后一次构图被覆盖

### Requirement: OpaquePass

OpaquePass SHALL 继承 SceneRasterPassTemplate，声明 color/depth attachments 和名为 opaque 的 FRONT_TO_BACK queue，并收集资源就绪的实体生成真实绘制。

#### Scenario: OpaquePass 构建 RDG
- **WHEN** 可见的程序化网格具有有效 geometry、program 和 material binding
- **THEN** 构建含该网格 DrawItem 的 SceneRasterPass，并在 Execute 中发出实际 draw
