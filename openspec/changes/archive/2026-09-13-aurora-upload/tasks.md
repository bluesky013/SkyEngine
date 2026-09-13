## 1. isUMA capability

- [x] 1.1 `DeviceCapability` 新增 `bool isUMA = false`
- [x] 1.2 Vulkan `UpdateDeviceCaps()`：查 `DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT` memory type
- [x] 1.3 DX12 `UpdateDeviceCaps()`：查 `D3D12_FEATURE_DATA_ARCHITECTURE`（`UMA`/`CacheCoherentUMA`）
- [x] 1.4 Metal `UpdateDeviceCaps()`：查 `MTLDevice.hasUnifiedMemory`
- [ ] 1.5 GLES `UpdateDeviceCaps()`：恒置 `true` —— aurora 无 GLES 后端，待落地

## 2. Queue 上传接口

- [x] 2.1 新增 `TransferTaskHandle` 类型（`using TransferTaskHandle = uint32_t`，Core.h）
- [x] 2.2 `Queue` 新增 `UploadBuffer`/`UploadImage`（复用 `BufferUploadRequest`/`ImageUploadRequest`）

## 3. per-frame staging ring

- [ ] 3.1 staging ring 管理：v1 用 per-upload 临时 staging（Vulkan 已落地），持久 ring 待后续
- [ ] 3.2 段回收：v1 同步 `fence->Wait()` 后即复用，无持久 ring

## 4. 后端上传实现（拓扑 × in-flight 路径选择）

- [x] 4.1 目标不可 host 直写（discrete `GPU_ONLY`）→ staging + `CopyBuffer`（Vulkan 已落地）
- [ ] 4.2 目标 host 可见且不在途 → 直接 map + memcpy（UMA 直写优化，待后续）
- [ ] 4.3 目标 host 可见且在途 → staging + copy（流式上传场景，待后续）
- [ ] 4.4 四后端落地：Vulkan 已实现；DX12/Metal 现为 stub（返回 0）；GLES 无后端

## 5. 测试

- [ ] 5.1 `AuroraRHITest`：isUMA 上报（集成/独显路径）
- [ ] 5.2 staging ring 回收：多帧上传不覆盖在途区段
- [ ] 5.3 UMA 非在途直写 vs 在途 staging；discrete staging
- [ ] 5.4 构建并运行 `AuroraRHITest` 确认通过

## 6. 构建与风格校验

- [ ] 6.1 新文件被相应 target 收集，编译通过
- [ ] 6.2 运行 clang-format / clang-tidy 校验新文件符合仓库规范
