# Design: migrate-taskflow-to-threadpool

## 映射关系

| taskflow | ThreadPool |
|---|---|
| `tf::Executor executor(N)` | `ThreadPool pool(N)` |
| `executor.dependent_async(fn, deps...)` | `pool.CreateTask(fn)` → 对每个 dep `node->DependsOn(dep)` → `pool.Submit(node)` |
| `tf::AsyncTask`（依赖句柄） | `TaskNodePtr`（`CounterPtr<TaskNode>`），别名 `TaskHandle` |
| `executor.wait_for_all()` | `pool.WaitIdle()` |

## Task 改造

```cpp
class Task : public RefObject {
    void StartAsync();          // CreateTask + DependsOn(deps) + Submit
    bool IsWorking() const;     // handle != nullptr && !isDone
    void ResetTask();           // handle = nullptr; isDone = false; deps.clear()
    TaskHandle GetTask() const { return handle; }
protected:
    virtual bool DoWork() = 0;
    virtual void PrepareWork() {}
    virtual void OnComplete(bool) {}
    std::atomic_bool            isDone{false};
    TaskNodePtr                 handle;
    std::vector<TaskHandle>     dependencies;
};
```

`StartAsync` 里 lambda 捕获 `CounterPtr<Task>`（保活），执行 `DoWork → OnComplete → isDone=true`。依赖边在建好后再 `Submit`（`TaskNode` 语义：父完成后 `pendingParents==0` 才入队）。

`TaskExecutor`（Singleton）内部持 `std::unique_ptr<ThreadPool>`，默认线程数 `max(1, hardware_concurrency())`；`WaitForAll()→WaitIdle()`，暴露 `GetPool()`。删除 `GetExecutor()`。

## NamedThread 改造

`ThreadPool(1)` 承载 dispatch；`Sync()`=`semaphore.Wait()`，`Signal()`=投递一个 `semaphore.Signal()` 到该线程（保持原语义：主线程 `Sync` 等该线程执行到 `Signal` 点）。构造时投递 `SetCurrentThreadName(name)`。

## 依赖清理顺序

1. 先改 `Task`/`NamedThread`（core 内不再 `#include <taskflow/...>`）。
2. 构建 core + 测试，确认无 tf 引用。
3. 移除 `engine/core/CMakeLists.txt` 的 `3rdParty::taskflow`、`cmake/thirdparty.cmake` 的 `sky_find_3rd(taskflow)`、`cmake/thirdparty.json` 的 taskflow 条目。
4. 消费方（navigation/recast/vegetation）无需改动（仅用 core API）。

## 风险

- `TaskNode` 生命周期由 `ThreadPool` arena + `CounterPtr` 管理；`Task::handle` 持引用至 `ResetTask`。需确认 `GetFuture()` 未被误用（nav 只用 `WaitForAll`）。
- `NamedThread` 单线程池的 FIFO 语义需与 taskflow 一致（`Sync/Signal` 配对）。
