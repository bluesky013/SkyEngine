## Context

旧 `engine/render` 材质三级：

- `Technique`：`ShaderRef`（shader 名 + entry points）+ `ShaderCollectionPtr`；`GraphicsTechnique` 加 `PipelineState`（depth/raster/blend）+ `RequestProgram(variantKey)` + `rasterID` + `vertexFlags`。
- `Material`：`std::vector<RDGfxTechPtr>` + `storage`（属性字节）+ `MaterialPropertyMap`（`Name→{offset,size}`）+ `textures`。
- `MaterialInstance`：`RDMaterialPtr` + 自己的 storage/textures + 版本号。
- `RenderTechniqueLibrary`：`Name→RDGfxTechPtr` 单例表（techID 收集/查表）。

aurora 现状：

- `GraphicsPipeline::Descriptor { PipelineState* state; Shader* shader; AttachmentFormat format; }`（`GraphicsPipeline` 仍是空 stub）。
- `PipelineState { DepthStencil; MultiSample; InputAssembly; RasterState; BlendState[] }`。
- `Shader : RefObject`；`Texture : RenderResource`（**非** RefObject，不能 `CounterPtr`）。
- `SceneRasterQueue.techniqueTag`（Name，队列过滤，空=不过滤）。
- `Mesh::SubMesh.materialIndex`（占位）。

约束：`namespace sky::aurora`；header-only；命名遵守 coding rules。

## Goals / Non-Goals

**Goals:**

- `Material`（`RefObject`）：**内部按 technique 维护**（`MaterialTechnique{tag, shader, state}` 列表）+ material 级属性（值/纹理）。
- `MaterialInstance`（`RefObject`）：共享 `Material` + **部分字段 runtime 稀疏覆盖**（未覆盖继承 material）。
- 最小测试：technique 列表/属性读写/纹理 + instance 部分覆盖/继承。

**Non-Goals:**

- 不做 `Technique` 顶层资源 / `RenderTechniqueLibrary`（technique 内联为 material 内部结构）。
- 不做 PSO 构建/缓存（`GraphicsPipeline` 是 stub）。
- 不做 shader-reflection 派生属性布局（v1 显式 `AddValue/AddTexture`）。
- 不做材质 asset 加载/序列化。
- 不做多 pass 分桶编排（随 `aurora-renderer`）。

## Decisions

### D1: technique 组织保留在 Material 内部（不独立成顶层资源）

```cpp
struct MaterialTechnique {
    Name               techniqueTag;   // 供 RDG queue 过滤分桶
    CounterPtr<Shader> shader;
    PipelineState      state;          // depth/raster/blend
};

class Material : public RefObject {
public:
    void AddTechnique(const MaterialTechnique &technique);
    const std::vector<MaterialTechnique> &GetTechniques() const;
    const MaterialTechnique *GetTechnique(const Name &tag) const;
private:
    std::vector<MaterialTechnique> techniques;
    // ... 属性 ...
};
```

- **理由**：technique 的职责（持 shader + state，按 tag 区分 pass）保留，但**内联为 material 内部结构**——不再有独立 `Technique` 资源与 `RenderTechniqueLibrary` 查表。material 直接携带 tag，collector 按 tag 分桶。
- **备选**：完全扁平（material 单个 shader+state）—— 被否，多 pass（shadow/prepass/main）需要每 pass 的 shader/state。
- **备选**：独立 `Technique : RefObject` + 顶层注册表 —— 被否，正是要消除的「收集」层。

### D2: 属性系统（material 级 + 值字节 + 纹理）

```cpp
enum class MaterialPropertyType : uint8_t { VALUE, TEXTURE };

struct MaterialPropertyEntry {
    MaterialPropertyType type = MaterialPropertyType::VALUE;
    uint32_t offset = 0;   // VALUE: storage 字节偏移; TEXTURE: textures 索引
    uint32_t size   = 0;   // VALUE: 字节数
};
using MaterialPropertyMap = std::unordered_map<Name, MaterialPropertyEntry>;

class Material : public RefObject {
public:
    void AddValue(const Name &name, uint32_t size, const void *defaultValue);
    void AddTexture(const Name &name);

    template <typename T> void SetValue(const Name &name, const T &value);
    template <typename T> bool GetValue(const Name &name, T &out) const;
    void     SetTexture(const Name &name, Texture *texture);
    Texture *GetTexture(const Name &name) const;

    const MaterialPropertyMap &GetPropertyMap() const;
private:
    std::vector<uint8_t>   storage;     // 默认值
    MaterialPropertyMap    properties;
    std::vector<Texture *> textures;
};
```

- **属性是 material 级**（跨 technique 共享）：同一份材质数据绑定到各 pass（各 pass 用其子集）。
- **`offset` 语义分离**：VALUE 用 storage 字节偏移，TEXTURE 用 textures 索引（旧设计复用同一字段，语义混淆）。
- **`Texture *` 非拥有**：`Texture : RenderResource` 非 `RefObject`，生命周期由外部管理。

### D3: MaterialInstance 部分字段 runtime 稀疏覆盖

```cpp
class MaterialInstance : public RefObject {
public:
    void SetMaterial(CounterPtr<Material> material);
    Material *GetMaterial() const;

    // 稀疏覆盖：只存被覆盖字段；未覆盖回退 material
    template <typename T> void SetValue(const Name &name, const T &value);
    template <typename T> bool GetValue(const Name &name, T &out) const;   // override 或继承
    void     SetTexture(const Name &name, Texture *texture);
    Texture *GetTexture(const Name &name) const;                           // override 或继承

    bool IsOverridden(const Name &name) const;
private:
    CounterPtr<Material> material;
    std::unordered_map<Name, std::vector<uint8_t>> valueOverrides;   // 稀疏
    std::unordered_map<Name, Texture *>            textureOverrides; // 稀疏
};
```

- **稀疏覆盖（partial override）**：instance 只记录被覆盖的字段；`GetValue/GetTexture` 优先 override，否则回退 material 默认值。未覆盖字段**继承** material（material 后续改默认值，instance 未覆盖处跟随）。
- **`SetMaterial` 只存引用**：不像旧 `MaterialInstance` 那样分配整份 storage 拷贝（那是「全量拷贝」而非「部分覆盖」）。
- **理由**：运行时常常只覆盖少数字段（如只改 baseColor），稀疏覆盖省内存且语义是「继承 + 覆盖」。
- **备选**：全量 storage 拷贝 + dirty 位（旧做法）—— 被否，非「部分」语义，且 material 默认值变更不传播。

### D4: RenderResource 继承 RefObject（引用计数）+ 所有权

`RenderResource` 改为 `public RefObject`，使渲染资源层（`Texture`/`Buffer`）可 `CounterPtr` 共享。

- 材料层所有权：
  - `MaterialTechnique.shader`：`CounterPtr<Shader>`（`Shader : RefObject`）。
  - `Material`/`MaterialInstance` 纹理：`CounterPtr<Texture>`（依赖本轮 `RenderResource : RefObject`）。
  - `MaterialInstance` 的 material：`CounterPtr<Material>`。
- **理由**：纹理是共享资源（一个纹理被多个 material 引用），必须引用计数避免裸指针悬垂；`Shader`/`Material`/`RenderGeometry` 已是 RefObject，统一模型。用户确认「用智能指针 hold 引用」。
- **兼容性**：`RenderGeometry` 的 `unique_ptr<VertexBuffer>` 拥有语义不变（`RefObject` 可被 `unique_ptr` 拥有）；栈对象 `VertexBuffer vb;`（无 `CounterPtr`）不变；`RefObject::OnExpire()` → `delete this`，子类析构（`Texture`/`StaticBuffer` 的 `WaitUploadComplete`）正常触发。
- **代价**：改 base 类（`RenderResource.h`），属 `aurora-resource` 能力。

### D5: PSO 构建延后

`Material` 只持 technique 的 `{shader, state}`，不建 PSO。`GraphicsPipeline` 当前是空 stub，待其落地后由 technique 的 shader+state + attachment format + vertex layout 产出（另开 change）。

- **理由**：现在写接口也是空壳，YAGNI；数据模型是可落地且下游需要的部分。
- **备选**：现在加 `BuildPipeline` 空接口 —— 被否，无消费者且底层 stub。

### D6: 位置与所有权

- 文件：`engine/aurora/core/include/aurora/resource/Material.h`（header-only）。
- `MaterialTechnique.shader`：`CounterPtr<Shader>`。
- `Material`/`MaterialInstance` 纹理：`CounterPtr<Texture>`（依赖 D4）。
- `MaterialInstance.material`：`CounterPtr<Material>`。

## Risks / Trade-offs

- **[属性布局手写易错]** `AddValue(name, size, default)` 的 size 必须与 `SetValue<T>` 的 `sizeof(T)` 一致。→ 缓解：`SetValue<T>` 校验 `sizeof(T) == entry.size`，不符不写（+assert）；测试覆盖。
- **[稀疏覆盖的查询开销]** 每字段查询走 map。→ 缓解：material 属性数量通常小；v1 数据模型优先，GPU 打包留后续。
- **[RenderResource 改基类影响面]** `RenderResource : RefObject` 影响所有 buffer/texture 子类与测试。→ 缓解：兼容（unique_ptr 拥有 / 栈对象不变），`AuroraCoreTest` 全绿兜底。
- **[techniqueTag 只是数据]** v1 只存 tag，实际分桶编排未接。→ 缓解：Non-Goal 明示，tag 与 `SceneRasterQueue.techniqueTag` 对齐。

## Migration Plan

1. `Material.h` 落地（`MaterialTechnique` + `Material` + `MaterialInstance`）。
2. `MaterialTest.cpp`：technique 列表/属性读写/纹理 + instance 部分覆盖/继承。
3. archive。

## Open Questions

- PSO 构建/缓存何时落地（依赖 RHI PSO + renderer）。
- 属性布局是否改为 shader-reflection 派生。
- 多 pass 如何用 techniqueTag 编排——随 `aurora-renderer`。
- 稀疏覆盖是否需要在 GPU 上传时打包为连续块。
