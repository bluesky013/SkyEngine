## Context

主 change 的 pipeline delta 混有未来 HDR ScenePass、TextureToScreen、PipelinePass 显式 ViewRenderContext 目标。当前新 Renderer 直接由冻结产品与 FrameComposer 建图，旧模板尚未整体迁移。因此目标条款移入这里；主 spec 只新增已验证的新 Renderer 最小管线，不宣称旧模板已改造。

## Decisions

- 原 3.11 的固定 Bounds/Object/point-light 与 View 提取已实现；剩余是可注册 CPU prepare feature、更多光类型及着色，需独立模块 lease/声明依赖/停用与任务 drain 验收。
- 原 4.1 的最小逐 View 状态已完成；剩余旧模板/HDR 采用显式上下文，禁止共享可变 scene/view/extent。具体模板保留或替换在实施前评估。
- 首个材质/HDR 扩展先确认实际资产入口、颜色格式、shader/layout、tonemap 和像素参考；有界缓存、精确版本和回收继续复用。
- Compute/Custom、storage、跨队列、GPU Scene 和更多格式属于候选扩展；没有具体消费者与读回案例前保持 Unsupported，不暗中放宽能力门。

## Validation

每个激活的子范围必须补真实输出、版本保活、失败传播和卸载测试。此记录当前没有实现，不能由文档齐全推断完成。
