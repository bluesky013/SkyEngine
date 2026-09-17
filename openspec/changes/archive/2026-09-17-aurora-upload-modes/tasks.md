## 1. StaticBuffer 异步完成同步

- [x] 1.1 `Buffer.h`：`StaticBuffer` 新增成员 `TransferTaskHandle pendingHandle` + `Queue *pendingQueue`；`Upload` 保存两者
- [x] 1.2 `IsUploadComplete()` / `WaitUploadComplete()`；析构（destructor）前 `WaitUploadComplete()`（`Release()` 也补等待，防御性）

## 2. Texture 异步完成同步

- [x] 2.1 `Texture.h`：`Texture` 新增成员 `pendingHandle` + `pendingQueue`；`UploadImage` 保存两者
- [x] 2.2 `IsUploadComplete()` / `WaitUploadComplete()`；析构（destructor）前 `WaitUploadComplete()`（`Release()` 也补等待，防御性）

## 3. 当帧 staging 上传（FrameStagingBuffer）

- [x] 3.1 `FrameStagingBuffer.h`（新增）：包装 `StagingBufferAllocator` + pending copy 队列 + `Upload`/`Flush`/`Reset`
- [x] 3.2 `DynamicBuffer`/`TransientBuffer` 注释 + 契约：`framesInFlight` 与 `DeviceFrameContext::inflightNum` 对齐、`AdvanceFrame()` 锁步（不改实现）

## 4. 测试

- [x] 4.1 `BufferResourceTest.cpp`：`VertexBufferUploadSmoke` 后断言 `WaitUploadComplete()` / `IsUploadComplete()`
- [x] 4.2 `TextureResourceTest.cpp`：`TextureLazyCreateAndUpload` 后断言 `WaitUploadComplete()` / `IsUploadComplete()`
- [x] 4.3 `BufferResourceTest.cpp`：`FrameStagingUpload`（staging 上传 + 内联 copy + 提交等待 + 读回验证）
- [x] 4.4 验证析构前等待消除 `vkDestroyBuffer`/`vkDestroyImage` 校验错误（无在途销毁 ERROR 日志）

## 5. 验证与收尾

- [x] 5.1 `cmake --build` AuroraCore 通过
- [x] 5.2 `AuroraCoreTest` 全绿
- [ ] 5.3 `openspec archive aurora-upload-modes` 归档（需用户确认）
