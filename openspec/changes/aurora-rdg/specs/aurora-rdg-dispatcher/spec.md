## ADDED Requirements

### Requirement: 单线程构建 + 批次提交

`FrameGraphDispatcher` SHALL 采用单线程构建、批次提交模型：`CreateTask` / `DependsOn` 仅在调用线程（构建期）执行，`Submit` 提交整批，批次内所有节点执行完毕后统一释放节点内存。

调用方 MUST 在 `Submit` 前完成全部 `CreateTask` / `DependsOn`；`Submit` 之后不得再增删节点。节点内存由批次整体持有，不按单节点引用计数。

#### Scenario: 构建后整体提交

- **WHEN** 调用方在构建期创建 N 个节点并建立依赖，然后 `Submit(pool)`
- **THEN** 整批节点被调度执行，批次 future 在所有节点完成后 ready

#### Scenario: 批次整体释放

- **WHEN** 批次所有节点执行完成
- **THEN** 节点内存随 `Clear` 或析构统一释放，节点间无逐节点引用计数

### Requirement: 节点 index 引用与连续存储

节点以 `NodeIndex`（`uint32_t`）标识，`children` 存子节点 index 而非指针。节点对象连续存储于批次持有的容器中，`Submit` 后该容器只读、不再重分配。

#### Scenario: children 用 index 引用

- **WHEN** 一个节点声明依赖多个父节点
- **THEN** 每个父节点的 `children` 存储子节点的 index，每边 4 字节

### Requirement: 无锁无引用计数调度

`DependsOn` SHALL 不持锁（单线程构建）；`Submit` 后子节点遍历为只读，无需锁。节点无引用计数成员，不执行 `AddRef` / `RemoveRef`。

#### Scenario: 构建与执行零锁零计数

- **WHEN** 构建期建立依赖、执行期父节点完成并调度子节点
- **THEN** 不涉及 `SpinLock`、`std::mutex`，也不发生引用计数增减

### Requirement: per-node 与批次级 future

`GetFuture(node)` SHALL 返回单节点完成 future，且仅在构建期调用（`Submit` 前）。`Submit(pool)` SHALL 返回批次级 future，信号所有节点完成。

#### Scenario: per-node future 构建期获取

- **WHEN** 构建期对某节点调用 `GetFuture(node)` 后在 `Submit` 后 `wait`
- **THEN** 该节点执行完成后 future ready

#### Scenario: 批次级 future 覆盖整体完成

- **WHEN** `Submit(pool)` 返回的 future 被 `wait`
- **THEN** 该 future 在所有节点执行完成后 ready

### Requirement: 并行执行依赖图

`FrameGraphDispatcher` SHALL 通过 `ThreadPool::Schedule` 将可执行节点提交到 worker 队列（round-robin + work-stealing），多 worker 并行执行无依赖的节点；父节点完成后递减子节点父计数，归零时子节点入队。

#### Scenario: 依赖顺序正确

- **WHEN** 一个线性链 / diamond / 多根依赖图被 `Submit` 并 `wait`
- **THEN** 每个节点仅在其所有父节点完成后执行一次，无丢失、无重复、无死锁
