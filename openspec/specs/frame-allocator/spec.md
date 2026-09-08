# frame-allocator Specification

## Purpose
TBD - created by archiving change core-frame-allocator. Update Purpose after archive.
## Requirements
### Requirement: FrameAllocator 提供帧作用域 bump 分配

`FrameAllocator` SHALL 提供非虚 bump 分配接口，基于 `LinearStorage` 块链管理。分配操作 O(1)，无每对象元数据。

- `Allocate(size, alignment) -> void*` — 从当前 block bump 分配；当前 block 不足时自动扩展新 block
- `AllocateArray<T>(n) -> T*` — 分配 `n * sizeof(T)` 对齐到 `alignof(T)`
- `Construct<T>(args...) -> T*` — bump 分配后 placement-new 构造
- `Arena() -> TransientAllocator&` — 返回内部 `TransientAllocator` 视图，供 `TransientStdAllocator<T>` / `MakeTransientVector` 等使用
- `GetMark() / Rewind(Mark)` — 检查点/回退（见 Requirement: 检查点与回退）
- `Reset()` — 帧末整体回收所有 block

`FrameAllocator` 为单线程使用。统计接口见 Requirement: 内存统计。

#### Scenario: bump 分配连续地址
- **WHEN** 依次 `Allocate(16)` 和 `Allocate(32)`
- **THEN** 两次返回地址在同一 block 内连续（或跨 block 扩展），无 free list 查找

#### Scenario: Construct 构造对象
- **WHEN** `Construct<std::pair<int, float>>(42, 3.14f)`
- **THEN** 返回构造完成的对象指针，对象位于 bump arena 内

### Requirement: 检查点与回退（Mark/Rewind）

`FrameAllocator` SHALL 支持 `GetMark()` 记录当前分配位置，`Rewind(Mark)` 回退到该位置。回退后 mark 之后分配的所有内存被逻辑回收，后续分配可复用空间。

- `Mark` 记录当前 block 索引 + block 内 offset
- `Rewind` 丢弃 mark 之后的 block，回退 mark 所在 block 的 offset
- `Rewind` 后 `mCurrentBytes` 相应回退

#### Scenario: setup 图编译后回收
- **WHEN** `Mark m = alloc.GetMark()`；建图/编译分配大量内存；`alloc.Rewind(m)`
- **THEN** mark 之后分配的空间被回收，后续 compiled graph 分配复用该空间

#### Scenario: 跨 block 回退
- **WHEN** 分配跨越多个 block 后 `Rewind` 到较早 mark
- **THEN** mark 之后的 block 被丢弃，mark 所在 block 的 offset 回退，无内存泄漏

### Requirement: 标准容器适配

`FrameAllocator` SHALL 通过 `Arena()` 提供 `TransientAllocator&`，配合 `TransientStdAllocator<T>` 使标准容器（vector/list/map/string）从 bump arena 分配。

- `TransientVector<T>` 别名 SHALL 存在
- `MakeTransientVector<T>(alloc)` 工厂 SHALL 存在

#### Scenario: TransientVector 从 arena 分配
- **WHEN** `auto vec = MakeTransientVector<int>(alloc); vec.emplace_back(1);`
- **THEN** vector 内部 buffer 从 bump arena 分配，`alloc.GetUsedBytes()` 相应增加

### Requirement: 内存统计

`FrameAllocator` SHALL 维护原子计数统计，零 per-allocation 元数据。

- `GetCurrentBytes() -> size_t` — 当前已分配字节数
- `GetPeakBytes() -> size_t` — 历史峰值字节数
- `GetAllocationCount() -> size_t` — 当前累计分配次数

`Allocate` 更新 `mCurrentBytes`/`mPeakBytes`/`mAllocationCount`；`Rewind` 回退 `mCurrentBytes`；`Reset` 清零 `mCurrentBytes` 并回退 `mPeakBytes`。

#### Scenario: 统计正确反映分配
- **WHEN** 连续 `Allocate(100)` 三次后 `GetCurrentBytes() == 300`；`Rewind` 回退一次后 `GetCurrentBytes() == 200`；`Reset` 后 `GetCurrentBytes() == 0`
- **THEN** 统计值与预期一致

