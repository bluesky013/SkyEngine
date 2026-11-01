## Why

用户已确认目标为华为纯血鸿蒙，即原生 HarmonyOS（HarmonyOS NEXT 技术体系）。SkyEngine 已有 Windows、macOS、Android、iOS 和 Linux 平台代码，需要沿现有 Core / Framework / Aurora 架构增加 HarmonyOS 原生宿主和 HAP 打包链路，让项目从交叉编译走到华为设备上的渲染与场景运行。

状态：active，已完成 API 26 SDK/三方编译、鸿蒙原生宿主与 Vulkan cooked 物理场景，并完成同工程 Android 基础真机验证；正在整理提交和补充既有平台回归。输入/IME、原生输入框和 Host 重复 Surface 生命周期已完成本轮验收；公开 package 的完整 Native 依赖闭包自动校验仍有实现缺口，纹理/坏资源、真实 Renderer 的 Surface/GPU 退休及长期稳定性仍未全部验收。任务状态见 [tasks.md](tasks.md)，实测边界见 [validation.md](validation.md)。

## What Changes

- 增加独立 HarmonyOS 产品平台标识、华为 HarmonyOS SDK 工具链接入和三方依赖构建；初始计划为 arm64-v8a、C++20、Vulkan。底层编译器与 Native API 保留 SDK 的 OHOS 标识。
- 三方库集合、来源和锁定版本与 Android 一致，使用 OHOS 工具链重编并独立输出到 `HarmonyOS`。共享包选项，鸿蒙专用 SDK 接口与必要编译差异单独适配。
- 在现有 Core / Framework target 内增加 OHOS 分支，实现平台服务、XComponent 原生窗口、输入、生命周期及应用资源读取。
- 在现有 `engine/aurora/rhi/vulkan` 扩展 `OHNativeWindow` Surface 创建与能力检查，复用现有 RendererHost、帧协议及资源退休机制。
- 增加面向 HarmonyOS 的 project-owned ArkTS / Native launcher 模板，复用 `package_project.py`、export preset 和宿主 AssetTool，形成 DevEco/HAP 构建、资源部署、调试签名安装与华为设备验收链路。
- 统一移动端 orientation 与资源 staging，默认固定 landscape；安装身份只从平台 export preset 读取，Android Java namespace 保持独立，旧项目须显式迁移应用身份。
- 使用本机 HarmonyPhysics 工程验证鸿蒙/Android 物理场景（`examples/` 不入库），修复同进程会话重启时资源处理器被销毁的问题；保持 metadata 唯一注册 owner。
- 分阶段验证：SDK 预检 → Core/Framework 编译 → 原生窗口 → Vulkan 首帧 → cooked 场景 → 生命周期和打包回归。

开源 OpenHarmony 设备适配不在本次产品范围。首轮不覆盖移动端编辑器、OpenGL 后端、全量 feature plugin 和应用商店发布。物理、音频、网络、Python 等在最小场景通过后逐项开启。

## Capabilities

### New Capabilities

- `harmonyos-platform`: HarmonyOS 平台识别、OHOS Native 工具链、原生窗口、平台服务、输入和生命周期。
- `harmonyos-vulkan`: 现有 Aurora Vulkan 后端在 HarmonyOS 上的 Surface、能力门和安全呈现。
- `harmonyos-packaging`: project-owned HarmonyOS launcher、导出预设、HAP 签名安装及 cooked 资源交付。

### Modified Capabilities

- `aurora-rhi-core`：按用户要求把 Vulkan maintenance1 作为可选能力，增加 checked core WSI 兼容路径，明确应用帧退休与 native presentation 资源寿命的区别；对应 delta 与代码一起 review。其它平台服务/导出契约沿用现有定义。

## Impact

- 构建：`cmake/{configuration,options,thirdparty}.cmake`、`cmake/thirdparty.json`、`python/third_party.py` 和现有 target 的平台条件。
- Core / Framework：平台识别、日志、动态装载、线程、时钟、窗口、bundle 文件系统，以及新增的 `platform/ohos` 实现。
- Aurora：仅扩展现有 Vulkan target 与宿主的移动平台接入，不创建 Aurora 业务 plugin、不搬迁现有目录。
- 工程：`engine/templates/launcher/harmonyos`、`engine/templates/game`、`python/package_project.py`、export preset 数据/反射/schema，以及相关测试。
- 产品目标和开始实施已确认；本轮以 API 26、Pura 80 Pro / HarmonyOS 7.0.0.109 为验收范围，API 20 仅是历史预检，不承诺较低运行版本。实施遵循 [design.md](design.md) 的现有模块归属和 launcher 方案。
- 既有 `aurora-renderer-platform-parity` 继续负责其原有平台范围；本 change 只负责 OHOS 的平台胶水与验收，不重复实现 Renderer/RDG。
