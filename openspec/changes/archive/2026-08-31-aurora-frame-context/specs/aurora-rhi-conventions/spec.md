## REMOVED Requirements

### Requirement: Device 线程池容量按 capability.maxThreads 钳制

**Reason**: 并行编码的 ThreadPool 已从 `Device` 迁移到 `DeviceFrameContext`（见 `aurora-frame-context` 的 ADDED requirement）。worker 线程数改由 `DeviceFrameContextInitInfo.parallelNum` 指定，不再在 `Device::Init()` 里按 `min(hwConcurrency - 1, capability.maxThreads)` 钳制。

**Migration**: 上层不再调用 `device->GetParallelContext()`；改为通过 `DeviceFrameContext::GetParallelContext()` 获取并行线程池，并在创建 FrameContext 时传入 `parallelNum`。
