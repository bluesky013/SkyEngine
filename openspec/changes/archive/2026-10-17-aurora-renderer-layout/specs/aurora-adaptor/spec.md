## ADDED Requirements

### Requirement: Renderer foundation 实现与模块归属

Renderer 接口与实现 SHALL 属于现有 Aurora core，Framework 服务注册与 World/资产对接 SHALL 属于现有 Aurora adaptor，运行时入口 SHALL 复用 Aurora.Render。不得为 Renderer 再建立 core/adaptor/runtime 实现模块体系或 Aurora plugin。仅接口 target SHALL 不链接实现；CPU prepare SHALL 不访问 GPU 或 Framework 可变对象，内部实现头 SHALL 不作为公开 include 传播。

#### Scenario: 宿主只消费接口
- **WHEN** 宿主包含 aurora/renderer 公共接口
- **THEN** 原 include 路径有效，接口 target 不要求链接 Aurora.Core.Static 或 Aurora.Adaptor.Static

#### Scenario: 动态与静态模块装载
- **WHEN** 宿主通过动态或 tagged static 方式初始化 Aurora.Render
- **THEN** 初始化成功后发布同一 Environment 中的 provider，重复 owner 不被覆盖，Shutdown 只撤销自己的注册

#### Scenario: CPU prepare 与内部实现边界
- **WHEN** Renderer 实现编译进 Aurora.Core.Static
- **THEN** scene/prepare/frame 代码仍只消费 CPU 接口数据，graph/resource 内部头不经 target 的公开 include 传播给宿主


## MODIFIED Requirements

### Requirement: adaptor 静态库 + runtime/editor 动态模块

Aurora Framework 适配 SHALL 位于 engine/aurora/adaptor，目标为 Aurora.Adaptor.Static；Renderer foundation 实现 SHALL 位于 Aurora.Core.Static，核心不得依赖 Framework 或业务 plugin。运行时入口 SHALL 位于 engine/aurora/runtime 的 Aurora.Render；编辑扩展 SHALL 位于 engine/aurora/editor 的 Aurora.Render.Editor，注册编辑器功能而不建立第二个 Renderer。模块 SHALL 复用既有静态实现，支持配置指定的动态或 tagged static 入口。

#### Scenario: 既有职责与装载入口
- **WHEN** 构建并装载 Aurora.Render 与 Aurora.Render.Editor
- **THEN** 两者使用 Aurora.Adaptor.Static，Renderer 实现只归 Aurora.Core.Static；编辑扩展不重复创建 Device

#### Scenario: core 与 Framework 边界
- **WHEN** 构建 Aurora.Core.Static 或仅接口消费者
- **THEN** core 不链接 Framework，接口消费者不需要链接 foundation 的实现 target
