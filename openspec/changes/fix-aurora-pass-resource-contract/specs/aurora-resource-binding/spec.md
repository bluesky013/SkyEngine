## ADDED Requirements

### Requirement: Pass 绑定按实际 shader 子集装配

绑定装配 SHALL 按实际 shader reflection 校验所提供共享 schema 的资源与 uniform 布局。未被 shader 使用的声明可省略；实际使用但缺少声明、缺少绑定值、重复提供或类型/形状冲突 SHALL 在 GPU 执行前失败。装配失败 SHALL 不改变原 draw；成功 SHALL 输出现有图绑定并沿用子资源分析与退休协议。

#### Scenario: 可选资源
- **WHEN** shader variant 不使用 Pass 声明中的纹理
- **THEN** 装配不要求该纹理的运行时绑定

#### Scenario: 上游 Pass 输出作为输入
- **WHEN** 下游 Pass 将上游写入的图 image 绑定到共享声明的采样槽位
- **THEN** 读写依赖作用于同一图资源及实际 view 范围，提交后的资源保持有效至退休

### Requirement: Renderer 保持四层资源语义

Renderer SHALL 使用 Global set 0、Pass set 1、Material set 2、Batch set 3 的资源分层。具体 Pass SHALL 拥有 Pass 层声明，View SHALL 为按需输入；系统 SHALL NOT 将四层重新规定为固定 View/Material/Object 块，或强制所有 shader 声明相机常量。材质数据 SHALL 归 Material，逐 draw 数据 SHALL 归 Batch；动态偏移 SHALL 服从实际声明和 shader reflection，不由 set 编号猜测字段、大小或类型。

#### Scenario: 无 View 的 Pass
- **WHEN** 一个 Pass 的 shader 与资源声明均不包含 View 数据
- **THEN** renderer 接纳匹配声明，不要求补充相机 UBO 或固定 80 字节块

#### Scenario: 不同 Pass 参数布局
- **WHEN** 两个 Pass 在各自 set 1 使用不同参数类型与大小
- **THEN** 两者分别按各自共享布局验证和绑定，不能用 Material 固定布局覆盖任一 Pass

### Requirement: 布局共享与帧数据隔离

系统 SHALL 允许共享不可变 shader/layout/PSO；可变 Global/Pass/Material/Batch 数据 SHALL 保持当前 frame lease 和使用范围内的隔离，数据与引用 SHALL 保活至对应 GPU 使用退休。动态绑定 SHALL 按声明/反射校验范围、对齐和顺序，不得从资源层名称推导 dynamic 类型。

#### Scenario: 多 View 与在途帧
- **WHEN** 同一 Pass 定义用于两个 View，且上一帧尚在 GPU 执行
- **THEN** 每次使用读取其对应的常量，不会被后续 View 或下一帧覆盖
