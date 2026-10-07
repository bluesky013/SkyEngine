# Tasks: aurora-metal-bindless-descriptor-heap

（未实现，待排期）

## 1. 模型与分析

- [ ] 1.1 评估 slang `DescriptorHandle<T>`（指针型）与现有 `DescriptorHeap`（VK 索引型）如何收敛为统一 tier2 抽象
- [ ] 1.2 设计句柄型 bindless 的资源驻留与生命周期（跨帧、GPU 完成信号）
- [ ] 1.3 选定首个 bindless 用例（材质纹理数组 / 场景资源表）并写 `DescriptorHandle<T>` shader

## 2. 后端

- [ ] 2.1 Metal：句柄表（GPU 地址 / `gpuResourceID`）+ 驻留（`useResource:`/`useHeap:`）+ bind 路径；打开 `feature.descriptorHeap`
- [ ] 2.2 Vulkan：`VK_EXT_descriptor_heap` 实现（与 Metal 共享 shader 源）
- [ ] 2.3 升级/验证 slang v2026.9.2+ 对 `DescriptorHandle<T>` on Metal 的 emit；跟踪 #10842 / #12291

## 3. 验证

- [ ] 3.1 bindless 采样/访问正确性
- [ ] 3.2 跨后端一致性（Vulkan / Metal）
