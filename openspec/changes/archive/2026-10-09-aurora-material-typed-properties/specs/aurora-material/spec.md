## MODIFIED Requirements

### Requirement: Material 属性系统（material 级，值 + 纹理）

`Material` SHALL 提供 material 级属性（跨 technique 共享）：

- `MaterialPropertyType` SHALL 复用 shader 的字段类型 `RgFieldType`（`using MaterialPropertyType = RgFieldType`）：值类型 `FLOAT / FLOAT2 / FLOAT3 / FLOAT4 / MAT4 / INT / UINT / BOOL`，纹理类型 `TEXTURE2D / TEXTURE_CUBE / SAMPLER`；SHALL NOT 再造一个仅 material 的类型枚举。
- `AddValue(Name, MaterialPropertyType type, const void *defaultValue)` / `AddTexture(Name, MaterialPropertyType type = TEXTURE2D)` 声明布局；`MaterialPropertyEntry` SHALL 为 `{ type, offset }`（value 为 storage 字节偏移、texture 为 textures 索引）。
- SHALL NOT 存储 `size`；字节大小 SHALL 由 `constexpr SizeOfPropertyType(type)` 派生。
- `SetValue<T>(Name, const T &)` / `GetValue<T>(Name, T &out)` SHALL 按 `sizeof(T)` 是否等于派生大小来校验声明类型；不匹配或未声明时 SHALL NOT 写 / `GetValue` 返回 false。
- `SetTexture(Name, CounterPtr<Texture>)` / `GetTexture(Name) -> Texture *`。
- `GetPropertyMap()` 返回 `MaterialPropertyMap`（`Name → {type, offset}`）。

`Material` SHALL 以 `CounterPtr<Texture>` 持有纹理（引用计数，`RenderResource` 继承 `RefObject`，见 `aurora-resource` spec）。

#### Scenario: 按类型声明并读写

- **WHEN** `AddValue(Name("baseColor"), MaterialPropertyType::FLOAT4, &white)` 后 `SetValue(Name("baseColor"), Vector4(1,0,0,1))` 再 `GetValue`
- **THEN** 读回 `Vector4(1,0,0,1)`；`GetPropertyMap().at("baseColor") == {FLOAT4, 0}`；未声明的 key `GetValue` 返回 false

#### Scenario: size 不匹配不写

- **WHEN** `AddValue(Name("baseColor"), MaterialPropertyType::FLOAT4, &white)` 后 `SetValue(Name("baseColor"), 1.5f)`
- **THEN** 底层值不变（`sizeof(float) != SizeOfPropertyType(FLOAT4)`）

#### Scenario: size 由 type 派生

- **WHEN** 阅读 `MaterialPropertyEntry`
- **THEN** 无 `size` 字段；字节大小经 `SizeOfPropertyType(type)` 获得（如 `FLOAT4 == 16`、`MAT4 == 64`）

#### Scenario: 类型词汇与 shader 共用

- **WHEN** 阅读 `aurora/resource/Material.h`
- **THEN** `MaterialPropertyType` 是 `RgFieldType` 的别名，material 不再定义自己的 `{ VALUE, TEXTURE }` 枚举

#### Scenario: 纹理挂接

- **WHEN** `AddTexture(Name("albedo"))` 后 `SetTexture(Name("albedo"), tex)`
- **THEN** `GetTexture(Name("albedo")) == tex.Get()`

### Requirement: MaterialInstance 部分字段 runtime 稀疏覆盖

`MaterialInstance`（`RefObject`）SHALL 持 `CounterPtr<Material>` + **覆盖**（只对被改写的字段生效）。

- 值覆盖 SHALL 存于单个连续存储（`material` 值块的一份拷贝，首次写入时惰性拷贝）；SHALL NOT 为每个被覆盖字段单独分配容器。
- `SetValue<T>(Name, const T &)` 经 `material->FindValue` 解析 entry（同 `Material` 的 `sizeof` 校验），在拷贝中原地写 `entry.offset`。
- `GetValue<T>` SHALL 读本拷贝；未发生拷贝时回退 material 默认值。
- `SetTexture(Name, CounterPtr<Texture>)` 以稀疏 map 记录覆盖纹理；`GetTexture` 优先覆盖，否则回退 material。
- `IsOverridden(Name)` SHALL 对值字段返回「本拷贝与 material 默认是否不同」，对纹理字段返回是否在覆盖 map 中。
- `SetMaterial` SHALL 清空覆盖（拷贝与纹理覆盖）。
- 未设 material 时 `SetValue/SetTexture` SHALL be no-op，`GetValue` 返回 false。

#### Scenario: 部分覆盖 + 继承

- **WHEN** `instance.SetMaterial(mat)` 后只 `instance.SetValue(Name("baseColor"), red)`
- **THEN** `instance.GetValue("baseColor")` 为 red；`instance.GetValue("roughness")` 回退为 mat 的默认值；`IsOverridden("baseColor") == true`、`IsOverridden("roughness") == false`

#### Scenario: 覆盖不污染共享 Material

- **WHEN** `instance.SetValue(Name("baseColor"), red)` 后读 `mat.GetValue("baseColor")`
- **THEN** mat 的默认值不变

#### Scenario: 值覆盖为单块连续存储

- **WHEN** 一个 `MaterialInstance` 只覆盖一个 `float` 字段
- **THEN** 值存于该实例的单块连续存储中，不存在每个覆盖字段各自的容器分配

#### Scenario: 未设 material

- **WHEN** 未 `SetMaterial` 的 `MaterialInstance` 调 `SetValue`/`GetValue`
- **THEN** 不崩溃（`SetValue` no-op、`GetValue` 返回 false）
