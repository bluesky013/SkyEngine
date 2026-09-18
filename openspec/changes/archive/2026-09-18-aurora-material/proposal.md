## Why

aurora 有 shader（slang 编译 + 反射）、`ResourceGroup`、`PipelineState`、`SceneRasterQueue.techniqueTag`，但没有材质系统——`Mesh::SubMesh.materialIndex` 只是占位索引。渲染需要「怎么渲染（shader + 状态，按 pass/technique 组织）+ 用什么值（属性/纹理，可运行时部分覆盖）」的封装。

旧 `engine/render` 的材质分三级：`Technique`（shader + `PipelineState`）→ `Material`（多个 technique + 属性布局）→ `MaterialInstance`（per-instance 值），外加 `RenderTechniqueLibrary`（`Name→Technique` 单例查表做「techID 收集」）。工程化后发现 **technique 作为独立顶层资源 + 查表库较鸡肋**：它主要解决「状态持有」和「techID 查表」。

本 change 落地 aurora 材质系统 v1：**technique 组织保留在 material 内部**（每个 technique = tag + shader + state，不独立成顶层资源、不建查表库）；`MaterialInstance` 支持**部分字段 runtime override**（稀疏覆盖，未覆盖字段继承 material）。

## What Changes

- **新增 `Material`**（`aurora/resource/Material.h`，header-only，`RefObject`）：
  - **内部按 technique 维护**：`std::vector<MaterialTechnique>`，每个 `MaterialTechnique { Name techniqueTag; CounterPtr<Shader> shader; PipelineState state; }`（tag 供 RDG queue 过滤分桶）。提供 `AddTechnique/GetTechniques/GetTechnique(tag)`。
  - 属性系统（material 级，跨 technique 共享）：`storage`（属性字节）+ `MaterialPropertyMap`（`Name → {type, offset, size}`）+ `textures`；`AddValue/AddTexture`（布局）、`SetValue<T>/GetValue<T>/SetTexture/GetTexture`。
- **新增 `MaterialInstance`**（同文件，`RefObject`）：持 `CounterPtr<Material>` + **稀疏覆盖**——只存被覆盖的字段（值/纹理），未覆盖字段 `GetValue/GetTexture` 回退到 material；`IsOverridden(Name)` 查询。
- **不引入 `Technique` 顶层资源 / `RenderTechniqueLibrary`**（technique 内联为 material 内部结构）。
- **`RenderResource` 改为继承 `RefObject`**（引用计数）：使 `CounterPtr<Texture>` / `CounterPtr<VertexBuffer>` 可用；material/instance 的纹理用 `CounterPtr<Texture>` 持有（不再裸指针）。
- PSO 构建延后（`GraphicsPipeline` 当前是 stub，待 RHI PSO 实装后由 `{technique.shader, technique.state}` 产出）。

## Capabilities

### New Capabilities

- `aurora-material`: 材质系统——`Material`（内部 technique 列表 + 属性）+ `MaterialInstance`（部分字段 runtime 稀疏覆盖）。

### Modified Capabilities

- `aurora-resource`: `RenderResource` 继承 `RefObject`（引用计数），渲染资源层可 `CounterPtr` 共享。

## Impact

- **新增文件**：`engine/aurora/core/include/aurora/resource/Material.h`（header-only，随 `aurora/core` GLOB 自动纳入）。
- **修改文件**：`engine/aurora/core/include/aurora/resource/RenderResource.h`（继承 `RefObject`）。
- **测试**：`engine/aurora/core/test/MaterialTest.cpp`（technique 列表/属性读写/纹理 + MaterialInstance 部分覆盖/继承）。
- **依赖**：`aurora/rhi`（`Shader`/`PipelineState`）、`aurora/resource/Texture.h`、`core`（`Name`/`RefObject`/`CounterPtr`）。
- **不影响**：RHI 接口层、`Mesh`/`RenderGeometry`/`Texture` 契约。
- **后续（Non-Goal）**：PSO 构建/缓存、shader-reflection 派生属性布局、材质 asset 加载、多 pass 分桶编排（随 `aurora-renderer`）。
