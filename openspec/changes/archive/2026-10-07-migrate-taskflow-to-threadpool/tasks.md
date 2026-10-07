# Tasks: migrate-taskflow-to-threadpool

## 1. core

- [ ] 1.1 `core/async/Task.h/.cpp` 基于 `ThreadPool`/`TaskNode` 重写（`TaskHandle=TaskNodePtr`；`StartAsync`/`IsWorking`/`ResetTask` 语义保持；`TaskExecutor` 持 `ThreadPool`，`WaitForAll→WaitIdle`）
- [ ] 1.2 `core/async/NamedThread.h/.cpp` 基于 `ThreadPool(1)` 重写（去掉 `tf::Executor`）
- [ ] 1.3 core 内确认无 `taskflow`/`tf::` 引用

## 2. 依赖清理

- [x] 2.1 从 `engine/core/CMakeLists.txt` 移除 `3rdParty::taskflow`；将链接下移到实际使用方 `engine/shader`（`ShaderFileSystem`）与 `engine/render/core`（`RenderGraphContext`）
- [ ] 2.2（deferred）legacy `engine/shader`、`engine/render/core` 迁出 `tf::Executor` 后，移除 `3rdParty::taskflow` 链接与 `cmake/thirdparty.{cmake,json}` 的 taskflow 条目

## 3. 测试

- [ ] 3.1 `engine/test/core/TaskTest.cpp`：`GetExecutor().wait_for_all()` → `WaitForAll()`
- [ ] 3.2 `AsyncTest` 通过（NamedThread 无回归）
- [ ] 3.3 构建 + 运行 navigation / recast / vegetation 相关测试（`NavigationTest`/`NaviMesh*`/`Recast*`/`Vegetation*`）

## 4. 验证

- [ ] 4.1 全量构建（含 plugins）通过且不链接 taskflow
- [ ] 4.2 `CoreTest` 通过（含 `TaskTest`/`AsyncTest`）
