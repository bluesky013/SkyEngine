## Why

SkyEngine 目前引擎级内存策略碎片化：旧 render RDG 用 `PmrVector`/`PmrUnSyncPoolRes`（PMR 虚调用 + 类型污染），aurora RDG 用默认 `std::allocator`（逐对象 malloc/free），两者都导致每帧建图/编译期成千上万次堆分配。后续 RDG 数据驱动重构（compiled graph + render item + barrier 段）需要一个统一的、帧作用域的、零 malloc 的 bump allocator 地基。

代码库已有 `TransientAllocator`（基于 `LinearStorage` 的 bump allocator + `TransientStdAllocator<T>` 适配 std 容器）并有 12 个测试，但缺关键能力：无 `Mark`/`Rewind` 检查点、无 `TransientVector` 别名、无统一的 `FrameAllocator` 门面、无内存统计。

## What Changes

- **core 层**：
  - `LinearStorage` 新增 `Mark`/`Rewind`/`GetMark` 检查点 API（支持跨 block 回退）。
  - `TransientAllocator` 透传 `Mark`/`Rewind`/`GetMark`。
  - 补 `TransientVector<T>` 别名与 `MakeTransientVector` 工厂。
  - 新增 `FrameAllocator`（core/memory/FrameAllocator.h）：非虚 bump + 检查点 + 标准容器适配入口 + 仅 bytes/peak 统计。
- **统计**：`Allocate`/`Rewind`/`Reset` 内原子累加 `mCurrentBytes`/`mPeakBytes`/`mAllocationCount`，零分配记录元数据。
- **单线程**：v1 不引入锁/TLS；后续并发需求再扩展。
- **RDG 集成**：`RenderGraph::Build` 改接收 `FrameAllocator&`；setup graph 容器迁移到 `TransientVector`；`Compile` 产出独立 `CompiledGraph`（从同一 arena bump）；`DeviceFrameContext` 持有 `FrameAllocator`。

## Capabilities

### New Capabilities

- `frame-allocator`: 帧作用域 bump allocator（`FrameAllocator`），含 `Mark`/`Rewind`、标准容器适配、bytes/peak 统计。

### Modified Capabilities

- `aurora-rdg`: setup graph 容器从 `std::vector`/`std::string` 迁移到 `TransientVector`/`TransientHashMap`；`Compile` 产出独立 `CompiledGraph`（扁平、只含 live pass、每 pass 一段连续 barrier）；`RenderGraph::Build` 签名增加 `FrameAllocator&`；`DeviceFrameContext` 持有 `FrameAllocator` 并负责帧末 `Reset`。

## Impact

- **受影响代码**：`engine/core/memory/LinearStorage.h/.cpp`、`TransientAllocator.h`、`engine/core/test/TransientAllocatorTest.cpp`；`engine/aurora/rhi/interface/include/aurora/rdg/*`、`engine/aurora/rhi/interface/src/rdg/*`；`engine/aurora/rhi/test/RDGTest.cpp`。
- **外部调用方**：`aurora-renderer` 未开，RDG 仅测试调用，无外部调用方受影响。
- **内存行为**：帧末统一 `Reset()` 释放所有帧数据；`Rewind` 用于 setup/compiled 阶段间回收。
