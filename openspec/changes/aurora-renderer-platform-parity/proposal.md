## Why

Status: blocked-on-prereq. Windows Renderer 已完成；Metal 新帧协议和非 Windows 原生宿主尚未完成。实施前置条件是可使用的 macOS/Metal 构建与 GPU 验证环境；iOS/Android 的原生窗口验收还需要相应设备。不是已排期任务。

## What Changes

- 接收 aurora-renderer 的 P02 与跨平台余项，保留原验收编号；Windows 结果作为参考，不重复实现共享 ECS/资源协议。
- 实现 Metal 独立 compiler/encoder、slot 分区、受检提交、上传与输出退休；验证 parent/children、autorelease 与强保活。
- 能力门通过后才切换对应平台宿主，并验证真实窗口、drawable timeout、物理旋转及失败退出。

## Capabilities

### New Capabilities

- `aurora-renderer-platforms`: 新后端整套能力门、原生设备验收与跨平台对等记录。

## Impact

复用 aurora-renderer 的 IRenderer/RDG/RHI 契约；不移动目录，不新增 Aurora plugin，不把 Metal bindless debt 作为首版前置。现有 aurora-metal-bindless-descriptor-heap 保持独立债务记录。World 自动提取继续由 aurora-scene-bridge 负责。
