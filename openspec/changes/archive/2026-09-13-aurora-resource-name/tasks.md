## 1. 宏定义

- [x] 1.1 定义 `SKY_ENABLE_RESOURCE_NAME`（config 头 `BuildConfig.h`：`#if SKY_DEVELOP → 1`，否则 `0`，`#ifndef` 兜底）
- [x] 1.2 确认代码统一用 `#if SKY_ENABLE_RESOURCE_NAME` 判读

## 2. Descriptor name 字段

- [x] 2.1 `rhi::Buffer::Descriptor` 加 `#if` 守卫的 `const char *name = nullptr`
- [x] 2.2 `rhi::Image::Descriptor` 加 `#if` 守卫的 `const char *name = nullptr`

## 3. 后端 debug label 落地

- [x] 3.1 Vulkan：`VulkanFunctions` 补加载 `vkSetDebugUtilsObjectNameEXT`；`CreateBuffer/CreateImage` 判 `name` 非空且函数可用时 `VkDebugUtilsObjectNameInfoEXT` 设名
- [x] 3.2 DX12：`CreateBuffer/CreateImage` 用 `ID3D12Resource::SetName`（ASCII → UTF-16 宽字符）
- [x] 3.3 Metal：`CreateBuffer/CreateImage` 用 `MTLResource setLabel:`（`NSString stringWithUTF8String:`）
- [ ] 3.4 GLES：`CreateBuffer/CreateImage` 用 `glObjectLabel`（`GL_KHR_debug`）—— aurora 当前无 GLES 后端，待后端落地后补

## 4. 测试

- [ ] 4.1 `AuroraRHITest`：develop 构建下，创建带 name 的 buffer/image，验证各后端 label 正确（可读/调试器可见）
- [ ] 4.2 验证 release（宏关闭）下 Descriptor 无 name 字段、label 调用被剔除（编译/行为）
- [ ] 4.3 构建并运行 `AuroraRHITest` 确认通过

## 5. 构建与风格校验

- [ ] 5.1 确认新宏在 develop（`SKY_DEVELOP`）/release 两意图下正确生效
- [ ] 5.2 运行 clang-format / clang-tidy 校验新文件符合仓库规范
