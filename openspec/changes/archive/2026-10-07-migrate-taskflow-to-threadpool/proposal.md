# Change: migrate-taskflow-to-threadpool

## Why

core 里并存两套作业系统：

- **旧（taskflow）**：`core/async/NamedThread`（`tf::Executor`）、`core/async/Task` + `TaskExecutor`（`tf::Executor` / `tf::AsyncTask`）。使用方：`navigation`（`NavigationSystem`/`NaviMeshBuilder`/`TerrainNavRebuild`/`NaviMeshFactory`）、`plugins/recast`（`RecastNaviMeshGenerator`/`RecastTileGenerator`）、`plugins/vegetation`（`VegetationSystem`）。
- **新（自研）**：`core/async/ThreadPool` + `ThreadContext` + `TaskNode`（依赖图）。使用方：`framework/asset`、`aurora`（`ShaderCompileTask`、RHI `DeviceFrameContext`/`DeviceFrameDispatcher`）。

代价：① 多一份三方依赖 `taskflow`（`engine/core/CMakeLists.txt` 链 `3rdParty::taskflow`）；② 两套并发模型并存；③ 旧路径与 core `Semaphore` 耦合——macOS `Semaphore::Signal` 的忙等 bug 正是经 `tf::Executor` 析构暴露（测试 `AsyncTest.FrameTest` 挂起）。

新 `ThreadPool` 的 `TaskNode::DependsOn` 与 `tf::AsyncTask::dependencies` 语义一一对应，迁移路径清晰。

## What Changes

- **`core/async/Task`**：改由 `ThreadPool`/`TaskNode` 实现，去掉 `tf::*`：
  - `TaskHandle` = `TaskNodePtr`（替代 `tf::AsyncTask`）；`dependencies` 为 `std::vector<TaskHandle>`。
  - `StartAsync` 用 `ThreadPool::CreateTask` + `DependsOn` + `Submit`；`IsWorking`/`ResetTask` 保留语义。
  - `TaskExecutor` 内部改持 `ThreadPool`，`WaitForAll()` → `ThreadPool::WaitIdle()`；移除 `GetExecutor()`（提供 `GetPool()`）。
- **`core/async/NamedThread`**：改由 `ThreadPool(1)` 实现（`Dispatch`/`Sync`/`Signal` 语义不变），去掉 `tf::Executor`。
- **依赖清理（部分）**：core 源文件不再 `#include <taskflow/...>`，且 `engine/core/CMakeLists.txt` 不再链接 `3rdParty::taskflow`（链接已下移到仍使用 `tf::Executor` 的 legacy 目标 `engine/shader`、`engine/render/core`）。taskflow 包本身的移除随渲染重构一起进行，见 `openspec/changes/legacy-render-thirdparty-cleanup/`。
- **消费方**不改变调用面（仅用 core 的 `StartAsync`/`ResetTask`/`IsWorking`/`WaitForAll`/`GetTask`；`GetTask()` 返回类型改为 `TaskHandle`）。

## Capabilities

### Modified Capabilities

- `async-threadpool`: 新增「遗留 taskflow 作业系统迁移到 ThreadPool」要求——`Task`/`TaskExecutor`/`NamedThread` 基于 `ThreadPool` 实现，core 不依赖 taskflow，`TaskHandle` 即 `TaskNodePtr`。

## Impact

- `engine/core/async/{Task,NamedThread}.{h,cpp}`、`engine/core/CMakeLists.txt`、`cmake/thirdparty.{cmake,json}`
- 消费方（navigation / recast / vegetation）编译面不变（类型经 core 封装）
- 测试：`TaskTest`（`GetExecutor().wait_for_all()` → `WaitForAll()`）、`AsyncTest`（API 不变）
