# Change: aurora-metal-bindless-descriptor-heap

> 状态：Debt record（未实现，not scheduled）。记录 Metal 侧 tier2 bindless 的模型差异与落地路线；未排期，待上游 slang 就绪后再提案。

## Why

RHI 的 tier2 bindless 抽象 `DescriptorHeap`（`aurora/rhi/DescriptorHeap.h`）是**索引型堆**：
按 `VK_EXT_descriptor_heap` 语义，为 tex/buf/smp 各建一块 backing buffer（per-type stride），
分配返回 per-type index，shader 侧以该 index 访问。三个后端现状：

| 后端 | tier2 | 说明 |
|---|---|---|
| DX12 | ✅ | SM6.6 `ResourceDescriptorHeap[]`（slang HLSL 天然对应） |
| Vulkan | ❌ | 需 `VK_EXT_descriptor_heap` + 后端实现 |
| Metal | ❌ | `feature.descriptorHeap=false`、`CreateDescriptorHeap()` 返回 nullptr |

此前记「Metal 工具链不支持」。**该判断过窄**，需修正：

- slang 对 Metal 的 bindless 走**指针/地址型**模型：`DescriptorHandle<T>` 在 Metal 上具 `T` 的布局、以
  `device T*` 表示（可 `ulong` 化），**不是** argument buffer 的 `[[id]]` 形态。
- slang 对 `DescriptorHandle<T>` on Metal 的 **compiler emit 已完成**（slang #10842）；本仓库 pin 的
  第 `v2026.9.2` 已含（`slang-emit-metal.cpp` 的 `DescriptorHandleType` / `CastDescriptorHandleToUInt64`、
  `slang-ir-wrap-cbuffer-element.cpp` 的 `wrapCBufferElementsForMetal`）。
- 缺口在：(a) 我们未采用 `DescriptorHandle<T>` 的 shader/RHI 模型；(b) Metal 运行时实现；
  (c) 纯 `uniform` 数组-of-resource 仍有 codegen bug（slang #12291，Q3 2026 里程碑）——与本条 bindless 路径不同层。

即：**不是“工具链不支持”，而是“模型不一致 + 未实现”**。

## What Changes（拟）

1. **模型扩展**：tier2 抽象从“单一 VK 索引堆”扩展为可表达两种 bindless：
   - 索引型（Vulkan `VK_EXT_descriptor_heap` / DX12 `ResourceDescriptorHeap`）
   - 句柄/地址型（Metal `DescriptorHandle<T>` = GPU 指针表）
2. **Shader 路径**：bindless pass 改用 slang 的 `DescriptorHandle<T>`（不再用 `[[vk::binding]]` 直接绑定）。
3. **Metal 运行时**：实现句柄表（GPU 地址/`gpuResourceID`）+ 资源驻留（`useResource:`/`useHeap:`）+ 生命周期。
4. **Vulkan 运行时**：`VK_EXT_descriptor_heap` 实现（与 Metal 共享 shader 源）。
5. **能力标志**：按后端实际能力置 `feature.descriptorHeap`。

## Tasks（待排期）

- [ ] 1.1 评估 `DescriptorHandle<T>` 与现有 `DescriptorHeap`/`ResourceGroup` 模型的收敛方式（是否需要统一抽象）
- [ ] 1.2 设计句柄型 bindless 的资源驻留与生命周期（跨帧、GPU 完成）
- [ ] 1.3 选定首个 bindless 用例（材质纹理数组 / 场景资源表）并写对应 shader（`DescriptorHandle<T>`）
- [ ] 2.1 Metal 后端：句柄表 + 驻留 + bind 路径；打开 `feature.descriptorHeap`
- [ ] 2.2 Vulkan 后端：`VK_EXT_descriptor_heap` 实现
- [ ] 2.3 升级/验证 slang 版本对 `DescriptorHandle<T>` on Metal 的 emit（并跟踪 #12291 / #10842）
- [ ] 3.1 测试：bindless 采样/访问正确性 + 跨后端一致性

## Impact

- `aurora/rhi/interface`（DescriptorHeap/模型扩展）、`aurora/rhi/metal`、`aurora/rhi/vulkan`
- 采用 bindless 的 shader 与 pipeline/材质层
- 依赖上游 slang（#10842/#12291）
