## Context

Aurora 已有 core、adaptor、pipeline、runtime、RHI。新增 renderer/core 与 renderer/adaptor 把 CPU prepare 和 GPU 编排区别提升为另一套模块层级，后者实际没有 Framework 适配职责。用户确认复用既有 Aurora 结构，结构 diff 与 B10 功能 diff 独立 review。

## Goals / Non-Goals

实现归属唯一、公开 include 兼容、私有头不泄漏、沿用现有装载入口。保持 CPU prepare、coordinator、RHI 的执行职责边界。不重写算法，不改后端路径，不把生产 Renderer 尚未实现的状态改为成功。

## Decisions

| 原位置/target | 新位置/target |
|---|---|
| renderer/include/aurora/renderer | core/include/aurora/renderer；保留 Aurora.Renderer.Interface |
| renderer/core 的 scene/prepare/frame | core/src/renderer 对应目录；Aurora.Core.Static |
| renderer/adaptor 的 graph/resource/provider | core/src/renderer；Aurora.Core.Static |
| renderer/test | core/test/renderer；保留验收 target 名称 |
| renderer/runtime provider 注册 | adaptor 的 AuroraModule；Aurora.Adaptor.Static |
| Aurora.Renderer 模块入口 | 现有 runtime/Aurora.Render |

CreateRendererProvider 是 core 给 adaptor 使用的 foundation 工厂；宿主仍通过服务取得 IRendererProvider，不直接调用工厂。Renderer 实现头归 core 私有 include；仅对应测试额外取得该路径。CPU scene/prepare/frame 仅消费 Core/接口数据，不 include RHI、Framework 或 GPU graph/resource 实现。

AuroraModule 先检查服务 owner，再执行既有初始化；全部成功才发布 provider。重复 Init 不重建资源，另一 owner 的 Init 失败且 Shutdown 不影响原 owner。Destructor/Shutdown 幂等撤销自身注册；生产消费者 drain 仍是 B12/B14 的前置要求，不由本结构变更伪装完成。

原模块测试改为实际 Aurora.Render 入口，保留动态 DLL 及 tagged static 两种方式。测试宿主先拥有共享 Instance/反射注册，确保跨 DLL 持久对象及回调不随被测模块卸载；测试不 Start 窗口，不作为 presentation 验收。接口不链接实现的检查仍由 ContractTest 独立承担。

## Risks / Trade-offs

实现合并进已有 core 后不再以 target 隔离 CPU/GPU；需持续检查 prepare 的源码依赖。core 本来依赖 RHI，本次不增加 Framework 或业务 plugin 依赖。新增测试子目录必须从原 CoreTest 的文件收集排除，避免重复 main；所有 renderer 测试保持独立目标。

## Validation

批前快照与 68 项路径映射用于对照实际差异；公开头逐字节/格式归一核对，移动后算法逻辑不变。重新配置和构建相关 core/adaptor/runtime、Renderer 测试及 Sandbox；跑接口/CPU/资源/模块、原 core/adaptor 测试和完整 GPU/RHI 回归。核对旧实现 target/目录不再被源码构建引用，并校验 OpenSpec 与文档链接。
