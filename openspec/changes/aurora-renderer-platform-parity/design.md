## Context

Windows Vulkan 双 compiler 与 DX12 已完成 B01–B14/P01。新 Metal 接口当前明确 Unsupported，旧平台宿主仍可用。此 change 接收原 tasks 的跨平台验收余项，不重新定义 FramePacket、资源精确版本或退休协议。

## Decisions

1. 先在 Mac 建立离屏与原生窗口 probe，确定 surface/drawable 的线程、超时和 presentation 完成证明；没有证据不切换宿主。
2. Metal 自行生成 immutable native scope、encode DAG、submission plan。parent 创建/结束归明确线程，child 保序并强持有；不支持的并行 scope 采用已验证的后端串行策略。
3. 按 slot/worker 隔离分配与 reset，单一 completion owner；提交不确定时保活而不伪造完成。失败和取消均必须 drain。
4. 复用 Windows 内容语义、upload/readback、双 slot 与乱序编码案例，添加 Mac 原生失败、最小化、resize/close；移动设备旋转单列实机证据。
5. 同一进程只选择一套宿主。只有 API/OS/设备组合完整通过能力门后才替换旧入口。
6. 对等完成后收口宿主代码：`AuroraModule` 不再保留非 Windows legacy 分支。所有平台统一经 `RendererHost` 持有唯一 `IRenderer`；`InitLegacy/StartLegacy/TickLegacy`、`legacy` 标志、`mDevice`/`mFrameContext`/`mCommandPool`/`mCommandBuffer`/`mViewport` 及对应头文件条件编译全部删除。非 Windows 真实窗口/离屏宿主必须先通过 capability gate，不能仅以可编译作为删除条件。

   macOS gate 已通过：`aurora-metal-frame-context` 实现 Metal slot/clear/copy/raster draw，原生 Cocoa/CAMetalLayer 宿主测试完成 Start/Tick/FrameCompleted/Shutdown。8.7/8.8 已完成；iOS/Android 实机验证留在各自平台 change。

## Risks / Open decisions

Metal drawable 的可退出等待、present 完成依据和不同设备的并行支持必须通过原生 probe 决定。Windows 构建不算该能力的验证。功能扩展/HDR 不随本平台迁移自动增加。
