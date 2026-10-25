## Context

本 change 保留尚未排期的 Renderer 扩展。旧模板最初作为逐 View/HDR 迁移候选；2026-10-26 调用审计确认无生产消费者后，用户要求删除，已由 `remove-unused-legacy-rendergraph` 承接。对应模板 delta 已移除，后续扩展只能通过生产 FrameGraph feature 接入。

## Decisions

- 原 3.11 的固定 Bounds/Object/point-light 与 View 提取已实现；剩余是可注册 CPU prepare feature、更多光类型及着色，需独立模块 lease/声明依赖/停用与任务 drain 验收。
- 原 4.1 的最小逐 View 状态已完成；未来 HDR/其它 pass 扩展采用 FrameGraphBuildContext，禁止共享可变 scene/view/extent；不恢复旧模板。
- 首个材质/HDR 扩展先确认实际资产入口、颜色格式、shader/layout、tonemap 和像素参考；有界缓存、精确版本和回收继续复用。
- Compute/Custom、storage、跨队列、GPU Scene 和更多格式属于候选扩展；没有具体消费者与读回案例前保持 Unsupported，不暗中放宽能力门。

## Validation

每个激活的子范围必须补真实输出、版本保活、失败传播和卸载测试。此记录当前没有实现，不能由文档齐全推断完成。
