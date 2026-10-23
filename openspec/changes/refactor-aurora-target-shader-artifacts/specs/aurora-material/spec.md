## MODIFIED Requirements

### Requirement: Material 属性系统（material 级，值 + 纹理）

Material SHALL 提供跨 technique 共享的 Name 属性，属性类型继续复用 RgFieldType 的词汇，不建立另一套材质字段类型枚举。CPU 值按其声明语义存储，字节大小由类型派生；MaterialPropertyMap SHALL 只描述 CPU 属性位置，不表示 GPU offset。SetValue/GetValue SHALL 校验 T 的语义类型/shape，SHALL NOT 仅凭 sizeof 通过校验；未知名或不匹配 SHALL 返回明确结果且不写入。

纹理 SHALL 按 Name 持有合法的 Texture 引用/逻辑资源身份，sampler SHALL 使用独立的类型化设置/引用，不能作为 Texture 伪装。参数到各 technique shader 的目的地 SHALL 以显式 Name alias 映射，最终进入ResourceGroup 共用的同一个 RHI 命名事务前端及布局计划，不暴露平台 slot/offset。CPU 属性可保留既有类型词汇，但执行路径不支持的字段/资源 SHALL 在受检绑定前明确 Unsupported；本次不自动扩大 shader 字段支持集合。

#### Scenario: 按类型声明并读写
- **WHEN** 声明 FLOAT4 的 baseColor 并按 Name 写入 Vector4
- **THEN** 可读回对应 CPU 值，GetPropertyMap 保留该 CPU 属性的类型与位置

#### Scenario: 相同大小不能替代类型
- **WHEN** 对 FLOAT 属性按 Name 写入 uint32_t
- **THEN** 明确类型错误且原值不变，即使二者同为四字节

#### Scenario: Size 由 type 派生
- **WHEN** 读取 material CPU 属性元数据
- **THEN** 值大小由声明语义类型派生，不重复存储另一份可漂移 size，也不当作 GPU block offset

#### Scenario: 类型词汇共用
- **WHEN** material 描述已有 scalar/vector/matrix 或资源属性
- **THEN** 复用共享字段词汇，执行能力由实际支持表校验

#### Scenario: 纹理与 sampler 挂接
- **WHEN** 按 Name 分别设置合法纹理与 sampler
- **THEN** 各自进入匹配逻辑资源，类型不兼容时明确失败，sampler 不以 Texture 容器表示

### Requirement: MaterialInstance 部分字段 runtime 稀疏覆盖

MaterialInstance SHALL 持有共享 Material 与实例覆盖。值覆盖 SHALL 延续单个连续 CPU 存储、首次写入惰性复制的表示，不逐字段分配容器；纹理/资源覆盖 SHALL 保持稀疏表示。Name 查找与语义类型校验 SHALL 与 Material 相同，失败返回明确结果且保留旧值。未覆盖项 SHALL 继承 Material 默认值，实例更新 SHALL 不污染共享 Material。

SetMaterial SHALL 清空该实例旧覆盖；IsOverridden SHALL 分别表达值差异与资源覆盖存在性。未设置 Material 的写入 SHALL 返回明确错误而非静默成功，读取返回缺失。实例最终参数 SHALL 按 technique/interface 的同一 Name alias 进入与 ResourceGroup 共用的 RHI 更新事务前端，并冻结对应值/资源版本供帧使用。

#### Scenario: 部分覆盖与继承
- **WHEN** 实例只按 Name 覆盖 baseColor
- **THEN** baseColor 使用实例值，其它属性继承默认值，覆盖状态可查询

#### Scenario: 覆盖不污染共享 Material
- **WHEN** 实例更新值、纹理或 sampler
- **THEN** 共享 Material 及其它实例保持原值

#### Scenario: 值覆盖保持连续存储
- **WHEN** 一个实例只覆盖一个 scalar 字段
- **THEN** 值仍存于实例的单块连续存储，不逐字段创建独立容器

#### Scenario: 未设置 Material
- **WHEN** 未关联 Material 的实例执行写入或读取
- **THEN** 写入返回明确错误，读取返回缺失，不崩溃也不伪装成功

#### Scenario: 更新与在途帧隔离
- **WHEN** 旧帧未退休时实例替换贴图或改变数值
- **THEN** 新帧使用新快照，旧帧继续读取旧值与精确图片版本

## ADDED Requirements

### Requirement: Material named bindings use the shared parameter transaction

材质资产/实例 SHALL 以 named typed 值和资源引用表达输入，按 technique/interface 验证 property Name 到 shader parameter Name 的显式 alias。SceneAssetSource 与渲染传输 SHALL NOT 独立预打包目标 uniform 字节或以硬编码数字 binding 构成另一条材质参数入口。材质与 Feature SHALL 保持其 provider/assembler 归属，统一使用相同参数表、RHI 参数事务前端、UniformUploadPlan 与 BindingPlan；格式改变 SHALL 有明确 schema 迁移及 recook。

#### Scenario: 同材质不同 technique
- **WHEN** 一个材质的属性在两个 technique 中映射到不同的合法 shader 参数名
- **THEN** 各映射显式校验并缓存，调用方按同一 material property Name 更新，目标数据布局由所选 technique 决定

#### Scenario: 别名缺失或越权
- **WHEN** 属性映射到不存在、类型错误或由其它 provider 拥有的目标参数
- **THEN** 受检装配失败，不搜索同名字段补齐或覆盖 Feature 参数

### Requirement: Material texture bindings retain exact resource dependencies

普通 2D 材质纹理 SHALL 经现有资产/cook 与资源发布/上传路径解析，adaptor 处理 UUID/加载，渲染快照持完整资源版本、视图和 sampler 信息。协调器 SHALL 验证 Resident 或既有 InGraph 完成资格、device epoch 与图读访问，解析后按同一 Name 事务绑定。引用及旧快照 SHALL 保活至对应 GPU 使用退休。RHI SHALL NOT 认识 AssetManager/UUID，编码阶段 SHALL NOT 查询最新资产或隐式改用默认纹理。

普通 2D sampled 颜色贴图 SHALL 正确保留 sRGB 语义，数据贴图 SHALL 按线性语义消费；mip/view、native format 与 sampler SHALL 受检。环境辐射专用转换 SHALL NOT 被当作全部材质纹理规则。其它未在本期支持的资产形态 SHALL 明确 Unsupported，不能省略或伪装成功。cook/package SHALL 收集被引用图片及所需数据，shipping SHALL 无需作者源码或 Slang 完成 Name 驱动的常量与采样。

#### Scenario: 材质常量与纹理一起生效
- **WHEN** 从真实材质资产解析 Name 属性、2D 纹理和 sampler 并完成命名事务
- **THEN** 三后端原生读回同时反映数值与采样内容，shader 不依赖各平台 slot/offset

#### Scenario: 纹理未就绪或失败
- **WHEN** 图片依赖尚未完成、加载失败、缺失或 device epoch 不匹配
- **THEN** 使用既有显式资源状态/错误处理，不以 null descriptor 或未声明默认纹理完成成功绘制

#### Scenario: 颜色与数据语义
- **WHEN** 材质分别绑定声明为 sRGB 颜色和线性数据的普通 2D 图片
- **THEN** 目标格式/采样语义与声明一致，不能将二者都按环境线性辐射限制处理

#### Scenario: Shipping 材质依赖
- **WHEN** 重新 cook 的产品被搬迁且作者源码/Slang 不可用
- **THEN** 命名表及材质图片依赖仍完整，合法 Name 更新和真实采样读回通过
