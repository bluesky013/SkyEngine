## Why

Status: active. Windows Renderer 与 macOS Metal offscreen/native-window 宿主已完成；iOS/Android 原生窗口仍需各自平台设备验收。

## What Changes

- 接收 aurora-renderer 的 P02 与跨平台余项，保留原验收编号；Windows 结果作为参考，不重复实现共享 ECS/资源协议。
- 实现 Metal 独立 compiler/encoder、slot 分区、受检提交、上传与输出退休；验证 parent/children、autorelease 与强保活。
- 能力门通过后才切换对应平台宿主，并验证真实窗口、drawable timeout、物理旋转及失败退出。
- 统一 `AuroraModule` 宿主：所有平台都走 `RendererHost` + `IRendererProvider`，删除非 Windows 的 `InitLegacy/StartLegacy/TickLegacy`、`legacy` 标志、直接 `Device`/`DeviceFrameContext`/`ClientViewport` 成员及对应 `#if !defined(_WIN32)` 分支。

## Capabilities

### New Capabilities

- `aurora-renderer-platforms`: 新后端整套能力门、原生设备验收与跨平台对等记录。

## Impact

复用 aurora-renderer 的 IRenderer/RDG/RHI 契约；不移动目录，不新增 Aurora plugin，不把 Metal bindless debt 作为首版前置。现有 aurora-metal-bindless-descriptor-heap 保持独立债务记录。World 自动提取继续由 aurora-scene-bridge 负责。

宿主统一会删除 `engine/aurora/adaptor` 中的平台分叉代码，并新增非 Windows 宿主测试；这是本 change 的收口条件之一，不另建重复的 legacy-host 清理 change。
