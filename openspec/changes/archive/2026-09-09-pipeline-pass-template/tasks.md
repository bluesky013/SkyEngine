## 1. RDG queue 结构（aurora 层）

- [x] 1.1 `CompiledGraph.h`：新增 `QueueSortPolicy` 枚举（`NONE / FRONT_TO_BACK / BACK_TO_FRONT`）；新增 `SceneRasterQueue`（`name` / `items` / `queueResourceGroup` / `sortPolicy`）；`SceneRasterPayload::items` 改为 `queues: TransientVector<SceneRasterQueue>`
- [x] 1.2 `RDGGraph.h`：`SceneRasterPassData::items` 改为 `queues: TransientVector<SceneRasterQueue>`
- [x] 1.3 `RenderGraph.h/.cpp`：新增 `AddQueue(uint32_t passIndex, const Name&, QueueSortPolicy) -> uint32_t` / `AddDrawItem(uint32_t passIndex, uint32_t queue, const DrawItem&)` / `SetQueueResourceGroup(uint32_t passIndex, uint32_t queue, ResourceGroup*)`；无 queue 的 `AddDrawItem(passIndex, item)` 路由到默认 queue 0（懒创建，name="default"）
- [x] 1.4 `RenderGraphBuilder.h` + builder 实现：`AddQueue(const Name&, QueueSortPolicy) -> uint32_t` / `AddDrawItem(uint32_t queue, const DrawItem&)` / `SetQueueResourceGroup(uint32_t queue, ResourceGroup*)`；保留无 queue 参数的 `AddDrawItem(item)` 兼容重载
- [x] 1.5 `Compile.cpp`：`ProduceCompiledGraph` 拷贝 queues 到 `SceneRasterPayload`
- [x] 1.6 `Execute.cpp`：SCENE_RASTER 按声明序遍历 queue：绑定 queue RG（set 1，无则 pass RG）→ 遍历 items（set 2 batch RG + draw）

## 2. pipeline 模块骨架（engine/aurora/pipeline/）

- [x] 2.1 `engine/aurora/pipeline/CMakeLists.txt`：target `Aurora.Pipeline`（STATIC），链接 `Aurora.RHI` + `Core`
- [x] 2.2 `engine/aurora/CMakeLists.txt` 加 `add_subdirectory(pipeline)`
- [x] 2.3 `include/aurora/pipeline/PipelinePass.h`：`PipelinePass` 基类（`OnSetup(Device*)` / `BuildRDG(RenderGraph&)` / `OnSceneChanged()`；持有持久 PSO/RG 成员）
- [x] 2.4 `include/aurora/pipeline/SceneRasterPassTemplate.h`：继承 PipelinePass，queue 声明辅助（`AddQueue(name, sortPolicy)`），`Collect` 空钩子

## 3. OpaquePass

- [x] 3.1 `include/aurora/pipeline/OpaquePass.h` + `src/OpaquePass.cpp`：继承 SceneRasterPassTemplate；BuildRDG 声明 color+depth attachment + "opaque" queue（FRONT_TO_BACK）
- [x] 3.2 `OnSetup` 占位（PSO 创建等 shader 管线接入后补）；`OnSceneChanged` 占位

## 4. 测试

- [x] 4.1 `RDGTest.cpp`：新增多 queue 保序测试（两个 queue 各自 items，断言 CompiledGraph 中 queue 顺序与 items 归属）
- [x] 4.2 `RDGTest.cpp`：默认 queue 兼容测试（无 AddQueue 直接 AddDrawItem，断言进入 queue 0）
- [x] 4.3 OpaquePass smoke test：BuildRDG 后 graph 中存在 SceneRasterPass 且 queues 含 "opaque"（放 AuroraTest，链接 RenderPipeline）

## 5. 验证与收尾

- [x] 5.1 全量 `cmake --build` 通过
- [x] 5.2 `AuroraTest` 全绿
- [ ] 5.3 `openspec archive pipeline-pass-template` 归档本 change
