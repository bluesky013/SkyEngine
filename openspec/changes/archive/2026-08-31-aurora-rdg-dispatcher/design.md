## Context

RDG Compile 需要把有依赖关系的 CPU 子任务（拓扑排序、barrier 推导、生命周期分析）并行化到 ThreadPool 的多 worker。约束：

- 构建期单线程（`CreateTask` / `DependsOn` 在调用线程）
- `Submit` 后依赖图冻结、执行期只读
- 节点内存随批次整体释放，不做逐节点引用计数

## Goals / Non-Goals

**Goals:**

- 单线程构建 + 批次提交，执行期无锁、无引用计数
- 节点紧凑（`NodeIndex` 引用 + 连续存储），热路径无可避免的间接访问
- 并行执行正确：每个节点仅在所有父节点完成后执行一次

**Non-Goals:**

- 不做拓扑排序（Compile 调用方自行决定子任务划分，Dispatcher 只负责依赖调度）
- 不替代 `ThreadPool::TaskNode` 的流式 API（并存）

## Decisions

### 决策 1：节点 index 引用 + vector 连续存储

节点用 `NodeIndex`（`uint32_t`）标识，`children` 存 index（4B/边），节点存于 `std::vector<Node>`。`Submit` 后 `mNodes` 只读不再重分配。

**Why:** index 比指针省一半（4B vs 8B）；`vector` 连续比 slab/deque 更 cache 友好。

### 决策 2：pendingParents 两阶段存储

构建期用 `std::vector<uint32_t> mPendingBuild`（单线程普通累加）；`Submit` 时一次性 `make_unique<std::atomic_uint32_t[]>` 冻结为连续原子数组供 worker 并发递减。

**Why:** `std::vector<std::atomic_uint32_t>` 因 atomic 不可移动而无法 realloc；deque 块链的 `operator[]` 在热路径 `fetch_sub` 上比连续数组慢（实测慢 ~40%）。两阶段存储消除间接访问。

### 决策 3：Submit 先收集根节点再入队

`Submit` 先遍历收集所有 `pendingParents == 0` 的根节点到一个 vector，再统一 `Schedule` 入队。

**Why:** 若边扫描边入队，worker 会并发 `fetch_sub` 尚未被扫描到的节点的 pending，把非根节点误判为根而重复执行（真实 bug，曾导致节点执行两次、`remaining` 下溢）。

### 决策 4：无锁、无引用计数、无 done 标志

`DependsOn` 单线程无锁；`children` 执行期只读无锁；节点无 `counter`/`AddRef`/`RemoveRef`。per-node `GetFuture` 仅构建期调用，promise 构建期创建，执行期只 `set_value` 已存在者，故无需 `done` 标志与同步。

### 决策 5：mPool 成员而非 lambda 捕获引用

`Submit` 时记录 `mPool = &pool`，入队 lambda 只捕获 `this + index`（约 12B），子节点调度经 `mPool->Schedule`。

**Why:** 减少 lambda 捕获体积（落在 `TaskFunc` 40B 内联内），避免每个 lambda 都捕获 pool 引用。

## Risks / Trade-offs

- **批次生命周期约束**：`Submit` 后、`future.wait` 前不得 `Clear`/析构（`~FrameGraphDispatcher` 用 `SKY_ASSERT(mRemaining == 0)` 兜底）
- **per-node future 仅构建期可用**：不支持"任务完成后才取 future"，该能力由批次级 future 覆盖
- **`GetFuture(node)` 后 `CreateTask` 触发 vector realloc**：future 已与 promise 解耦（`get_future` 独立于 promise 对象位置），realloc 不影响已取出的 future

## Migration Plan

1. `core/async/ThreadPool` 加 public `Schedule`
2. 实现 `FrameGraphDispatcher`
3. 测试 + benchmark（对比 `TaskNode`）
