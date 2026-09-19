## 1. AuroraRender 模块

- [x] 1.1 新增 `engine/aurora/adaptor/`：`AuroraModule.h`（`AuroraModule : sky::IModule`）+ `AuroraModule.cpp`
- [x] 1.2 `AuroraModule::Init`：解析 `-r` → API，`Instance::Init` + `CreateDevice`，建 `DeviceFrameContext`（inflight=1）+ GRAPHICS `CommandPool`/`CommandBuffer`
- [x] 1.3 `AuroraModule::Start`：经 `ISystemNotify::GetMainWindowHandle()` 取主窗口 handle → `ClientViewport::Init(device, SwapChain::Descriptor{window,...})`
- [x] 1.4 `AuroraModule::Tick`：frame begin → viewport `Begin`/`Acquire` → backbuffer barrier → clear → barrier → submit（acquire/render-done sema + frame fence）→ present → frame end
- [x] 1.5 `AuroraModule::Shutdown`：逆序销毁
- [x] 1.6 `AuroraRegistry.cpp`：`REGISTER_MODULE(sky::aurora::AuroraModule)`

## 2. 构建接入

- [x] 2.1 `engine/aurora/adaptor/CMakeLists.txt`：`sky_add_library(TARGET AuroraRender SHARED ...)`，link `Aurora.RHI` + `Framework`
- [x] 2.2 `engine/aurora/CMakeLists.txt`：`add_subdirectory(adaptor)`
- [x] 2.3 依赖：后端 dll `add_dependencies` + `sky_add_dependency(TARGET AuroraRender DEPENDENCIES Launcher)`；产物落到 `output/bin/<cfg>` 与 exe 同目录

## 3. Launcher 接入（移除 legacy render）

- [x] 3.1 `GameApplication`：只注册/加载 `AuroraRender`，移除 `--renderer` 选择与 `SkyRender` 注册
- [x] 3.2 `Launcher`：从 LIBS 移除 legacy `RenderAdaptor`
- [x] 3.3 窗口 handle：`ISystemNotify::GetMainWindowHandle()` + `GameApplication` 实现；`AuroraModule::Start` 优先用它（回退 `Platform::GetMainWinHandle()`）

## 4. 引擎内置 configs

- [x] 4.1 新增 `engine/configs/`（modules_game/modules_editor/asset_build_presets/render_preload_assets），参考 TestProj
- [x] 4.2 `Launcher` POST_BUILD 把 `engine/configs` 拷到 exe 旁 `configs/`
- [x] 4.3 `GameApplication`：`CONFIG_PATH` 统一 `configs/modules_game.json`，workFs 缺失时回退到 exe 旁内置配置
- [x] 4.4 `engine/configs/modules_game.json` 依赖改为 `AuroraRender`

## 5. 验证

- [x] 5.1 构建 `AuroraRender` + `Launcher` 成功，`AuroraRender.dll` 落到 `output/bin/Debug`
- [x] 5.2 `-r dx12` 启动：AuroraRender 加载、设备创建、viewpot 创建（无 "no main window handle"）、主循环运行 10s 无崩溃、无 validation 报错
- [x] 5.3 `-r vulkan` 启动：AuroraRender 加载、VK 设备创建、主循环运行无崩溃
- [x] 5.4 `openspec validate aurora-launcher-integration --strict` 通过

## 待办（后续，不在本 change 范围）

- 窗口尺寸驱动 swapchain 初始大小（当前模块用固定 1280x720，靠 `GetSurfaceSize`/Resize 自纠）
- 出帧的人工视觉确认 / pixel readback 验证
