## ADDED Requirements

### Requirement: Material 内部按 technique 维护

`Material`（`RefObject`）SHALL 内部持有 `std::vector<MaterialTechnique>`，每个 `MaterialTechnique { Name techniqueTag; CounterPtr<Shader> shader; PipelineState state; }`。SHALL 提供 `AddTechnique` / `GetTechniques` / `GetTechnique(const Name &tag)`（按 tag 查，未找到返回 `nullptr`）。

`Material` SHALL NOT 引入顶层 `Technique` 资源类，SHALL NOT 依赖 `RenderTechniqueLibrary` 之类查表库。

#### Scenario: 添加并按 tag 查 technique

- **WHEN** `material.AddTechnique({Name("opaque"), shader, state})`
- **THEN** `GetTechniques().size() == 1`；`GetTechnique(Name("opaque"))->shader == shader`；`GetTechnique(Name("missing")) == nullptr`

#### Scenario: 无顶层 Technique 资源

- **WHEN** 阅读 `aurora/resource/Material.h`
- **THEN** 无顶层 `Technique` 类 / `RenderTechniqueLibrary`；technique 为 material 内部结构

### Requirement: Material 属性系统（material 级，值 + 纹理）

`Material` SHALL 提供 material 级属性（跨 technique 共享）：

- `AddValue(Name, uint32_t size, const void *defaultValue)` / `AddTexture(Name)` 声明布局（`offset`：VALUE 为 storage 字节偏移、TEXTURE 为 textures 索引；`size`：VALUE 字节数）。
- `SetValue<T>(Name, const T &)` / `GetValue<T>(Name, T &out)`：按 `sizeof(T)` 写/读 `storage`；`sizeof(T) != entry.size` 或未声明时 SHALL NOT 写 / `GetValue` 返回 false。
- `SetTexture(Name, CounterPtr<Texture>)` / `GetTexture(Name) -> Texture *`。
- `GetPropertyMap()` 返回 `MaterialPropertyMap`（`Name → {type, offset, size}`）。

`Material` SHALL 以 `CounterPtr<Texture>` 持有纹理（引用计数，`RenderResource` 继承 `RefObject`，见 `aurora-resource` spec）。

#### Scenario: 值读写

- **WHEN** `AddValue(Name("baseColor"), sizeof(Vector4), &white)` 后 `SetValue(Name("baseColor"), Vector4(1,0,0,1))` 再 `GetValue`
- **THEN** 读回 `Vector4(1,0,0,1)`；未声明的 key `GetValue` 返回 false

#### Scenario: 纹理挂接

- **WHEN** `AddTexture(Name("albedo"))` 后 `SetTexture(Name("albedo"), tex)`
- **THEN** `GetTexture(Name("albedo")) == tex.Get()`

### Requirement: MaterialInstance 部分字段 runtime 稀疏覆盖

`MaterialInstance`（`RefObject`）SHALL 持 `CounterPtr<Material>` + **稀疏覆盖**（只存被覆盖的字段）。

- `SetValue<T>(Name, const T &)` 记录覆盖值；`GetValue<T>` SHALL 优先返回覆盖值，未覆盖则回退 material 默认值。
- `SetTexture(Name, CounterPtr<Texture>)` 记录覆盖纹理；`GetTexture` 优先覆盖，否则回退 material。
- `IsOverridden(Name)` 返回该字段是否被覆盖。
- 未设 material 时 `SetValue/SetTexture` SHALL be no-op，`GetValue` 返回 false。

#### Scenario: 部分覆盖 + 继承

- **WHEN** `instance.SetMaterial(mat)` 后只 `instance.SetValue(Name("baseColor"), red)`
- **THEN** `instance.GetValue("baseColor")` 为 red；`instance.GetValue("roughness")` 回退为 mat 的默认值；`IsOverridden("baseColor") == true`、`IsOverridden("roughness") == false`

#### Scenario: 覆盖不污染共享 Material

- **WHEN** `instance.SetValue(Name("baseColor"), red)` 后读 `mat.GetValue("baseColor")`
- **THEN** mat 的默认值不变

#### Scenario: 未设 material

- **WHEN** 未 `SetMaterial` 的 `MaterialInstance` 调 `SetValue`/`GetValue`
- **THEN** 不崩溃（`SetValue` no-op、`GetValue` 返回 false）
