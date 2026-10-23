## Context

Windows Vulkan 双 compiler 与 DX12 已完成 B01–B14/P01。此 change 接收原 tasks 的跨平台验收余项，不重新定义 FramePacket、资源精确版本或退休协议。当前 Metal checked 宿主、无源码 shipping 读回及 RHI 回归已有独立原生证据；iOS/Android 交叉编译通过，移动设备验收仍按 tasks.md 跟踪。

## Decisions

1. 先在 Mac 建立离屏与原生窗口 probe，确定 surface/drawable 的线程、超时和 presentation 完成证明；没有证据不切换宿主。
2. Metal 自行生成 immutable native scope、encode DAG、submission plan。parent 创建/结束归明确线程，child 保序并强持有；不支持的并行 scope 采用已验证的后端串行策略。
3. 按 slot/worker 隔离分配与 reset，单一 completion owner；提交不确定时保活而不伪造完成。失败和取消均必须 drain。
4. 复用 Windows 内容语义、upload/readback、双 slot 与乱序编码案例，添加 Mac 原生失败、最小化、resize/close；移动设备旋转单列实机证据。
5. 同一进程只选择一套宿主。只有 API/OS/设备组合完整通过能力门后才替换旧入口。
6. 对等完成后收口宿主代码：`AuroraModule` 不再保留非 Windows legacy 分支。所有平台统一经 `RendererHost` 持有唯一 `IRenderer`；`InitLegacy/StartLegacy/TickLegacy`、`legacy` 标志、`mDevice`/`mFrameContext`/`mCommandPool`/`mCommandBuffer`/`mViewport` 及对应头文件条件编译全部删除。非 Windows 真实窗口/离屏宿主必须先通过 capability gate，不能仅以可编译作为删除条件。

   2026-10-22 复核：现有基线已统一宿主，但原 Cocoa smoke 仅 tick 四次，不能证明 FrameCompleted，已标为 superseded。修正后的 Cocoa 宿主测试等待完成并验证 drain/close deferral，RHI 154/154 通过；这些证据不代替 iOS/Android 实机验证。

## Risks / Open decisions

Metal drawable 的可退出等待、present 完成依据和不同设备的并行支持必须通过原生 probe 决定。Windows 构建不算该能力的验证。功能扩展/HDR 不随本平台迁移自动增加。

## Native acceptance corrections

Keep existing Aurora targets and RHI directories. Complete Metal's checked swapchain protocol before claiming its native host gate: drawable ownership, GPU completion and presentation retirement are separate facts. A nonblocking acquire may report busy while the native drawable request remains owned; resize/close cannot invalidate that request or any live frame.

Mobile surface replacement preserves scene state but retires the old output generation before creating its successor. The host retains the exact native surface through packet acceptance, GPU use and output retirement, coalesces replacement events to the newest handle, and keeps polling/submitting control work while no surface is available. An unsupported presentation capability is an observable host error rather than a repeated warning with a permanently black output.

Editor integration reuses the existing RendererHost and scene bridge on macOS in the current EditorRender target and source paths. Shader resource selection and project deployment corrections belong to `refactor-aurora-pipeline-resource-ownership`; this change consumes that resource graph without introducing a second host or packaging path.

The Metal acquire worker sets `allowsNextDrawableTimeout=YES`; Apple's native one-second wait is isolated from checked calls. Teardown joins the request before releasing its layer. A completed present command buffer proves that this implementation may retire its native references, not physical scanout time. References: https://developer.apple.com/documentation/quartzcore/cametallayer/allowsnextdrawabletimeout and https://developer.apple.com/documentation/metal/creating-a-custom-metal-view.

Native acceptance also exposed a Metal raster dynamic-offset omission. The compiled draw now retains binding-sorted offsets beside its immutable resource groups, and dispatch passes those offsets explicitly; two draws with distinct uniform slices are verified by readback. Compile/dispatch use bounded autorelease scopes while retained native plan/submission owners survive those scopes.

## Stage-specific shader argument indices

The stripped Metal host exposed duplicate MSL buffer indices across logical descriptor sets and an active compute image omitted by mixed-category filtering. The resource-ownership change owns the correction: cache/reflection version 5 preserves `(set, binding, stage) -> native index`, while the logical resource determines its native category. A layout-only SPIRV target in the same MSL session supplies logical keys by parameter name; no SPIRV bytecode or packing is used. Metal layout supplies block offsets/sizes, native indices and active-resource queries. Metal consumes each stage's mapping rather than deriving indices from merged resource order; conflicting mappings and unsupported native capacity fail explicitly.

Old shader products require recooking. Source-free Core resolution and library-selection audits are separate from native execution: scene, compute-to-fullscreen, offscreen readback and checked window presentation still require their own successful native runs. This correction keeps the existing Aurora/Metal targets and resource owners.

## Depth views and native capabilities

The Intel native probe read correct depth bytes from a restricted array mip/layer view while sampling returned black. Raster compilation now inserts a GPU copy of only the requested depth subresources to an independent array texture before sampling. Identical ranges share that copy within the pass; attachment writes retain their original views, and complete depth arrays and color views keep their direct path. Copy textures and steps follow the compiled plan into the submission payload and retire after GPU completion. The existing depth-array regression now verifies color and alpha after write, load, copy and sample.

Image format advertisement and creation use the same BC support query. Multisample arrays and extended sampler address modes require guarded OS availability plus the supported Apple7/Mac2 GPU family; unsupported descriptors fail before native creation. Supported sample counts are queried from the device, and invalid MSAA mip/compression combinations are rejected. Evidence: [Metal capabilities](https://developer.apple.com/metal/capabilities/), [Apple A14 GPU capabilities](https://developer.apple.com/videos/play/tech-talks/10858/) and the SDK's availability declarations. These guards are cross-compiled for iOS 13; unavailable hardware remains a separate acceptance boundary.
