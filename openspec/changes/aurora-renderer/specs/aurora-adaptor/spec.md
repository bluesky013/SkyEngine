## ADDED Requirements

### Requirement: RenderSceneProxy 命令入口

Aurora adaptor SHALL 提供主线程 RenderSceneProxy，将宿主 scene 生命周期映射为 Renderer IDs 与有序命令。Proxy SHALL 只消费 IRenderer 接口，不访问渲染线程 ECS 或 provider 具体类型。完整组件/资产事件提取 SHALL 由 aurora-scene-bridge 实施并复用该命令入口。

#### Scenario: Scene 创建与关闭
- **WHEN** 宿主创建或关闭 World
- **THEN** Proxy 提交对应 scene 命令并处理完成状态，不跨线程调用 RenderScene 的 Add/Get/DestroyEntity

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
