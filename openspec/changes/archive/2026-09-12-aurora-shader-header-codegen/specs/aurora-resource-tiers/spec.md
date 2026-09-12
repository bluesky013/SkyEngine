## MODIFIED Requirements

### Requirement: RgBlockDesc 单一事实源

`RgBlockDesc` SHALL 描述一个 resource block：`{ set, binding, blockName, fields[] }`。其 `set/binding/kind/fields` SHALL 由 `.slang` shader 反射生成（而非手写），同一份 desc SHALL 可产出：

- RHI `ResourceGroupLayout::Descriptor`（cbuffer → `UNIFORM_BUFFER`/`UNIFORM_BUFFER_DYNAMIC`；texture/sampler → 对应 `DescriptorType`）
- 供 shader `#include` 的 Slang 共享头（struct + `ParameterBlock` + `[[vk::binding]]`）
- C++ 镜像 struct（与 shader 布局一致，带 `static_assert` 校验）

C++ 侧 UBO 写入与 shader 声明 SHALL 使用同一 offset 表（offset 来自 slang 反射）。

#### Scenario: 反射派生 desc

- **WHEN** 对含 `[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal` 的 `.slang` 做 codegen
- **THEN** 生成 `RgBlockDesc{set=0, binding=0, blockName="Global", kind=CBUFFER}`，字段与 `GlobalParams` 成员一致

#### Scenario: 一致性

- **WHEN** 同一 `.slang` 反射分别产出 layout、shader 头、C++ struct
- **THEN** binding 编号、字段顺序、类型、offset 在两侧一致
