## 1. core 接口

- [x] 1.1 `core/async/ThreadPool` 加 public `Schedule(ThreadTask&&)`（round-robin 入 local queue）

## 2. DeviceFrameDispatcher

- [x] 2.1 `aurora/rhi/interface/include/aurora/rdg/DeviceFrameDispatcher.h`：单线程构建 / 批次提交 / index 引用
- [x] 2.2 `aurora/rhi/interface/src/rdg/DeviceFrameDispatcher.cpp`：无锁无引用计数实现，两阶段 pendingParents，Submit 先收集根节点

## 3. 测试

- [x] 3.1 `aurora/rhi/test/DeviceFrameDispatcherTest.cpp`：线性链 / diamond / 多根 / 空批次 / Clear 复用 / per-node + 批次 future
- [x] 3.2 `aurora/rhi/test/DeviceFrameDispatcherBenchmark.cpp`：TaskNode vs DeviceFrameDispatcher 吞吐对比

## 4. 收尾

- [x] 4.1 全量 CoreTest + AuroraTest 通过
- [x] 4.2 clang-format
