## ADDED Requirements

### Requirement: PreparedRaster 消费 Pass 契约

新 Renderer 的具体 Pass SHALL 决定资源布局与参数来源，并使用共享类型填充数据。PreparedRaster SHALL 负责冻结版本解析、mesh/LOD/range 校验和声明式 draw 组装，不得定义所有 Pass 必须使用的 View/Material/Object ABI。生产入口 SHALL 验证实际声明与 reflection 的一致性，不得要求固定三个 UBO、固定 80/64 字节或固定 Material=set 1。

#### Scenario: 声明驱动的生产绘制
- **WHEN** 生产 renderer 接收使用共享 Pass 布局的数据和匹配 shader
- **THEN** 组装并执行对应 draw，绑定数据来自该 Pass 声明，不从示例 ABI 补齐或改写

#### Scenario: 不匹配布局
- **WHEN** 提供的数据范围、字段布局或绑定类型不符合该 Pass 的 shader reflection
- **THEN** 在执行前返回明确错误，不截断、不重新解释为固定结构、不绕过反射校验

#### Scenario: 保留资源生命周期
- **WHEN** Pass 使用同图首次上传资源或旧版本回退资源
- **THEN** 精确版本 pins、InGraph 依赖、frame lease 与 GPU 退休行为保持有效，共享布局接入不得绕过这些协议
