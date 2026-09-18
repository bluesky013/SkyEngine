## 1. RenderResource 继承 RefObject（引用计数）

- [x] 1.1 `RenderResource.h`：`class RenderResource : public RefObject`
- [x] 1.2 验证现有 resource（Buffer/Texture/`unique_ptr` 拥有）不受影响（`AuroraCoreTest` 全绿）

## 2. Material（Material.h）

- [x] 2.1 `MaterialTechnique { techniqueTag, shader, state }` + `Material::AddTechnique/GetTechniques/GetTechnique(tag)`
- [x] 2.2 `MaterialPropertyType` / `MaterialPropertyEntry{type, offset, size}` / `MaterialPropertyMap`
- [x] 2.3 `Material::AddValue`/`AddTexture`（布局）+ `SetValue<T>`/`GetValue<T>`/`SetTexture`/`GetTexture`（`CounterPtr<Texture>`，sizeof 校验）+ `GetPropertyMap`

## 3. MaterialInstance（Material.h）

- [x] 3.1 `MaterialInstance : RefObject`：`SetMaterial`/`GetMaterial`
- [x] 3.2 稀疏覆盖：`SetValue<T>`/`GetValue<T>`（优先 override，回退 material）、`SetTexture`/`GetTexture`、`IsOverridden`
- [x] 3.3 未设 material 时 no-op / 返回 false

## 4. 测试

- [x] 4.1 `core/test/MaterialTest.cpp`：technique 增查
- [x] 4.2 属性布局/值读写（含 size 不符不写）/纹理挂接（`CounterPtr<Texture>`）
- [x] 4.3 MaterialInstance：部分覆盖 + 继承 + `IsOverridden` + 不污染共享 Material + 未设 material no-op

## 5. 验证与收尾

- [x] 5.1 `cmake --build` AuroraCore 通过
- [x] 5.2 `AuroraCoreTest` 全绿
- [ ] 5.3 `openspec archive aurora-material` 归档（需用户确认）
