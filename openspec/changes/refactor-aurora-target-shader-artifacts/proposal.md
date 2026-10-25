## Why

各目标已经能生成自己的 shader，但当前反射仍混合逻辑接口与目标 ABI：Metal 借第二份 SPIR-V layout 恢复逻辑键，缺失 native metadata 会进入顺序槽位约定，Pass 上传要求目标布局等于 C++ 内存布局。缓存的实际工具链指纹为零，且公共资源分类丢失只读 buffer 的 DXIL register class，需要将这批问题收敛为同一个产物与绑定契约。

## What Changes

- 将共享声明的逻辑接口、CPU 数据描述、目标 ABI 分开；资源归属为 Global / Pass / Material / Batch，Pass 内的 View 输入可选。
- 每个目标独立产出代码、活跃资源、原生定位与字节布局；共享声明提供稳定逻辑资源键，移除 Metal 的辅助 SPIR-V layout 和跨目标名字拼接。
- **BREAKING**：引入带完整身份的 stage artifact，统一 code format、精确代码长度、entry/stage、TargetEnvironment、生产者指纹、逻辑接口、目标反射及内容校验；迁移现有 index/blob，旧产品需重新 cook。
- 禁止从空反射、资源顺序或缺失 stage 记录猜测 native binding；显式提供无资源反射，手写后端 shader 同样提交受检的完整映射。
- 由现有 Shader/RHI 构建不可变 BindingPlan；保留资源访问方式和原生类别，贯穿修复只读 structured/raw buffer 的 SRV/UAV 选择。
- 由 CPU 描述与目标布局生成 UniformUploadPlan；支持本期字段集合的 offset/stride/矩阵转换，ABI 完全一致时保留整块复制快路。
- 将 ResourceGroup 的受检参数更新统一为 Name-first 事务：数据字段/完整 block、image、sampler、buffer 经同一逻辑参数表解析；材质属性与实例覆盖使用显式 Name 映射进入该通路，不再独立预打包目标字节。
- 补齐普通 2D 材质贴图从资产引用、精确资源版本冻结、图读依赖到命名绑定的最小闭环，明确 sRGB 颜色/线性数据语义；资产解析留在 adaptor，RHI 不识别 UUID/AssetManager。
- 开发编译与离线 cook 共用编译身份和验证；shipping 从发布清单解析产物，检查运行时 ABI/设备兼容性，无需当前编译器或作者源码。

## Capabilities

### New Capabilities

无；在既有 shader 与 resource-binding capability 内修订契约。

### Modified Capabilities

- `aurora-shader`：逻辑接口与目标 ABI 分离、单目标反射、完整 stage artifact、编译与运行时兼容身份、严格反射与缓存校验。
- `aurora-resource-binding`：逻辑键到原生地址的 BindingPlan、访问方式、CPU 到目标 uniform 的 UniformUploadPlan、原子装配与生命周期。
- `aurora-material`：受检的 Name 参数/资源入口、材质属性到 shader 名称映射、实例覆盖与普通 2D 贴图的版本化传递。

## Impact

影响现有 `SharedShader`/导出工具、`ShaderCompile`/`ShaderReflection`、Slang adapter、ShaderTechnique/RasterShaderResolver、cache codec/resolver/cook/package、PassResourceBindings、Material/MaterialInstance、材质资产 schema/SceneAssetSource、材质图片依赖与参数上传，以及 Metal/Vulkan/DX12 的 Shader/ResourceGroup/descriptor 编码路径与相关测试。

Aurora 保留 `engine/aurora` foundation 归属及现有 targets、目录和装载入口；不恢复独立 PipelineLayout API，不增加业务 plugin 或第二套资源注册/分发系统。无需新增第三方依赖。

## Scope and sequencing

状态：代码已实施并完成当前 macOS 宿主验收；跨平台原生验收仍待对应宿主/设备，见 tasks.md 与 implementation.md。基线为已实现但未归档的 `refactor-aurora-pipeline-resource-ownership`；本 change 接续其 shader ABI 边界，不重做技术选择、资源注册、View 归属或 surface 生命周期。与既有 changes 的精确关系见 design.md。

本期保持平坦 ConstantBuffer、现有 uniform 字段集合与受支持的直接资源绑定；修复已可达的只读/可写 structured/raw buffer 链路。descriptor arrays、argument buffers、bindless、嵌套 uniform、通用 shader 热重载及强制离线 metallib 不在本期扩展范围。已有其他后端能力不得因本期有限支持集合而被静默降级。

2026-10-23 用户补充要求统一材质参数入口，因此本期纳入 Name-first ResourceGroup 与普通 2D 贴图闭环。该补充不代表本期自动支持材质 cube/压缩/storage/descriptor-array 资产传递；执行路径不支持的资源仍明确拒绝。

commit、archive 仍分别遵循用户确认规则；本方案不包含执行这两项操作的授权。
