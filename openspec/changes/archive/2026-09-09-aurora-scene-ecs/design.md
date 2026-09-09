## Context

现状：`RenderScene` = `std::vector<RenderPrimitive*>` + 每 primitive 一个 `unordered_map<Name, TechniqueBinding>`；收集是 `N × (指针解引用 + 哈希)`。framework `EntityManager` 是 stub 不可用。

用户已定方向：sparse-set + SoA，基础容器放 core；**Techniques 分桶设计推迟**（用户再考虑），但 `RenderPrimitive` 不保留——剥掉 technique map 后它只剩 geometry+bounds，等于组件本身，留着是多余包装。过渡方案：`RenderItem` 组件携带单个 `techniqueTag` + 完整 `DrawItem`，保住现有 queue 过滤语义，未来 technique 设计落地后替换。

## Goals / Non-Goals

**Goals:**

- core 提供 `EntityId`（index + generation）/ `SparseSet<T>` / `EntityRegistry`，通用可复用。
- `RenderScene` 改 ECS：entity 注册 + SoA 组件（`Bounds` / `RenderItem` / `Light` / `Skin`）。
- 删除 `RenderPrimitive` 独立结构；Collect 遍历 dense 数组。
- 过滤语义不退化：queue 的 techniqueTag 与 entity 的 `RenderItem.techniqueTag` 匹配才收集。

**Non-Goals:**

- 不做 archetype / chunk 式 ECS（sparse-set 足够 render scene 规模）。
- 不做 system 调度 / job 并行（systems 先为自由函数，job 化留后续）。
- 不做 technique 分桶/多 binding 设计（`RenderItem` 为过渡形态，后续 technique change 替换）。
- 不改 framework 的 `EntityManager`（不同模块的 game-object 层，独立演进）。
- 不做光照/蒙皮的功能实现（组件结构先落地，功能后续）。

## Decisions

### 1. SparseSet 结构

```cpp
EntityId = uint32_t { 24 bit index | 8 bit generation }

SparseSet<T>:
  sparse[] : uint32_t (id.index → dense index, EMPTY = invalid)
  dense[]  : EntityId          // dense index → entity（swap-remove 时回写 sparse）
  data[]   : T                 // 与 dense[] 对齐

  Add(id, T)      — sparse[id.index] = dense.size(); dense/data push_back
  Remove(id)      — swap-remove：把末尾 entity 换到被删槽位，回写 sparse
  Get(id)         — generation 校验后返回 data[sparse[id.index]]
  迭代             — data[] 连续扫，dense[] 同步可取 entity
```

generation 作用：id 被复用后，持旧 generation 的引用 `Get` 失败（防悬垂）。

### 2. RenderItem 过渡组件

```cpp
struct RenderItem {
    Name     techniqueTag;   // 与 queue.techniqueTag 匹配；空 = 仅被空 tag queue 收集
    DrawItem item;           // pso / batchResourceGroup / vb / ib / offsets / args
};
```

每 entity 一份（当前语义等价于 RenderPrimitive 单 binding）；未来 technique 设计（多 binding / 分桶 / 材质系统）落地后此组件被替换，queue 过滤接口保持不变。

### 3. EntityRegistry 管理多组件池

```cpp
EntityRegistry:
  CreateEntity() → EntityId   // free list + generation bump
  DestroyEntity(id)
  template<typename T> SparseSet<T> &Pool()   // 按 type id 懒建池
  template<typename T> T *Get(EntityId)
  template<typename T> void Add(EntityId, T)
```

组件 type id：静态自增 counter（`TypeId<T>()`），小整数作池索引。

### 4. Collect 适配

```cpp
auto &boundsPool = scene.Pool<Bounds>();
auto &itemPool   = scene.Pool<RenderItem>();

for (dense i in boundsPool):            // 连续扫
    entity = boundsPool.DenseEntity(i)
    if (!view->FrustumCulling(boundsPool[i])) continue;
    auto *ri = itemPool.Get(entity);    // sparse 索引，无哈希
    if (!ri) continue;
    if (queue.tag 非空 && ri->techniqueTag != queue.tag) continue;
    builder.AddDrawItem(queue, ri->item);
```

空 tag queue 语义与现状一致：收集所有（不过滤）。

### 5. 占位组件结构

```cpp
struct Light {
    LightType type = LightType::DIRECTIONAL;
    Vector3   color{1.f, 1.f, 1.f};
    Vector3   direction{0.f, -1.f, 0.f};
    float     intensity = 1.f;
};

struct Skin {
    uint32_t jointCount = 0;
    // skin data 引用，蒙皮管线接入后填充
};
```

## Risks / Trade-offs

- **[多池查询回 sparse 跳转]** Collect 每实体除 bounds 连续扫外，还要 `itemPool.Get(entity)`（sparse 跳一次）。比现状每实体哈希查找仍好（int 索引 vs Name 哈希）。→ v2 可按 archetype 分桶或收集侧缓存。
- **[swap-remove 打乱 dense 顺序]** dense 顺序无语义（收集侧自行排序），可接受。
- **[RenderItem 是过渡形态]** 未来 technique 设计落地后组件结构变更。→ queue 过滤接口（techniqueTag 匹配）保持稳定，替换只在组件侧。
- **[Light/Skin 组件先落地但无功能]** 空结构。→ 明确标注占位，光照/蒙皮 change 填充。

## Migration Plan

1. core：`EntityId` / `SparseSet<T>` / `EntityRegistry` + 单元测试。
2. scene：`SceneTypes.h`（Bounds/RenderItem/Light/Skin）+ `RenderScene` 改 ECS；删除 `RenderPrimitive.h` + `src/scene/RenderPrimitive.cpp`；views 保留原 registry 不变。
3. `SceneRasterPassTemplate::Collect` 适配 + `OpaquePass` 不变（走模板）。
4. 测试适配（SceneCollectTest mock 改 `CreateEntity + Add<T>`，断言行为不变）+ core ECS 测试。
5. archive。后续 change：technique 设计 → RenderItem 组件替换。
