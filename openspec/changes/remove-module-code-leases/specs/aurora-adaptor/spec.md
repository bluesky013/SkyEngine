## MODIFIED Requirements

### Requirement: Feature 模块代码保活

动态模块 loader SHALL 将成功加载的 DLL 保持到进程退出，不向 Feature 或帧逐层传递代码 lease。模块会话 Shutdown SHALL 撤销服务并收齐任务；Feature 实例及其数据 SHALL 由实际 owner 保持到最后的 CPU、GPU、presentation 借用退出。二进制更新和移除 SHALL 在进程重启后生效。

#### Scenario: Manager 清理后仍有 Feature 借用
- **WHEN** ModuleManager 已 Shutdown/销毁模块对象，而已接受帧仍持有 Feature 实例
- **THEN** Feature 及所需数据保活到帧退休，动态库在最后对象释放后仍保持进程常驻

#### Scenario: Provider 初始化失败
- **WHEN** provider 初始化失败或声明的依赖未能加载
- **THEN** 不初始化依赖它的消费者，宿主返回明确启动失败并清理已初始化模块

### Requirement: Provider 注册与卸载顺序

宿主 SHALL 经 engine 接口的显式注入或服务注册取得 Renderer。provider SHALL 先于消费者就绪，消费者 SHALL 先停止提交再关闭 provider；模块会话关闭 SHALL 等待渲染线程退出与资源退休，DLL 代码 SHALL 由装载层保持进程常驻。跨 DLL 共享状态 SHALL 使用 sky::Singleton；每份反射数据 SHALL 由唯一 metadata owner 在进程 definition 创建时注册一次，SHALL NOT 使用 Once、once_flag、static 标志或查表跳过掩盖重复入口；静态模块构建 SHALL 提供相同服务解析行为。

#### Scenario: 多模块消费同一服务
- **WHEN** runtime 与 editor 扩展在一个宿主进程解析 Renderer
- **THEN** 得到同一活动实例，重复注册不创建第二线程/设备，会话关闭后撤销服务注册，不留下访问已销毁服务或数据的回调

#### Scenario: 模块初始化失败
- **WHEN** 动态或静态渲染宿主模块初始化失败
- **THEN** 清理部分初始化状态并在模块代码仍可执行时销毁对象，不调用该失败模块的 Start/Tick，也不留下已发布的 Renderer

#### Scenario: 既有模块的 provider-only 启动
- **WHEN** Aurora.Render 以 provider-only 模式加载，消费者创建真实 Renderer 后请求关闭
- **THEN** 模块只注册 provider，不启动旧 device/loop；消费者收到 RendererStopped 并销毁对象后才能关闭 provider 会话，动态与静态入口均可再次初始化并创建新 epoch 的实例，DLL 保持常驻
