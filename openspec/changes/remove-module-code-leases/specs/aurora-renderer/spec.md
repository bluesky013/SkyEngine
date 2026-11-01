## MODIFIED Requirements

### Requirement: FrameGraph 构建产物与扩展边界

Renderer SHALL 在 coordinator 上用冻结的 PreparedFrame、有效输出租约和 UploadBatch 构建一个当帧 RDG。构建 SHALL 先固定资源版本解析和共享上传，再加入逐 View passes 与 FrameComposer，最后 Freeze 并交由后端 Compile/FrameContext Dispatch，收齐 RecordedFrame 后显式 Submit。Frame feature SHALL 通过消费方 engine 接口注册，不能从 callback 直接提交队列或访问宿主/可变 ECS。UI 快照 SHALL 使用 logical texture/ViewOutput ID 及 generation/revision，在本图解析并保留资源 lease。

#### Scenario: UI 快照引用已 resize 的 ViewOutput
- **WHEN** UI 快照产生后对应输出 generation 已变化
- **THEN** 旧引用按声明策略占位或跳过，不使用过期 Image 指针或其它图的 RDG handle

#### Scenario: feature 停用与卸载
- **WHEN** 仍有 jobs、录制回调、continuations 或 GPU leases 引用 feature
- **THEN** 停用阻止未来建图，模块卸载等待所有相关使用退出

#### Scenario: 停用后重新启用
- **WHEN** feature 停用时有尚未建图的排队帧，随后同一 generation 重新启用
- **THEN** 旧 activation 的回调仍不执行，新帧使用新 activation；已有录制和提交继续退休

#### Scenario: 多 View 写同一输出
- **WHEN** 多个 View 按稳定顺序写同一个 viewport，UI 通过其 ViewOutputRef 采样
- **THEN** 引用指向本帧该 viewport 的合成内容；各相机独立图像使用不同 viewport

#### Scenario: GPU 完成但 presentation 未退休
- **WHEN** feature 请求退休，GPU 已完成而相关窗口的 presentation 尚未结束
- **THEN** Feature 实例及帧资源继续保活，OnRetired 和 RenderFeatureRetired 等待最后 presentation 责任退出；DLL 代码保持进程常驻

#### Scenario: feature 声明窗口却不产生绘制
- **WHEN** acquired output 在最终图中没有存活访问/export，其他输出仍有工作
- **THEN** 只放弃该输出，其他 command buffers 正常提交；实际使用的 borrowed output 仍必须匹配 acquire 和提交同步

#### Scenario: 非法 UI 快照与有效命令同包
- **WHEN** owned UI snapshot 的版本、字节数、索引范围或 clip 无效
- **THEN** 在应用 Scene/Viewport 和 feature 命令前拒绝整包，后续有效 packet 仍正常执行


## ADDED Requirements

### Requirement: Host explicitly assembles pipeline owners

The host SHALL assemble selected built-in or user features through existing RegisterRenderFeature/IFrameGraphFeature commands and owned Feature instances under process-resident module code. Built-in Scene registration SHALL NOT be a prerequisite or fallback for custom pipelines. Plugins SHALL share the existing producer, Renderer/Device, graph composition and retirement protocols.

#### Scenario: User pipeline replaces built-in scene
- **WHEN** the host selects custom features without registering built-in Scene
- **THEN** those features produce output without constructing built-in Passes or enforcing their layouts

#### Scenario: Owner retires with outstanding work
- **WHEN** an owner retires with accepted compilation, frame or GPU/presentation borrowers
- **THEN** new work stops and existing material/Feature/source/resource/module snapshots remain valid until users complete or drain
