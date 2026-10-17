## ADDED Requirements

### Requirement: 平台整套协议验收

新增迁移平台 SHALL 自行实现匹配的 compiler/encoder 和 FrameContext；只有受检提交、内容读回、资源预算与生命周期均通过实际平台验证后才声明支持。共享 Windows 参考算法或返回 Unsupported 不构成能力完成。

#### Scenario: Metal 内容与并行编码
- **WHEN** 同一合法图在 Metal 串行与受支持的并行配置执行
- **THEN** 实际 readback 内容一致，乱序任务不改变透明顺序，多个在途 slot 不互相 reset，所有 parent/child 和资源保活至完成

### Requirement: 原生宿主能力门

平台宿主迁移 SHALL 记录 surface/drawable 操作线程、可退出等待与 presentation 退休证明，并在真实原生窗口验证。移动设备 transform 验证 SHALL 包含物理旋转，不能仅用纯坐标函数测试替代。

#### Scenario: 关闭与不可用输出
- **WHEN** 窗口最小化、resize、关闭或 acquire 失败
- **THEN** 只隔离不可用输出，独立上传和离屏工作继续；原生窗口在借用、GPU 与 presentation 责任结束后才销毁
