## Context

aurora 资源层现状：

- `VertexBuffer`（`aurora/resource/Buffer.h`）：带 `VertexLayout`（stride + `VertexSemanticMask` + `VertexInputRate`），`Init(dev, size)` + `Upload(data, size)`。
- `IndexBuffer`：带 `IndexType`（U16/U32），`Init(dev, size)` + `Upload`。
- `RenderGeometry`（`aurora/resource/RenderGeometry.h`）：`RefObject`，持有 `std::vector<std::unique_ptr<VertexBuffer>>` + `std::unique_ptr<IndexBuffer>` + `AABB`，纯封装（调用方建 buffer 交进来）。

旧 `engine/render` 的参考：

- `MeshInterface.h`：`MeshVertexDataInterface`（抽象，`Resize/GetStride/Num/GetDataPointer` + 模板 `SetVertexData/GetVertexData`）、`TRawMeshVertexData<T>`（`BinaryData` 承载）、`RawMeshIndexData`（U16/U32）。
- `Mesh.h`：`Mesh : RenderResource`，`SubMesh`（firstVertex/vertexCount/firstIndex/indexCount/materialIndex/aabb）+ `RenderGeometryPtr geometry` + `MeshUploadData`。

业界调研结论（Unity/Unreal/glTF/Filament）：mesh 接口 = 顶点数据布局 + submesh 分段 + **blend shape = delta 增量混合**（`base + Σ weightᵢ·deltaᵢ`）+ 蒙皮/LOD。本 change 落地 CPU 数据接口 + 资源侧 `Mesh` + blend shape 数据结构。

约束：`namespace sky::aurora`；header-only（沿用 `Buffer.h`/`RenderGeometry.h` 模式）；`Mesh` 是 `RenderGeometry` 之上的纯封装（沿用「调用方建 buffer 交进来」哲学）；命名遵守 coding rules（无尾下划线、注释只解释 why）。

## Goals / Non-Goals

**Goals:**

- CPU 侧数据接口：`MeshVertexDataInterface` + `TRawMeshVertexData<T>` + `RawMeshIndexData`。
- 资源侧 `Mesh`：`RefObject`，持有 `RenderGeometry` + `SubMesh` 列表 + `BlendShape` 列表。
- `SubMesh`：firstIndex/indexCount/vertexCount/materialIndex/bounds。
- `BlendShape`：delta 增量（position/normal/tangent），与 base 顶点一一对应。
- **蒙皮**：顶点语义 `JOINTS`/`WEIGHTS` + `Skeleton` 资源 + `Mesh::SetSkeleton/HasSkin`。
- 最小测试：数据接口构建/存取 + `Mesh` 结构 + `RenderGeometry` 挂接 + 蒙皮挂接。

**Non-Goals:**

- 不做 GPU morphing（blend shape 的独立 morph buffer + vertex shader 混合）——依赖 skinning/渲染管线，后续 change。
- 不做 GPU 蒙皮执行（每帧 joint matrix 的 skinning buffer + vertex shader `Σ weight·joint·inverseBind·base`）——依赖动画/渲染管线，后续 change。
- 不做 LOD（`Mesh` 上的多级 index/vertex range）。
- 不做材质系统（`materialIndex` 只是占位索引）。
- 不做 mesh 上传便捷封装（调用方沿用 `RenderGeometry` 模式：建 buffer → `Upload` → 交进来）。

## Decisions

### D1: 文件与位置（header-only）

- `engine/aurora/core/include/aurora/resource/MeshData.h`（CPU 侧数据接口）。
- `engine/aurora/core/include/aurora/resource/Mesh.h`（资源侧 `Mesh`，`SubMesh`/`BlendShape`）。

- **理由**：与 `Buffer.h`/`RenderGeometry.h` 同目录同模式；两者职责不同（数据 vs 资源），分文件清晰。
- **备选**：合成一个 `Mesh.h` —— 被否，CPU 数据接口与 GPU 资源语义不同，且 `MeshData.h` 可被离线导入/工具复用。

### D2: CPU 数据接口用 BinaryData 承载字节

`TRawMeshVertexData<T>` / `RawMeshIndexData` 用 `core/archive/BinaryData`（`RefObject` 字节缓冲：`Data()` / `Resize(uint32_t)` / `Size()`）承载字节，与旧 `MeshInterface.h` 一致。

- **理由**：`BinaryData` 提供 `RefObject` 生命周期管理（`BinaryDataPtr` 共享），且旧引擎 `MeshInterface.h` 已用它，移植保持一致；数据接口作为「可共享的字节缓冲」语义更贴合。
- **备选**：`std::vector<uint8_t>` —— 被否，无 RefObject 共享能力，且与旧接口不一致。

### D3: MeshVertexDataInterface 抽象 + 模板具体类

```cpp
class MeshVertexDataInterface {
public:
    virtual ~MeshVertexDataInterface() = default;
    virtual void     Resize(uint32_t count) = 0;
    virtual uint32_t GetStride() const = 0;
    virtual uint32_t Count() const = 0;
    virtual uint8_t       *GetData() = 0;
    virtual const uint8_t *GetData() const = 0;

    template <typename T>
    void SetVertexData(uint32_t index, const T &v, uint32_t offset = 0);
    template <typename T>
    const T &GetVertexData(uint32_t index, uint32_t offset = 0) const;
};

template <typename T>
class TRawMeshVertexData : public MeshVertexDataInterface {
    // stride = sizeof(T); BinaryDataPtr data; (Resize 重建 + memcpy)
};
```

- **理由**：抽象接口让「顶点结构 T」与「字节缓冲」解耦，支持未来不同 mesh 类型（static/skinned）与不同数据来源（内存/文件/程序生成）复用同一套构建/上传逻辑（对齐旧 `MeshInterface.h` + Unity 分离数组模型）。const 重载让 `GetVertexData` 可在 const 上下文读。
- **备选**：不抽象，直接 `std::vector<T>` —— 被否，失去类型无关的统一构建接口，且旧引擎已有此抽象的先例。

### D4: RawMeshIndexData 按 IndexType 存取

```cpp
class RawMeshIndexData {
public:
    explicit RawMeshIndexData(IndexType type);
    RawMeshIndexData(uint32_t count, IndexType type);
    void Resize(uint32_t count);
    uint32_t Count() const;
    uint8_t *GetData();
    void     SetIndex(uint32_t idx, uint32_t val);
    uint32_t GetIndex(uint32_t idx) const;
    IndexType GetIndexType() const;
private:
    IndexType     indexType;
    BinaryDataPtr data;   // 每元素 2 或 4 字节
};
```

- **理由**：U16/U32 双格式，按 `GetIndexSize()` 存取值；`GetData()` 直接喂 `IndexBuffer::Upload`。`SetIndex` 在 U16 时按低 16 位写入，调用方保证 `val ≤ 0xFFFF`。
- **备选**：模板化 `TRawMeshIndexData<T>` —— 被否，索引类型是运行时选择（U16 优化 vs U32 大网格），`IndexType` 成员更贴切。

### D5: SubMesh / BlendShape / Mesh 结构

```cpp
struct SubMesh {
    uint32_t firstVertex   = 0;
    uint32_t vertexCount   = 0;
    uint32_t firstIndex    = 0;
    uint32_t indexCount    = 0;
    uint32_t materialIndex = 0;   // technique/material 占位索引
    AABB     bounds{};
};

struct BlendShape {
    Name               name;
    std::vector<Vector3> positionDeltas;   // 与 base 顶点一一对应
    std::vector<Vector3> normalDeltas;     // 可空
    std::vector<Vector3> tangentDeltas;    // 可空
};

class Mesh : public RefObject {
public:
    Mesh() = default;
    explicit Mesh(const Name &inName);

    void SetGeometry(CounterPtr<RenderGeometry> geo);
    RenderGeometry *GetGeometry() const;

    void AddSubMesh(const SubMesh &sub);
    const std::vector<SubMesh> &GetSubMeshes() const;

    void AddBlendShape(BlendShape shape);
    const std::vector<BlendShape> &GetBlendShapes() const;
    uint32_t GetBlendShapeCount() const;

    const Name &GetName() const;
    const AABB &GetLocalBounds() const;   // 委托 geometry->GetLocalBounds()

private:
    Name name;
    CounterPtr<RenderGeometry> geometry;
    std::vector<SubMesh>     subMeshes;
    std::vector<BlendShape>  blendShapes;
};
```

- **SubMesh**：对齐旧 `MeshSubSection`（`firstVertex`/`vertexCount` 定义子网格顶点区间，`firstIndex`/`indexCount` 定义索引区间；去掉 meshlet 字段，meshlet 属后续 mesh shader change）。
- **BlendShape**：对齐 Unity `AddBlendShapeFrame` / glTF morph target 的 delta 增量模型；`positionDeltas` 必需、`normal/tangentDeltas` 可空（缺省按零增量）。
- **Mesh 用 `CounterPtr<RenderGeometry>`**（可共享，geometry 可被多个 Mesh 复用）；`RefObject` 而非 `RenderResource`（Mesh 本身不持有底层 buffer，buffer 在 RenderGeometry 里）。`GetLocalBounds()` 委托 `geometry->GetLocalBounds()`（总包围盒在 RenderGeometry，submesh 包围盒在 `SubMesh.bounds`）。
- **理由**：Mesh = 几何 + 分段 + morph 的纯封装，与 `RenderGeometry`「纯封装」哲学一致；material 用索引占位。
- **备选**：`Mesh : RenderResource`（旧引擎做法）—— 被否，Mesh 不直接持有 buffer，`RenderGeometry` 已承担 resource 职责，再叠一层 RenderResource 会重复 Create/Release。

### D6: blend shape 只落数据结构，GPU 混合留后续

`BlendShape` 只存 CPU delta 数据（position/normal/tangent），不建 morph buffer、不改 shader。GPU morphing（`final = base + Σ weightᵢ·deltaᵢ` 在 vertex shader 用独立 morph buffer + `MorphWeights` uniform）依赖 skinning/渲染管线，属后续 change。

- **理由**：delta 增量数据结构是「接口」的一部分，现在落地；GPU 混合是「执行」，依赖尚未开的渲染管线，YAGNI 现在做。
- **备选**：现在就做 GPU morph buffer —— 被否，超出 mesh 接口范围，且 skinning 未开。

### D7: 蒙皮（skinning）——语义 + Skeleton + Mesh 挂接

蒙皮分三层落地：

1. **顶点语义扩展**（改 `aurora/rhi/VertexSemantic.h`）：加 `JOINTS`（4 骨骼索引，`uint16x4`）+ `WEIGHTS`（4 权重，`float4`），`kVertexSemanticCount` 13→15。对齐 glTF `JOINTS_0`/`WEIGHTS_0`。蒙皮顶点数据（joints/weights）就是带这两个语义的 vertex stream（SoA，落在 `RenderGeometry` 里），**不引入新类型**。

2. **`Skeleton` 资源**（`Mesh.h` 内，或独立 `Skeleton.h`）：

```cpp
struct Bone {
    Name    name;
    int32_t parent = -1;       // 父骨骼索引（-1 = root）
    Matrix4 inverseBind;       // 逆绑定矩阵（bind pose）
};

class Skeleton : public RefObject {
    // std::vector<Bone> bones;
    // const std::vector<Bone> &GetBones() const;
    // const Bone *GetBone(uint32_t index) const;
    // uint32_t GetBoneCount() const;
};
```

3. **`Mesh` 挂接**（可选骨架 + `HasSkin`）：

```cpp
class Mesh : public RefObject {
    void SetSkeleton(CounterPtr<Skeleton> skel);
    CounterPtr<Skeleton> GetSkeleton() const;
    bool HasSkin() const { return skeleton != nullptr; }
private:
    CounterPtr<Skeleton> skeleton;   // 共享，多个 Mesh 可复用同一骨架
};
```

- **理由**：蒙皮 = 顶点数据（JOINTS/WEIGHTS 语义，已在 vertex stream）+ 骨架（bone 层级 + 逆绑定矩阵）。骨架被多个 Mesh 共享用 `CounterPtr`；蒙皮顶点数据无需新类型，语义扩展即可。
- **GPU 蒙皮执行留后续**：每帧 joint matrix（`final = Σ weightᵢ·(jointᵢ·inverseBindᵢ)·base`）的 skinning buffer + vertex shader 属动画/渲染管线，本 change 只落「语义 + 骨架 + 引用」。
- **备选**：`SkeletonMesh : Mesh` 子类（旧引擎 `SkeletonMesh.h` 做法）—— 被否，`HasSkin()` 标志 + 可选 skeleton 更轻，避免「是否蒙皮 × 是否 LOD × 是否 meshlet」的类爆炸（维度正交用可选成员表达）。

## Risks / Trade-offs

- **[抽象接口过度设计风险]** `MeshVertexDataInterface` 多态接口在 v1（无 skinned mesh）可能过度。→ 缓解：旧引擎已有此先例，且「顶点结构 T 与字节缓冲解耦」是 mesh 构建的基础抽象，成本低。
- **[blend shape delta 与 base 顶点数一致性]** `positionDeltas.size()` 必须等于 base 顶点数，否则混合越界。→ 缓解：`AddBlendShape` 时校验（与 geometry 顶点数对齐由调用方保证），测试覆盖。
- **[materialIndex 未绑定真实材质]** 占位索引在材质系统落地前无实际语义。→ 缓解：Non-Goal 明示，后续 change 把 `uint32_t` 换成材质/technique 引用。
- **[tangent delta 维度]** Unity/glTF 的 tangent 是 vec3（w 位在 delta 里通常为 0）。→ 缓解：用 `Vector3`，与 Unity `deltaTangents` 一致；需要 vec4 时后续扩展。
- **[JOINTS/WEIGHTS 语义占用位]** `kVertexSemanticBits=16` 已用 13 位，加 2 位到 15，仍在 16 位预算内。→ 缓解：不超预算；若未来语义超 16 位需改 variant 布局（另开 change）。
- **[骨架生命周期]** `Skeleton` 被 Mesh 共享（`CounterPtr`），骨架释放时 Mesh 的 `HasSkin()` 失效。→ 缓解：`CounterPtr` 保活，Mesh 存活期间骨架不释放。

## Migration Plan

1. `VertexSemantic.h`：加 `JOINTS`/`WEIGHTS` 语义。
2. `MeshData.h` 落地（接口 + 模板 + 索引数据）。
3. `Mesh.h` 落地（SubMesh/BlendShape/Skeleton/Mesh）。
4. `MeshResourceTest.cpp`：数据接口 + Mesh 结构 + RenderGeometry 挂接 + 蒙皮挂接。
5. archive。

## Open Questions

- blend shape GPU morphing 与 GPU 蒙皮的执行（morph/skinning buffer + shader）何时落地——依赖动画/渲染管线。
- `materialIndex` 何时换成真实材质/technique 引用——依赖材质系统。
- LOD/meshlet 的 mesh 扩展——后续 change。
- `Skeleton` 是否独立文件（`Skeleton.h`）还是放 `Mesh.h`——实现时按体量定。
