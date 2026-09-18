## Context

aurora 已有：
- `RenderGeometry`（`aurora/resource/RenderGeometry.h`）：`RefObject`，持有 `std::vector<std::unique_ptr<VertexBuffer>>` + `std::unique_ptr<IndexBuffer>` + `AABB`，纯封装。
- `VertexBuffer`（带 `VertexLayout{stride, inputRate, VertexSemanticMask}`）+ `IndexBuffer`（带 `IndexType`）。
- `MeshData.h`（`MeshVertexDataInterface`/`TRawMeshVertexData<T>`/`RawMeshIndexData`）。
- `Mesh.h`（`SubMesh`/`BlendShape`/`Skeleton`/`Mesh`）。
- `VertexSemantic`：POSITION/NORMAL/TANGENT/UV1…（JOINTS/WEIGHTS 已加）。

`core/math` 有 Vector2/Vector3/Vector4/Matrix4 等，无几何体生成器。

需求边界（用户指定）：
- **算法放 `core/math`**：参数化曲面生成（顶点位置/法线/切线/uv/索引的计算）是纯数学，不依赖 aurora/rhi。
- **geometry 装配放 aurora**：把 math 产出的数据流组装成 `RenderGeometry`（建 VertexBuffer/IndexBuffer、Upload）。
- **SoA 分离 stream**：position 独立成一个 stream；基础属性带 uv / normal / tangent。

约束：`namespace sky`（core/math）与 `namespace sky::aurora`（资源层）；header-only；命名遵守 coding rules。

## Goals / Non-Goals

**Goals:**

- `core/math/GeometryGenerator.h`：6 个图元（Cube/Plane/Sphere/Cylinder/Cone/Capsule）生成 `GeometryStreams`（SoA）。
- `aurora/resource/BuiltinGeometry.h`：`BuiltinGeometry::Build` 把 `GeometryStreams` 装配成 `RenderGeometry`（4 分离 stream + index + bounds）。
- 测试：图元数据合法性（计数/法线单位/tangent 正交/uv 范围）+ `Build` 后的 stream 语义/bounds。

**Non-Goals:**

- 不做 LOD / meshlet / icosphere / 细分曲面。
- 不做蒙皮 builtin 几何。
- 不做顶点去重 / 索引缓存优化（参数化布局直接输出，允许重复顶点）。

## Decisions

### D1: 分层（算法 math / 装配 aurora）

```cpp
// core/math/GeometryGenerator.h  —— namespace sky，纯数学，不 include aurora/rhi
struct GeometryStreams {
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector4> tangents;   // xyz = tangent, w = handedness (±1)
    std::vector<Vector2> uvs;
    std::vector<uint32_t> indices;
};
GeometryStreams GenerateCube(float size);
GeometryStreams GeneratePlane(float width, float depth, uint32_t segX, uint32_t segZ);
GeometryStreams GenerateSphere(float radius, uint32_t rings, uint32_t sectors);
GeometryStreams GenerateCylinder(float radiusTop, float radiusBottom, float height, uint32_t sectors);
GeometryStreams GenerateCone(float radius, float height, uint32_t sectors);
GeometryStreams GenerateCapsule(float radius, float height, uint32_t rings, uint32_t sectors);
```

- **理由**：参数化曲面生成（顶点/法线/切线/uv/索引的数学）是纯数学，放 `core/math` 可被离线工具/其它模块复用，且不引入 aurora/rhi 依赖；装配（建 GPU buffer、Upload）才是 aurora 资源层的职责。
- **备选**：全放 aurora —— 被否，算法与 GPU 资源耦合，且用户明确要求「算法放 math」。

### D2: SoA 数据模型（独立数组，position 独立 stream）

`GeometryStreams` 用独立数组（SoA），对应 aurora 的分离 vertex stream。**position 独立成一个 stream**（用户要求），normal/tangent/uv 各一个 stream。

- **理由**：aurora 的 `RenderGeometry` 本就是「多分离 `VertexBuffer`」模型，SoA 直接映射，无需 interleave 打包；独立 position stream 便于未来做 position-only pass（depth/prepass）复用。
- **备选**：interleaved 单 buffer —— 被否，与 aurora 分离 stream 模型不符。

### D3: 属性集（position + normal + tangent + uv）

基础属性 = `position`(Vector3) + `normal`(Vector3) + `tangent`(Vector4，xyz 对齐 UV U 方向，w=handedness) + `uv`(Vector2)。

- **tangent 用 Vector4**：w 存 handedness（±1），对齐 Unity/glTF 惯例，供 normal mapping；各图元按参数化布局解析计算（sphere/cylinder/capsule 沿经度方向，cube 沿面 U 轴，plane 沿 +X）。
- **理由**：用户要求「基础带 uv、normal、tangent」；Vector4 tangent 是 normal mapping 的标准。
- **备选**：tangent 用 Vector3 —— 被否，缺 handedness 位，normal mapping 需额外标志。

### D4: 图元清单

`GenerateCube` / `GeneratePlane` / `GenerateSphere`(UV sphere) / `GenerateCylinder`(上下半径可不同) / `GenerateCone`(cylinder 特例) / `GenerateCapsule`(圆柱 + 两个半球)。

- **理由**：覆盖常用测试/调试图元；胶囊体是用户点名。UV sphere 简单，icosphere 留后续。
- **备选**：加 torus/icosphere —— 被否，v1 先做最小常用集，后续按需加。

### D5: BuiltinGeometry::Build 装配流程

```cpp
// aurora/resource/BuiltinGeometry.h  —— namespace sky::aurora
class BuiltinGeometry {
public:
    static CounterPtr<RenderGeometry> Build(
        Device *device, const sky::GeometryStreams &streams, IndexType indexType = IndexType::U32);
};
```

`Build` 流程：
1. 建 4 个分离 `VertexBuffer`（`VertexLayout.stride` = 对应类型 sizeof，`semantics` 设 POSITION/NORMAL/TANGENT/UV1），`Init(device, size)` + `Upload(data, size)`。
2. 建 `IndexBuffer`（`SetIndexType`），`Upload` 索引。
3. 组装进 `RenderGeometry`（`AddVertexStream` × 4 + `SetIndexBuffer`）。
4. `SetLocalBounds`（由 positions 算 min/max）。
5. 返回 `CounterPtr<RenderGeometry>`。

- **索引 U32 默认，U16 可选**：math 产出 uint32 索引；`indexType == U16` 时降级（要求 `vertexCount < 65536`，否则返回 null）。
- **理由**：装配职责单一（建 buffer + Upload + 组装），几何数据来源（math / 文件 / 程序生成）解耦。

### D6: 拓扑与朝向

三角形列表（triangle list），CCW 正面朝外；索引 uint32。

- **理由**：与 aurora `DrawItem::CmdDrawIndexed` / `PrimitiveTopology::TRIANGLE_LIST` 默认一致。
- **备选**：triangle strip —— 被否，通用性差，v1 用 triangle list。

## Risks / Trade-offs

- **[tangent 解析计算复杂]** 各图元 tangent 需按 UV 参数化方向解析求导，实现易错。→ 缓解：测试断言 `dot(tangent, normal) ≈ 0`、`|tangent.xyz| ≈ 1`、`w ∈ {±1}`；sphere/cylinder/capsule 沿经度、cube/plane 沿面轴。
- **[允许重复顶点]** 图元顶点不跨面共享（cube 24 顶点、sphere (rings+1)×(sectors+1)），索引无去重。→ 缓解：Non-Goal 明示；图元尺寸小，重复顶点可接受。
- **[U16 降级越界]** 顶点数 ≥ 65536 时不能 U16。→ 缓解：`Build` 校验并返回 null；默认 U32。
- **[tangent 流与 shader 语义对齐]** `TANGENT` 语义 + Vector4 格式需与 shader 顶点输入一致。→ 缓解：`VertexSemantic::TANGENT` 已存在，格式由 `VertexLayout.stride`（sizeof(Vector4)）表达。

## Migration Plan

1. `core/math/GeometryGenerator.h` 落地（`GeometryStreams` + 6 个生成函数）。
2. `aurora/resource/BuiltinGeometry.h` 落地（`Build` 装配）。
3. `core/test/GeometryGeneratorTest.cpp`（图元数据校验）。
4. `aurora/core/test/BuiltinGeometryTest.cpp`（Build 后 stream 语义/bounds）。
5. archive。

## Open Questions

- 是否后续加 torus / icosphere / 细分曲面。
- 是否后续做顶点去重 / 索引缓存（大图元内存优化）。
- tangent 是否需要针对 normal-map 切线空间做额外处理（当前按 UV 参数化解析计算）。
