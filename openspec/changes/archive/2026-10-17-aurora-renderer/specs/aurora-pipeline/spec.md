## ADDED Requirements

### Requirement: Renderer 逐 View 最小绘制管线

新 Renderer 路径 SHALL 从冻结 Scene/View 数据提取 owned draw packets，按 View 独立执行相机常量、裁剪、LOD、tag 与稳定排序；coordinator SHALL 解析精确 mesh/material 版本并在单帧图中构建 raster 与合成。首版已支持的附件范围 SHALL 为 RGBA8 color 与可选 D32 depth；未支持的 HDR/格式 SHALL 返回明确错误。旧 ScenePass/PipelinePass 模板不构成新 Renderer 的执行入口。

#### Scenario: 同 Scene 两个相机
- **WHEN** 两个 View 读取相同 Scene 并使用不同 projection、extent 或 tag
- **THEN** Scene prepare 只执行一次，逐 View draw 与常量相互独立，片段完成顺序不改变稳定结果

#### Scenario: 精确版本与逐 View 常量
- **WHEN** 本帧已冻结 mesh/material 版本，后续 packet 更新资源或相机
- **THEN** 当前帧保留原 geometry、layout、backing 和常量，不能解析成更新版本或改写在途 descriptor

### Requirement: Renderer 输出合成与统一 slot

FrameScheduler SHALL 管理窗口与离屏输出，复用 DeviceFrameContext 的唯一 slot ring。BeginViewport SHALL 查询 surface 配置；CPU prepare 和 slot 就绪后才为输出 pass acquire，并复核配置。FrameComposer SHALL 用带 generation/revision 的 ViewOutput 在同一图组合场景、UI 和输出；成功 acquire 的同步与 presentation 责任 SHALL 独立保活至合法退休。

#### Scenario: 不可用窗口保留其它工作
- **WHEN** 一个或所有窗口无法 acquire，但存在共享上传或离屏绘制
- **THEN** 只省略相应输出 pass，其余命令继续由同一 context 提交，不使用 fake backbuffer

#### Scenario: 双窗口与离屏的有界分配
- **WHEN** 多个 View 在同一 lease 请求常量、staging 或 descriptor
- **THEN** 分配范围互不覆盖，超额显式失败，不能通过重置 slot 或借用其它在途分区掩盖容量不足

## REMOVED Requirements

### Requirement: RenderPrimitive（aurora）
**Reason**: 持久 RenderPrimitive 与 ECS binding/当帧提取重复管理同一实体的绘制状态。
**Migration**: geometry、bounds、technique binding 通过 ECS 组件和资源句柄表达；tag 过滤与 DrawItem 生成移至逐 View 提取。
