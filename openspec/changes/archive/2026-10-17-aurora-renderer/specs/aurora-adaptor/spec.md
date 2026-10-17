范围：下列新 Renderer 宿主条款适用于已迁移的 Windows 路径；未迁移平台保持完整旧模式并通过平台能力门拒绝未实现的新协议。

## ADDED Requirements

### Requirement: 渲染端 UI feature

UI adaptor SHALL 将 owned vertices、indices、draw order、clip、圆角参数、精确图像版本和 logical ViewOutput 引用编码为有界快照。渲染端 SHALL 通过消费方 FrameGraphBuildContext 创建图声明与 lease-owned 上传，不读取可变编辑器 UI 对象。shader 编译归生产者，native 对象创建与退休归 coordinator。

#### Scenario: 透明 UI draw 并行编码
- **WHEN** UI draw 形成多于 encode worker 的任务片段且有多个帧在途
- **THEN** 片段按快照顺序提交，clip、alpha blend 与 LOAD 像素结果保持一致

#### Scenario: 一个 UI 窗口不可用
- **WHEN** 两个 UI 输出采样同一个离屏结果，其中一个 native 输出不能 acquire
- **THEN** 只跳过不可用窗口，共享 producer 和另一输出的采样绘制继续

### Requirement: RenderSceneProxy 命令入口

Aurora adaptor SHALL 提供主线程 RenderSceneProxy，将宿主 scene 生命周期映射为 Renderer IDs 与有序命令。Proxy SHALL 只消费 IRenderer 接口，不访问渲染线程 ECS 或 provider 具体类型。完整组件/资产事件提取 SHALL 由 aurora-scene-bridge 实施并复用该命令入口。

#### Scenario: Scene 创建与关闭
- **WHEN** 宿主创建或关闭 World
- **THEN** Proxy 提交对应 scene 命令并处理完成状态，不跨线程调用 RenderScene 的 Add/Get/DestroyEntity

#### Scenario: Scene 等待退休
- **WHEN** Proxy 已向宿主 packet 追加 Scene 销毁命令
- **THEN** 不再暴露该 Scene 为可写目标，收到匹配 generation 的 SceneDestroyed 或整个 Renderer 已停止后才释放生命周期状态；不匹配的旧回执不能释放新 Scene

### Requirement: 编辑器共享 Renderer 与 UI 快照

编辑器 SHALL 使用同一 IRenderer 接口提交主窗、工具窗和离屏 View 请求。UI 布局、输入、平台事件 SHALL 留在主线程；UI 绘制数据 SHALL 以拥有数据的快照交给渲染端扩展，资源上传/绑定按 FrameSlot 保活。合成 SHALL 显式声明对离屏结果的读取；Renderer 实现 SHALL 不依赖 EditorShell 或在渲染线程调用捕获 shell 状态的 GuiPaintFn。

#### Scenario: 主窗合成双场景视图
- **WHEN** 主线程 UI 快照引用两个离屏 View 的图像
- **THEN** FrameComposer 在同一 Frame RDG 声明其采样依赖，渲染端使用快照完成 UI 绘制而不读取主线程可变 UI 状态

### Requirement: Provider 注册与卸载顺序

宿主 SHALL 经 engine 接口的显式注入或服务注册取得 Renderer。provider SHALL 先于消费者就绪，消费者 SHALL 先停止提交再关闭 provider；模块卸载 SHALL 等待渲染线程退出与资源退休。跨 DLL 共享注册 SHALL 使用 sky::Singleton，共享幂等守卫 SHALL 使用 sky::Once；静态模块构建 SHALL 提供相同服务解析行为。

#### Scenario: 多模块消费同一服务
- **WHEN** runtime 与 editor 扩展在一个宿主进程解析 Renderer
- **THEN** 得到同一活动实例，重复注册不创建第二线程/设备，卸载后不留下指向已卸载代码的回调

#### Scenario: 模块初始化失败
- **WHEN** 动态或静态渲染宿主模块初始化失败
- **THEN** 清理部分初始化状态并在模块代码仍可执行时销毁对象，不调用该失败模块的 Start/Tick，也不留下已发布的 Renderer

#### Scenario: 既有模块的 provider-only 启动
- **WHEN** Aurora.Render 以 provider-only 模式加载，消费者创建真实 Renderer 后请求关闭
- **THEN** 模块只注册 provider，不启动旧 device/loop；消费者收到 RendererStopped 并销毁对象后才能卸载，动态与静态入口均可再次加载并创建新 epoch 的实例

### Requirement: 编辑器 CPU 纹理与快照版本

UI texture registry SHALL 仅持有有界 CPU 数据与逻辑版本。主线程 SHALL 先完成本帧所有窗口的字形/图集修改，再生成拥有数据的快照；每个 image draw SHALL 使用确切 product/revision/usage/device epoch。无效或未就绪版本 SHALL 跳过该 draw，不以其他版本替换。

#### Scenario: 多窗口共享更新后的字体图集
- **WHEN** 后绘制窗口新增字形导致 atlas revision 更新
- **THEN** 本帧全部 UI 快照引用该最终 revision，其上传沿用 Renderer 的资源和帧预算

#### Scenario: 图集更新或释放
- **WHEN** CPU registry 更新或释放已发布纹理
- **THEN** producer 发出旧版本释放请求；已接纳帧和未决上传继续持有需要的数据，最后借用退出后回收

#### Scenario: 工具窗口关闭
- **WHEN** 主线程收到工具窗口关闭请求
- **THEN** 只停止该输出的新快照并提交 DestroyViewport；收到匹配回执后才解除窗口事件监听和销毁 HWND，其它输出继续提交

### Requirement: Feature 模块代码保活

动态模块 loader SHALL 为 feature 提供真实代码 lease，并使依赖代码至少保活到消费者代码释放。模块逻辑 Shutdown 与代码卸载 SHALL 分离；最后的 feature、CPU、GPU、presentation 借用退出前不得卸载定义其回调的模块。

#### Scenario: Manager 清理后仍有代码 lease
- **WHEN** ModuleManager 已 Shutdown/销毁模块对象，而外部仍持有有效 lease
- **THEN** 动态库保持加载；释放最后 lease 后才执行模块 detach 与 native unload

#### Scenario: Provider 初始化失败
- **WHEN** provider 初始化失败或声明的依赖未能加载
- **THEN** 不初始化依赖它的消费者，宿主返回明确启动失败并清理已初始化模块
