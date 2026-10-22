## Context

基线 `230ee6b6` 的 FrameScheduler 在 prepare 与 acquire 后各查询一次实时配置。完整 QueryConfiguration 将 capabilities、格式和 present modes 全部重取；采样显示格式查询占绝大多数开销。现有 checked acquire/present 与独立 presentation fence 的退休契约必须保持。

## Goals / Non-Goals

**Goals:** 稳定帧保留两次轻量 capabilities 复核而不枚举格式/模式；原生失效时重新协商；提供 native 调用计数与性能证据。

**Non-Goals:** 不调整帧调度、UI dirty 绘制、Present 线程、公共 RHI、后端目录、构建配置或 HDR 输出策略。

## Decisions

1. `QueryCapabilities` 读取窗口像素尺寸与实时 VkSurfaceCapabilitiesKHR，计算 extent、usage、transform、alpha、image count。`QueryPresentationOptions` 单独选择 format/color-space/present-mode。创建时两者都执行；普通 QuerySurface 复用已应用的三个选项。capabilities 本身不含格式列表，不能删掉协商路径。
2. 同一 coordinator 持有的 backend-local dirty 标记由 acquire/present 的 SUBOPTIMAL 设置；OUT_OF_DATE 状态和无交换链也强制协商。完整查询失败不清 dirty；只有选项与已应用值一致才清除，变化持续报告 OutOfDate 直至安全重建。创建成功清 dirty 并推进既有 surfaceRevision；查询不推进创建代次。
3. SUBOPTIMAL acquisition 仍发布成功的 serial、image、semaphore/fence。获取后复核若发现变化，继续由 FrameScheduler abandon/release；present 的 SUBOPTIMAL 仍保留已排队 presentation fence。重复 SUBOPTIMAL 可重复协商，但相同选项不无条件重建。避免通过伪造 acquire 失败丢失资源责任。
4. extent/transform 等改变立即报告 OutOfDate；重建时重新枚举格式/模式。跨屏/HDR 导致当前交换链不再匹配时依赖 Vulkan 的 SUBOPTIMAL/OUT_OF_DATE 通知。仅新增可用格式、但当前 SDR 配置仍可用，不自动切换输出策略。
5. 延用现有 native FrameOutputs 的函数表注入测试，不增加生产计数器、测试专用 RHI 或 target。临时性能探针只存在 ignored output 备份与验证期间，结束后恢复源码并重建普通二进制。

旧 direct 协议的 QuerySurface 继续完整协商，兼容其已有获取/呈现行为；稳定帧优化面向 FrameScheduler 使用的 checked 协议，不扩大到遗留原生对象调用链。

## Risks / Trade-offs

- 驱动持续 SUBOPTIMAL → 持续完整复核是保守回退，避免擅自吞掉真实格式变化；稳定正常路径获得收益。
- capabilities 不能发现单独的格式变更 → native invalidation 触发完整查询，并测试尺寸不变的格式/色彩空间/模式变化。
- 查询和获取间状态变化 → 保留获取后的实时复核与显式 abandon 退休。
- 测试注入不等于实机 HDR/跨屏验收 → 分开报告本机测试与未具备的显示器场景；不宣称全部平台已验。

## References

- [VkSurfaceCapabilitiesKHR](https://docs.vulkan.org/refpages/latest/refpages/source/VkSurfaceCapabilitiesKHR.html)
- [Vulkan WSI acquire/present result semantics](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html)

## Migration Plan

无数据/接口迁移。单独提交此 change 的代码与 spec；如回退只撤销此优化，不回滚已提交的 Gizmo/相机基线。
