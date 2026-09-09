## Context

`pipeline-pass-template` 完成 PipelinePass 基类 + OpaquePass 骨架（`Collect` 空钩子）。旧 render/core 的 `RenderSceneVisitor` 是成熟参考：queue `rasterID` 作 technique tag 过滤，primitive 按 tag 持有 technique，`GatherRenderItem` 命中才 append；culling 分两遍（cull → dispatch）。

## Goals / Non-Goals

**Goals:**

- aurora/pipeline 内新建 scene 抽象：`SceneView`（frustum + view 常量）、`RenderPrimitive`（geometry + worldBounds + tag→TechniqueBinding 映射）、`RenderScene`（primitives + views 管理）。
- `SceneRasterQueue::techniqueTag`（RDG 纯数据字段，收集方用作过滤器）。
- `SceneRasterPassTemplate::Collect` 默认实现：frustum cull → tag 过滤 → AddDrawItem → 按 sortPolicy 排序。
- OpaquePass 接入：main view + queue tag "opaque" + FRONT_TO_BACK。

**Non-Goals:**

- 不做遮挡剔除系统（IRenderSceneCulling 类，v2）。
- 不做 ShadowPass 实现（tag 机制验证后，ShadowPass 是另一个 pass 模板实例，后续）。
- 不做 view 常量 → global RG 的上传（set 0 绑定留 resource-group 落地后）。
- 不做 primitive 的 LOD / skinning / instance 合并。
- 不复用旧 render/core 类型。

## Decisions

### 1. technique tag 挂 queue

`SceneRasterQueue::techniqueTag`（`Name`）。同一 SceneRasterPass 内不同 queue 可挂不同 tag（opaque queue + transparent queue 共存于一个 pass）。RDG 只存储，不消费；收集方（pass 模板 Collect）读 tag 作过滤。与旧设计（`RasterQueue::rasterID`）语义一致。

### 2. primitive 按 tag 持有 TechniqueBinding

```cpp
struct TechniqueBinding {
    GraphicsPipeline *pso = nullptr;
    ResourceGroup    *batchResourceGroup = nullptr; // set 2
};

struct RenderPrimitive {
    Buffer *vb; Buffer *ib; uint32_t vbOffset, ibOffset;
    CmdDrawIndexed args;
    AABB worldBounds;
    TransientHashMap<Name, TechniqueBinding> techniques; // key = tag
    void GatherRenderItem(const GatherContext &ctx); // ctx.tag 命中才 append
};
```

`GatherRenderItem` 是纯 CPU 遍历：`techniques.find(ctx.tag)` 命中 → append `DrawItem{pso, batchRG, vb, ib, args}`。

### 3. 收集在 BuildRDG（setup 期）

`SceneRasterPassTemplate::BuildRDG` 内调 `Collect(builder)`：此时 setup graph 尚未 Compile，items 直接写入 pass data。RDG 层永远不知道 scene 存在——收集完成后 RDG 只看到扁平 items。这是既定约束（RDG 纯数据驱动）的直接推论。

### 4. v1 culling 只做 frustum

`SceneView::FrustumCulling(const AABB&)`。pass 模板持有一个 `SceneView*`（OpaquePass = main view；未来 ShadowPass = light view），Collect 时对每个 primitive 做 frustum 测试。遮挡剔除系统（旧 `IRenderSceneCulling`）留 v2。

### 5. 排序由 Collect 完成

收集完 queue items 后，按 `queue.sortPolicy` 排序：`FRONT_TO_BACK` = view 空间深度升序；`BACK_TO_FRONT` 降序；`NONE` 不排。需要 view 矩阵把 worldBounds 中心变换到 view 空间。RDG 不参与排序。

### 6. SceneView v1 最小面

frustum（6 plane）+ view 矩阵 + viewProject 矩阵。常量上传（set 0 global RG）留 resource-group 落地后，v1 SceneView 只为 culling + 排序服务。

## Risks / Trade-offs

- **[Collect 每帧 CPU 遍历全部 primitives]** O(N×queues)。v1 可接受；v2 引入 visible set 缓存 + 增量更新。→ 缓解：frustum cull 先过滤；后续 scene 侧维护 dirty 标记。
- **[tag 空字符串语义]** 默认 queue（tag 为空）收集所有 primitive 还是收集「tag 也为空的 primitive」？→ 决策：空 tag = 收集所有（不过滤），保持默认 queue 兼容行为。
- **[TransientHashMap 在 primitive 上的开销]** primitive 是持久对象，不应绑帧 arena。→ techniques 用 `std::unordered_map`（持久堆），不用 Transient 容器。

## Migration Plan

1. RDG：`SceneRasterQueue::techniqueTag` + builder AddQueue 三参重载。
2. scene 抽象：`SceneView.h` / `RenderPrimitive.h` / `RenderScene.h`（aurora/pipeline/scene/）。
3. `SceneRasterPassTemplate`：`SetScene/SetView` + `Collect` 默认实现（cull + gather + sort）。
4. `OpaquePass`：接 scene/view，queue tag "opaque"。
5. 测试：mock scene（多 tag primitives + view）→ 断言 tag 过滤、frustum 剔除、排序正确性。
6. archive。
