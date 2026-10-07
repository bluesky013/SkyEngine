# Delta: async-threadpool

## ADDED Requirements

### Requirement: 遗留 taskflow 作业系统迁移到 ThreadPool

`Task` / `TaskExecutor` / `NamedThread` SHALL 基于引擎自研 `ThreadPool`（+ `TaskNode`）实现，core SHALL NOT 依赖 `taskflow`。`Task` 的依赖句柄 SHALL 为 `TaskNodePtr`（别名 `TaskHandle`），`StartAsync` SHALL 经 `CreateTask` + `DependsOn` + `Submit` 表达依赖图；`TaskExecutor::WaitForAll` SHALL 委托 `ThreadPool::WaitIdle`。

#### Scenario: Task 依赖经 ThreadPool 表达

- **WHEN** 一个 `Task` 声明对其它 `Task` 的依赖并 `StartAsync`
- **THEN** 其依赖句柄为 `TaskNodePtr`，依赖边经 `TaskNode::DependsOn` 建立，父任务完成后本任务才执行

#### Scenario: core 源文件不依赖 taskflow

- **WHEN** 编译 core 的 `Task` / `NamedThread`
- **THEN** 其头/源不 `#include <taskflow/...>`，实现基于 `ThreadPool`（taskflow 依赖在 legacy shader/render 迁移完成前仍由 core 目标传递链接保留）

#### Scenario: NamedThread 基于单线程池

- **WHEN** 构造 `NamedThread` 并 `Dispatch`/`Sync`/`Signal`
- **THEN** 行为与迁移前一致（`Sync` 等待该线程执行到 `Signal`），实现基于 `ThreadPool(1)`
