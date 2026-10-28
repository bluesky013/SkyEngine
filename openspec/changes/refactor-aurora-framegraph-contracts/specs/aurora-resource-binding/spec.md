This delta defines the final public contracts of `refactor-aurora-framegraph-contracts`. For the requirements named below it supersedes earlier API wording, including overlapping active change deltas; unmodified shader semantics, target ABI, supported capabilities and retirement requirements remain in force.

## MODIFIED Requirements

### Requirement: Device::CreateResourceGroup 创建 group 实例

`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 按 `Descriptor::{shader, set}` 分配一个新的 ResourceGroup 实例。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。返回的 ResourceGroup 在创建后所有 binding 均为"未写入"状态，原生调用方必须先成功完成 numeric DescriptorEncoder 写入才能被执行消费；命名调用方必须经 ShaderParameterUpdate 发布新的不可变 ResourceGroup，再消费其 GetResult。ResourceGroup 的 binding 布局 SHALL 等于 shader reflection 中该 set 的完整资源集。

#### Scenario: 创建 group

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr

### Requirement: ResourceGroup::Update 批量写入资源

ResourceGroup SHALL 经由 CreateParameterUpdate(updateContext) 创建的独立 ShaderParameterUpdate 事务写入数据与资源，名称由 interface/set 参数表解析，并按计划校验类型、array element、资源类别、访问方式、device owner 与范围。错误 SHALL 通过受检结果显式报告，不能用断言、未定义行为或静默忽略表达可恢复错误。批量写入 SHALL 在全部验证与编码成功后发布不可变版本；失败 SHALL 保持先前有效绑定。多次成功事务 SHALL 允许部分更新，未覆盖的值继承先前版本，不修改仍被 GPU 使用的 descriptor/uniform。

ResourceGroup SHALL NOT 恢复 Update(vector) 或 CreateEncoder(updateContext) 命名重载；CreateEncoder() SHALL 只返回 numeric native writer。命名事务和图装配 SHALL 直接复用 ShaderParameterTransaction，不以两种互斥模式实现同一个 encoder 接口。

#### Scenario: 写入 uniform 与 sampled image
- **WHEN** 对合法 uniform/image/sampler logical slots 批量提供同 device 的匹配资源
- **THEN** 发布后的绑定按计划在 native shader 中读取正确值

#### Scenario: 类型或 owner 不匹配
- **WHEN** 写入资源类型、访问方式或 GetDevice 与目标契约不匹配
- **THEN** 返回明确错误，原有有效绑定不变，不进入错误原生对象转换

### Requirement: DescriptorEncoder 接口

DescriptorEncoder SHALL 仅提供以 numeric binding/arrayElement 寻址的 WriteBuffer、WriteImage、WriteSampler 与 End，用作受检原生 descriptor 写入协议。它 SHALL NOT 提供 Name-first WriteData、WriteBlock、GetResult、命名事务 Cancel 或相应失败占位方法；这些能力 SHALL 由独立 ShaderParameterUpdate 接口提供。

原生调用方 SHALL 保证目标 group 可写且旧 GPU 使用已退休；published parameter snapshot SHALL 拒绝创建 numeric writer。所有资源和范围 SHALL 按 Shader 派生 BindingPlan 验证，任一写入失败 SHALL 使 End 返回失败；低层 writer SHALL NOT 被宣传为跨 group 或原地更新的原子事务。接口 SHALL NOT 恢复 ResourceUpdateInfo 或依赖 Framework、MaterialPropertyValue、Aurora.Core 图、资产类型。

#### Scenario: Numeric 资源写入
- **WHEN** 原生调用方按 numeric binding 写入 buffer、image 和 sampler 后 End
- **THEN** writer 检查资源类型、device owner、范围和 array element，成功后才消费该 group；End 不提交 queue

#### Scenario: 命名接口不再混入 native writer
- **WHEN** 调用方需要写入 typed 字段或取得新 immutable snapshot
- **THEN** 使用 ShaderParameterUpdate；DescriptorEncoder 不暴露这些方法

### Requirement: ResourceGroup::CreateEncoder 返回后端 encoder

ResourceGroup::CreateEncoder() SHALL 只创建所属 device/backend 的 numeric DescriptorEncoder，不接受 updateContext 或解析参数名。布局 SHALL 来自该 group 的 Shader/interface/set，unknown owner 或 published parameter snapshot SHALL 在原生下转型或写入前拒绝。writer SHALL NOT 提交 GPU、等待 fence 或推进独立帧 ring。

ResourceGroup::CreateParameterUpdate(updateContext) SHALL 是唯一命名更新工厂；其显式上下文、语义事务、原子发布和部分继承要求由 ShaderParameterUpdate 契约规定。

#### Scenario: Numeric writer 的 set 作用域
- **WHEN** 向 group 的 numeric writer 写入不属于其派生 set 布局的 binding
- **THEN** 返回失败，End 同样失败，不跨 set 搜索

#### Scenario: Native 更新不提交 GPU
- **WHEN** numeric writer 成功 End
- **THEN** 只完成该 group 的原生写入，不创建 queue、等待 fence 或独立 Submit

#### Scenario: Published snapshot 不能原地修改
- **WHEN** 调用方对已发布的 immutable parameter snapshot 调用 CreateEncoder()
- **THEN** 返回空 writer；修改必须另建 ShaderParameterUpdate 并取得新版本

### Requirement: DX12 shader-visible descriptor ring

D3D12 后端 SHALL 用 **CPU-only staging heap**（`D3D12_DESCRIPTOR_HEAP_FLAG_NONE`）作为描述符的持久 source of truth，并用 `ringSize` 张 **shader-visible heap**（每 in-flight frame 一张，默认 3）做 GPU 可见视图，两者 offset 1:1。

`D3D12DescriptorAllocator::BeginFrame(frameIndex)` SHALL 把当前帧设为 `frameIndex % ringSize`，只由显式创建的 `DirectFrameContext::TryBeginFrame()` 在其 direct slot 退休并成功重置 fence 后选择。`D3D12ResourceGroup::EnsureFrameCopy()` SHALL 在 `mDirty` 或当前帧未复制过时，用 `CopyDescriptorsSimple` 把 staging 区间拷到当前帧的 shader-visible heap。`D3D12GraphicsEncoder::BindResourceGroup` / `D3D12ComputeEncoder::BindResourceGroup` SHALL 在绑表前调用 `EnsureFrameCopy`。

此 ring SHALL 仅服务 direct compatibility 路径。生产 FrameGraph SHALL 使用当前 DeviceFrameLease 分区的 descriptor heap，不从 getter 或旧隐式 frame index 推进 ring；两条公开协议 SHALL 分离。

#### Scenario: 每帧重写不覆盖 in-flight 帧

- **WHEN** 帧 N 写入描述符并提交，帧 N+1 的 direct context 成功 TryBeginFrame 后重写同一 RG
- **THEN** 帧 N+1 的 copy 落到不同的 shader-visible heap，不覆盖帧 N 仍在 GPU 读取的描述符

#### Scenario: 静态描述符跨帧复用

- **WHEN** RG 写一次后连续多帧 bind
- **THEN** 每帧第一次 bind 时 `EnsureFrameCopy` 从 staging 拷到当前帧 heap，GPU 读到正确描述符

### Requirement: 移除 ResourceUpdateInfo 与 Update(vector)

`ResourceGroup` SHALL 不再有 `Update(const std::vector<ResourceUpdateInfo>&)` 方法；`ResourceUpdateInfo` 与 `ResourceWriteKind` SHALL 从接口层移除。`DescriptorHeap::Update` SHALL 同步迁移到 encoder（bindless tier2，未启用时仅签名对齐）。

#### Scenario: Update 接口移除

- **WHEN** 调用 `group->Update({write})`
- **THEN** 编译错误（接口已移除，命名更新改用 `CreateParameterUpdate(context)` + typed/name `Write*` + `End/GetResult`，原生 numeric 写入使用无参数 `CreateEncoder()`）

## ADDED Requirements

### Requirement: ResourceGroup::CreateParameterUpdate 返回命名更新事务

ResourceGroup::CreateParameterUpdate(ShaderParameterUpdateContext &) SHALL 返回独立 ShaderParameterUpdate。该接口 SHALL 提供 Name/typed-handle WriteData、带 CPU schema 的 WriteBlock、Name-first WriteBuffer/WriteImage/WriteSampler、End、GetResult、GetError 与 Cancel；SHALL NOT 暴露 numeric native writer 的 binding 重载。WriteData SHALL 携带中性语义类型和 shape，SHALL NOT 仅凭裸字节数推断类型。

ResourceGroup 与 Core 图装配 SHALL 直接委托同一 RHI 中性 ShaderParameterTransaction，复用 BindingPlan/UniformUploadPlan，统一名称/接口/set 作用域、类型、重复与重叠写、inactive、必需参数和原子冻结。各后端 SHALL NOT 各自搜索名字；仅原生资源与图资源的 materializer 分离，图 materializer SHALL 保留访问声明和现有 lease，不提前分配 native graph backing。

updateContext SHALL 显式提供匹配 device、有效 lease/epoch、uniform 分配与 retention，不暴露 AssetManager、UUID 或 GraphImageHandle。成功 End SHALL 经 GetResult 发布新的 immutable ResourceGroup；部分更新 SHALL 继承旧版本的未覆盖值。写入、编码、分配或上下文失效 SHALL 使整个事务失败，不替换既有版本；即使调用方忽略单项错误，End 也 SHALL 失败。不 End、Cancel 或销毁事务 SHALL 取消未发布暂存；End SHALL NOT queue Submit、等待 fence 或自行推进 ring。

#### Scenario: 混合命名与类型验证
- **WHEN** 一个事务写入 typed 字段、纹理和 sampler；或者向 FLOAT 字段写入同大小 uint32_t
- **THEN** 合法完整输入发布同一新快照，类型错误使整个事务失败，不把相同字节数当类型匹配

#### Scenario: 作用域和重叠写
- **WHEN** 参数句柄属于其它接口/set，或同一字段经别名重复/与整个 block 重叠写入
- **THEN** 显式报告错误，不跨作用域搜索；End 不发布合法子集

#### Scenario: 更新与旧版本并存
- **WHEN** 已提交旧 snapshot 后执行合法部分更新并释放外部旧引用
- **THEN** 新 snapshot 继承未改值，旧 shader/resources/uniform retention 保持到原 GPU 使用安全退休

#### Scenario: 失效上下文或取消
- **WHEN** owner/lease/epoch 不匹配、上传失败、显式 Cancel 或未 End 即销毁
- **THEN** 不发布 GetResult，不改变原有 snapshot，也不创建独立提交或等待
