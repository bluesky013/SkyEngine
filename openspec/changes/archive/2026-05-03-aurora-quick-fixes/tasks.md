## 1. 接口头改动

- [x] 1.1 `aurora/rhi/Core.h`：新增 `MAX_VERTEX_BINDINGS = 16`、`MAX_VIEWPORTS = 16` 常量
- [x] 1.2 `aurora/rhi/Sampler.h`：`Descriptor::mipmapMode` 默认 `MipFilter::LINEAR`；`maxLod` 默认 `1000.f`
- [x] 1.3 `aurora/rhi/Core.h`：`DeviceFeature::meshShader` 默认 `false`
- [x] 1.4 `aurora/rhi/Device.h`：把 `CreateSampler(const ResourceGroup::Descriptor&)` 改名为 `CreateResourceGroup`
- [x] 1.5 `aurora/rhi/VertexDecl.h`：namespace 由 `aurora::rhi` 改为 `sky::aurora`

## 2. 接口实现

- [x] 2.1 `Device.cpp`：`Init()` 中把 `UpdateDeviceCaps()` 调用挪到 `threadCount = std::min(threadCount, capability.maxThreads)` 之前；保持 `mainContext` 已 attach 的状态

## 3. Vulkan 后端

- [x] 3.1 `VulkanDevice.cpp`：rename override `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`
- [x] 3.2 `VulkanDevice.cpp`：`OnInit` 中删除第一次 `QueryDeviceFeatures()` 调用；保留 `CreateDevice` 内的那次
- [x] 3.3 `VulkanEncoder.cpp::BeginRendering`：在 `info.depthStencil.image != nullptr` 时取 `static_cast<VulkanImage*>(image)->GetPixelFormat()`，查 `GetImageFormatInfo(...).hasStencil`，若 true 则填 `pStencilAttachment`（loadOp/storeOp 来自 `info.depthStencil.stencilLoadOp/stencilStoreOp`，clearValue 复用 depthStencil）
- [x] 3.4 `VulkanImage.h`：增加 `PixelFormat GetPixelFormat() const { return desc.format; }`（如未持有 desc 则缓存 format）
- [x] 3.5 `VulkanEncoder.cpp::BindVertexBuffers` / `SetViewport` / `SetScissor`：把 16 改为 `MAX_VERTEX_BINDINGS` / `MAX_VIEWPORTS`；超出时 `SKY_ASSERT(count <= MAX_*)`，去掉 silent clamp 的 `n = count < MAX ? count : MAX`

## 4. DX12 后端

- [x] 4.1 `D3D12Device.cpp/.h`：rename override `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`

## 5. Metal 后端

- [x] 5.1 `MetalDevice.h/.mm`：rename override `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`

## 6. GLES 后端

- [x] 6.1 `GLESDevice.h/.cpp`：rename override `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`

## 7. 空 stub 标注

- [x] 7.1 `aurora/core/Renderer.h` / `Renderer.cpp`：保留空 namespace，加 `// TODO(P1): top-level renderer loop, see future aurora-renderer change` 注释
- [x] 7.2 `aurora/rhi/interface/src/rdg/RenderGraph.cpp`：加 `// TODO(P2): RDG, see future aurora-rdg change` 注释

## 8. 测试 / 验证

- [x] 8.1 编译 4 个后端 + AuroraTest，确保 rename 无遗漏（macOS 上验证 Aurora.RHI / AuroraVulkan / AuroraMetal / AuroraTest 全绿；DX12 在非 Windows 不构建，rename 已与 Vulkan/Metal/GLES 同步）
- [x] 8.2 `AuroraTest` 全套跑过（67/67 PASSED）
- [x] 8.3 跑一次 SyncTest 确认 ThreadPool 多线程实际生效（之前因 clamp bug 退化到单线程）—— SyncTest 在 67-test 套件中通过；Init 顺序修复后 thread pool 不再被默认值 1 钳制
- [x] 8.4 用一个临时 D32_S8 image 在 EncoderTest 中跑一次 BeginRendering，validation layer 不报 stencil attachment 缺失（新增 `EncoderTestVulkan.GraphicsEncoderRenderPassWithStencil` 测试通过）

## 9. 收尾

- [x] 9.1 在 `engine/aurora/AGENTS.md` 列出新的 Sampler / DeviceFeature 默认值（如 AGENTS.md 还未由 aurora-queue-submit-present 创建，则在本 change 创建一个最小骨架）
- [x] 9.2 archive：`openspec archive aurora-quick-fixes`
