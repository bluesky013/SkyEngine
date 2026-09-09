## 1. CompiledGraph.h 修改

- [x] 1.1 `CompiledGraph` 新增 `uint32_t finalBarrierOffset`（帧末 barrier 段起点）
- [x] 1.2 `ComputePayload` 新增 `std::function<void(ComputeEncoder &, RDGContext &)> executeFn`
- [x] 1.3 `CopyBlitPayload` 新增 `std::function<void(BlitEncoder &, RDGContext &)> executeFn`

## 2. Compile.cpp 适配

- [x] 2.1 `ProduceCompiledGraph`：append final barriers 前记录 `finalBarrierOffset`
- [x] 2.2 compute pass 的 `executeFn` 拷入 `ComputePayload`
- [x] 2.3 copyblit pass 的 `executeFn` 拷入 `CopyBlitPayload`

## 3. Execute.cpp 重构

- [x] 3.1 `ExecutePasses` 遍历 `mCompiledGraph->passes`（不触碰 setup graph）
- [x] 3.2 per-pass barriers 从 `barriers[cpass.barrierOffset .. +barrierCount)` emit
- [x] 3.3 final barriers 从 `barriers[finalBarrierOffset .. end)` emit
- [x] 3.4 `SCENE_RASTER`：`BeginRendering` + items 遍历（`BindResourceGroup(2)`/`BindPipeline`/`BindIndexBuffer`/`DrawIndexed`）+ `EndRendering`
- [x] 3.5 `FULLSCREEN`：`BeginRendering` + `BindResourceGroup(1)` + `BindPipeline(pso)` + `Draw(3,0,1,0)` + `EndRendering`
- [x] 3.6 `COMPUTE`：`BindPipeline(pso)` + `BindResourceGroup(1)` + executeFn fallback
- [x] 3.7 `COPYBLIT`：executeFn fallback
- [x] 3.8 `PRESENT`：无 encoder 操作
- [x] 3.9 `CUSTOM`：`fn(ctx, cmdBuf)`
- [x] 3.10 `RDGContext` 的 image/buffer table 从 `CompiledGraph.resolvedImages/resolvedBuffers` 取

## 4. RenderGraph.h/.cpp 修改

- [x] 4.1 `AddSceneRasterPass(name, setup)` 移除 execute 参数
- [x] 4.2 删除 `AddSceneRasterPass` 内 `(void)execute` 过渡代码

## 5. 测试

- [x] 5.1 `RDGTest.cpp`：`AddSceneRasterPass` 调用移除 execute lambda 参数
- [x] 5.2 断言 executor 数据源为 CompiledGraph（可用一个 culled pass 验证：culled pass 不在 `CompiledGraph.passes` 中）

## 6. 验证与收尾

- [x] 6.1 全量 `cmake --build` 通过
- [x] 6.2 `AuroraTest --gtest_filter=RDG*` 全绿
- [ ] 6.3 `openspec archive aurora-rdg-compiled-executor` 归档本 change
