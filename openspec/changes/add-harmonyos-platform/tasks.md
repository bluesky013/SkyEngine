# 纯血鸿蒙（HarmonyOS）接入待办

状态：active；canonical API 26 SDK 下 Android 同集合 15 包已实际重建，严格预检为 0 missing/stale、0 output-missing。Pura 80 Pro 已验证签名 HAP、Vulkan 清屏、HarmonyPhysics 的 cooked World/material/indexed mesh、物理模拟及前后台恢复；新 OHOSHost 通过 30 项设备断言和 65 秒真实组件生命周期压力。Windows FrameworkTest 完整 270 项通过，含公共输入框状态 17 项和 OHOS 输入策略 13 项。Android 隔离 UI 的 9 个终态、OHOS 修复后的 7 个 UI 流程和原生中文 IME 通过，两端无验收 probe 的纯正式包已公开重建、审计并恢复真机物理场景及同进程前后台；用户暂无 Mac，UIKit 编译运行明确 deferred。纹理专项、缺资源/图形失败路径、旋转及长期压力仍待完成；短测 RSS 增长不作为无泄漏证明。用户明确三方库集合/版本与 Android 保持一致，并要求独立 platform；`[x]` 只代表有确认/实测/产物证据的任务完成。

依赖顺序：P0 → P1 → P2 → P3 → P4 → P5。P6 是后续扩展，只有具体需求/设备条件满足才排期。各阶段代码与对应 spec 一起 review，不混入目录重组。

| 阶段 | 交付目标 | 验收门槛 |
|---|---|---|
| P0 | 平台方案、工具链预检和待办 | 明确产品/API/设备与模块/入口方案 |
| P1 | 可重复的 OHOS 构建 | Android 同集合/版本依赖独立构建；Core/Framework arm64 编译通过 |
| P2 | Native 宿主与平台服务 | HAP 打开窗口，输入/生命周期/资源读取可用 |
| P3 | Vulkan 首帧 | 真机能力门通过，clear/triangle 与安全恢复通过 |
| P4 | 项目打包与场景 | public package 命令生成可安装、能加载 cooked 场景的 HAP |
| P5 | 回归与支持状态 | 真机长循环/失败路径和现有平台回归有记录 |
| P6 | 可选能力 | 按功能逐项验证，不阻塞首个最小场景 |

## 1. P0 — 基线与方案

- [x] 1.1 获取 origin 并 rebase 当前跟踪分支；记录 `60079ac7 → 2f8e9dff`、15 个提交、无冲突。
- [x] 1.2 梳理最新平台、Vulkan、launcher/packaging 接入点及已有 change 的责任边界，完成 proposal/design/specs。
- [x] 1.3 检查本机 SDK 元数据和 OHOS 宏；完成 C++20 arm64 Native 头文件及 `.so` 编译/链接预检，记录证据与未验证范围。
- [x] 1.4 确认产品目标：用户明确需要纯血鸿蒙系统适配，以华为原生 HarmonyOS 为本次产品范围。
- [x] 1.5 用户在方案给出后指示“开始集成”，并补充独立 platform 与 Android 三方库对齐约束；按已说明的 Core/Framework/Aurora 归属和 launcher 方案开始实施。
- [x] 1.6 严格校验 OpenSpec，核对任务状态、链接和验收记录。
- [x] 1.7 本轮真机验收锁定 HUAWEI Pura 80 Pro（LMR-AL00）、7.0.0.109、API 26、arm64-v8a，HDC USB 已授权连接；与当前 HAP API 26 配置匹配，不承诺更低系统兼容。模拟器仍可作为补充验证。
- [x] 1.8 获取官方 Command Line Tools 26.0.0.865 并实测 Native SDK 26.0.0.105 / API 26、Clang 15.0.4 新 revision 与 libc++ 15004；复测 C++20/NEON 及 Core/Framework。ranges 通过 SDK 的 experimental-library 选项可用；memory_resource 和 string/optional 三路比较限制仍存在，详见 validation。

## 2. P1 — 构建与三方依赖

产品目标 1.4 已满足；涉及模块归属/target/装载入口的实现前置为 1.5。1.7 限定设备验收范围，不阻塞已有 SDK 的工具链准备；本机 SDK 路径只作为参数，不写入工程构建文件。

- [x] 2.1 增加 `SKY_PLATFORM_OHOS` / `PlatformType::HarmonyOS` 和名称映射；新增 `harmonyos` CMake preset，真实 SDK 编译验证 OHOS 优先于 Linux、既有有效平台值保持兼容、UNDEFINED 保持末尾。项目导出 preset 属于 5.2。
- [x] 2.2 增加 `CMAKE_SYSTEM_NAME=OHOS`、移动端标识、PIC、独立输出和平台缓存选择；排除设备端桌面 tools/sandbox，保留 Android 同集合的三方和 shader compiler 配置；拒绝错误 ABI/STL 和其它平台依赖缓存。
- [x] 2.3 扩展 `third_party.py` 的 HarmonyOS/SDK 入口，使用 arm64-v8a/c++_shared；依赖 key 和中间目录包含 SDK 身份，缺 SDK 显式失败。
- [x] 2.4 对齐 Android 全部 15 个三方包及版本，继承 Android 包选项后应用 HarmonyOS 特有覆盖；增加集合一致性回归测试，不单列精简包集。
- [x] 2.5 核对并复用 Unix 线程/信号量，适配 OHOS 日志、模块装载和基础文件路径；真实 Core/Framework target 全量编译链接通过，HAP 内游戏/Jolt/Renderer 模块已随物理场景装载运行。Rawfile 读取失败专项另归 3.5/5.6。
- [x] 2.6 完成 Core/Framework 源码归属、构建契约、arm64/NEON 与宿主回归：41 项 Python 契约/源码归属、真实 API 26 whole-archive/no-undefined 链接通过；保留现有 VS2022 cache 重编 Framework/FrameworkTest，最终完整 270 项通过。
- [x] 2.7 API 26 下逐包完成 Android 同集合 15 包的实编/安装，`--check --strict` 为 0 missing/stale、0 output-missing。RapidJSON、VulkanHeaders、VMA、ACL 按 header 包记录配置/安装，不宣称有独立二进制。
- [x] 2.8 完成 Slang host codegen 和 Debug/Release 目标库、OpenSSL Release、CPython 3.13.9 musl/pyconfig 与 stdlib ZIP 的实编/安装；CPython 编译配置探测避免把空 libintl stub 当作 gettext 实现。设备 TLS/Python 运行另行验证。
- [x] 2.9 在 `D:/Program Files/Huawei/sdk/default/openharmony/native` 使用公开 third_party 流程实际重建/安装全部 15 包到 `build/harmonyos-thirdparty/HarmonyOS`，显式并行 6；canonical 路径 `--check --strict` 为 0 missing/stale、0 output-missing，SDK 内容指纹保持一致，未通过改 key 冒充重编。
- [ ] 2.10 Android 三方完整重建完成 14/15；CPython 3.13.9 在 frozen.c/getpath.c 缺生成头，需修复 Android frozen-module 生成步骤并重跑严格预检。HarmonyPhysics 未启用 Python，不将其真机通过等同于全部三方完成。

## 3. P2 — Native 宿主、窗口和服务

前置：P1；不以 stub 返回成功作为平台能力完成。

- [x] 3.1 已增加 Stage UIAbility/XComponent、N-API Native Launcher 和 Hvigor 模板，并编出真实 HAP；真机启动并在同一进程完成一次前后台恢复，GameApplication/module 仅初始化一次。
- [x] 3.2 实现 OHOSPlatform/OHOSWindow 及工厂分支，接通 Surface create/change/destroy、稳定 WindowID 与 framebuffer 尺寸；两真实 XComponent 的设备断言验证替换保留 WindowID，65 秒真实销毁/重建验证 48 次接入及 Host 引用收齐。
- [ ] 3.3 已接通 NativeVSync 与外部帧循环；30 项设备断言验证后台/零尺寸跳帧、迟到 generation、Destroy 中回调、Stop/join 与跨会话迟到回调；65 秒真实 SDK VSync/组件压力通过。恢复首帧 delta 的数值专项仍未实测，不用短测代替完整时钟验收。
- [x] 3.4 实现 primary-touch、取消、鼠标/键盘和 committed-text 路径；13 项桌面策略测试通过。Pura 80 Pro 实测 pointer 注入 (2000,300) 精确映射、原生 key 2017→scan 4、IME 提交 U/v/中文“你”，拼音 ni 预编辑不提交；键盘隐藏后仅显式 Start 即可重新弹出，截图与提交日志齐备。多接触触控压力不由这些有限设备输入结果推断。
- [ ] 3.5 实现 Rawfile/bundle 文件系统、沙箱路径、HiLog、单调时钟及必要平台服务；不支持项显式返回失败，验证读取失败不泄漏资源。
- [x] 3.6 Pura 80 Pro 成功安装并启动调试签名 HAP，缺 maintenance1 时持续呈现、前后台恢复和正常退出通过；新 Host 重复附着、Surface 替换及过期回调通过 6.10，输入/IME 与修复后的原生 UI 通过 3.4/3.9。更完整资源/图形失败矩阵仍归 6.2，临时验收后的纯正式包恢复另归 6.11。

- [x] 3.7 鸿蒙 launcher 在加载页面前设置全屏布局并隐藏状态栏、导航栏和手势指示条，忽略过期 stage 完成回调；模板及 HarmonyPhysics 同步，Pura 80 Pro 实测 1276×2848 swapchain 和同进程前后台恢复通过。
- [x] 3.8 统一 `NativeTextInputOptions`、快照事件与 owner-thread 一次性终态，Android/ArkUI/UIKit 使用各自原生编辑 UI；公共状态 17 项及桌面 OHOS 输入策略 13 项通过，非法终态文本与桥接转换失败使用最后合法快照收尾，不记录密码内容。
- [x] 3.9 完成本轮 Android/OHOS 原生 UI 真机验收，覆盖单行/多行、Unicode 标量长度、密码、取消/后台、重开。Android 隔离 fixture 的 native 121/UI 20 检查通过、9 终态（6 confirmed/2 cancelled/1 error），实际 JNI null→ERROR 保留最后合法文本且 retry 成功。OHOS 桥接修复后 PID 31461 完整 7 cases/0 fail，19 CHANGED、5 CONFIRMED、2 CANCELLED，IME 展开时编辑区/动作可见。隔离验收与纯正式包恢复分开，后者仍归 6.11。
- [ ] 3.10 UIKit 源码及设备测试/runner 已提供；用户明确暂无 Mac，编译与 UIKit 运行 deferred，后续有 Mac 再验收，不阻塞本轮 OHOS/Android 验收。37 项 Python owner/runner 检查不作为 UIKit 编译或运行证据。

## 4. P3 — Aurora Vulkan 首帧

前置：P2；可在 P1/P2 期间先收集目标设备 Vulkan capability 信息。

- [x] 4.1 在既有 Vulkan target 增加 OHOS 宏、扩展与函数加载，统一 Khronos 头并检查 `vkCreateSurfaceOHOS`、库名和 native ABI；生产 Aurora.Vulkan 的 API 26 编译/no-undefined 链接通过。
- [ ] 4.2 使用 `OHNativeWindow` 创建 Surface，检查 present queue、格式、extent 与 swapchain 能力；覆盖缺扩展/缺入口/创建失败的显式错误。
- [x] 4.3 真机 Maleoon 920 报告 Vulkan 1.3.309，frame context 初始化通过；未提供 `VK_EXT_surface_maintenance1` / `VK_EXT_swapchain_maintenance1`，已按能力选择实际实现的 core WSI 兼容呈现路径，未伪报扩展支持。
- [x] 4.4 窗口已接入既有 RendererHost / Aurora runtime，两轮各 600 帧蓝绿 clear 通过；HarmonyPhysics 进一步验证了带 vertex/index 上传的立方体 mesh 和四个 cooked 材质，已保存真机截图/日志。
- [ ] 4.5 验证 resize、零尺寸、后台、Surface 销毁/重建、acquire 失败和退出，检查旧 generation 不复用、任务收齐、GPU/present 退休。
- [x] 4.6 按用户要求将 OHOS Surface 扩展校验/创建拆到 `vulkan/platform/ohos`，HiLog 拆到 `core/platform/ohos`；既有 target 选择源文件，HAP 实编/签名和 36 项 Python 契约/源码归属测试通过。
- [ ] 4.7 已实现 maintenance1 / core WSI 能力选择、逐图像 semaphore、应用帧与呈现资源分离、取消/resize/close；手机上 3 项协商测试与持续可见呈现通过。提交前 Windows FrameOutputs 回归 119 通过、13 个不适用参数跳过，覆盖两种 Vulkan compiler 下强制兼容、旧 generation 回收、放弃帧及 resize 查询失败；core WSI 最终关闭的队列排空限制仍保留。

## 5. P4 — 正式打包与 cooked 场景

前置：P3；P2 的最小 HAP 仅用于平台验证，本阶段才接入公开项目/导出工作流。

- [x] 5.1 已扩展现有 `create/generate/package` 的 HarmonyOS 分支，保留工程自有 launcher，生成 Hvigor/CMake 工程；模板与失败产物失效测试通过。完整 cook/package 执行仍属于 5.6。
- [x] 5.2 新增 HarmonyOS 导出 preset，复用既有 ABI/API/signing/packing/cacheVariables 字段，无新增 schema key；C++ shader environment 接受 HarmonyOS Vulkan，Python schema 契约通过。当前导出兼容 API 为 26，不承诺低版本。
- [x] 5.3 宿主 AssetTool/ShaderCookTool 已通过 HarmonyOS 平台映射烘焙 World/material 和 Vulkan shader environment；HarmonyPhysics 使用 SKY_DEVELOP=OFF，在设备上加载预编译产品并可见渲染。
- [ ] 5.4 打通华为 SDK、hvigor/ohpm/CMake 配置与 HAP 产物识别，明确编译/兼容 API、应用身份和调试签名配置；验证签名/设备授权不匹配、cook/build/signing 失败不会沿用旧产物。
- [ ] 5.5 最终 HAP 的 11 个 Native 库闭包、STL、80 个 rawfiles、资产索引、模块发现和设备系统启动已由独立审计/真机验证；公开 `validate_harmonyos_hap` 目前只检查 launcher/STL 和两项资源，尚未解析全部 `.so` 的 DT_NEEDED。须将完整依赖闭包校验接入公开 package，并验证缺传递依赖时拒绝产物，不能以人工审计代替这项实现。
- [ ] 5.6 通过公开 create/package 命令生成并安装最小场景；真机显示 mesh/material/texture，验证缺失资源有明确错误且能退出。
- [x] 5.7 按用户要求创建 `examples/HarmonyPhysics`：使用现有 World/Jolt/Renderer 加载 cooked 场景，27 个动态方块、静态地面、固定相机、四个材质和 12 秒重置；公开 package 构建/签名安装、真机碰撞/休眠/重置、一次前后台恢复和最终画面通过。

- [x] 5.8 Shader 缓存、反射、variant 及关联 JSON/材质校验统一使用 END，保持序列化数值；42 项定向 C++ 测试、22 项 Python 契约测试通过，规则加入 AGENTS 及两份编码 skill。

- [x] 5.9 补齐共享 orientation（默认及 default 别名均为 landscape），覆盖 C++/反射/schema、Android/HarmonyOS/iOS 生成、非法值拒绝与项目源文件保护；统一资源准备入口，移除 Android/HarmonyOS Native 的重复部署，鸿蒙签名安装、2848×1276 横屏物理场景与前后台恢复通过。
- [x] 5.10 同一 HarmonyPhysics 工程在小米 23049RAD8C 完成横屏、碰撞/休眠/重置、后台三秒恢复、两轮返回键重开。修复 AssetManager 误销毁进程级处理器；同一 PID 三个会话均可见渲染。NDK 28.1.13356709 / Native API 31、compileSdk 34 / targetSdk 33、minSdk 31。
- [x] 5.11 应用安装标识仅归属平台 export preset，移除 `.skyproj` applicationId；保持 Android Java namespace 独立，56 项 Python、3 项 descriptor C++ 和真实 APK 包名验证通过。

## 6. P5 — 回归与验收

前置：P4；SDK-only、设备运行、签名安装结果分别记录。

- [ ] 6.1 在选定设备执行前后台/旋转/Surface 重建/关闭的重复循环与持续渲染，记录内存、句柄、卡死及异常。6.10 已完成 Host 的 65 秒真实组件压力及 RSS/线程采样；尚未完成旋转、持续 GPU 呈现压力和长期测试，fd 读取被系统拒绝。
- [ ] 6.2 执行错误路径：缺 SDK、错误 ABI、缺库、坏资源、Vulkan 能力不足、初始化中断、待处理回调时退出。6.10 已覆盖 Host create/request/factory 失败、迟到回调及退出；不代表资源/图形失败矩阵全部完成。
- [ ] 6.3 提交前 Python 打包/预设/shader/源码归属、OHOS 平台/打包契约与 Win32 Aurora/Renderer GPU 定向回归已有通过记录，详细阶段数量见 validation；最新 Framework 完整 270 项、公共输入框 17 项、桌面 OHOS 输入策略 13 项通过。本次 Python 组合 96 项有 2 failures/6 errors/5 skipped；8 个 failures/errors 均为已有 native_package_artifacts Windows 夹具限制。排除该模块的可移植 8 模块 68 项通过，不宣称全 Python 通过。移动端完整失败路径和全编辑器回归仍待执行；UIKit 按用户无 Mac 条件 deferred，不把 Python 检查计为 iOS 原生运行。
- [x] 6.4 更新 `docs/architecture/platform-support.md` 与 validation，公布 canonical SDK 三方严格预检、Host 真机/短压力、两端原生 UI/IME 和 Windows 回归证据；区分 iOS deferred、纯正式包恢复状态与未完成的图形/长期验收。
- [ ] 6.5 整理同一 change 的实现+spec+测试供 review；按根 AGENTS 分别取得 commit 与 archive 确认后再执行对应动作。

- [ ] 6.6 后续按模块清理历史具体枚举上限：RHI/FrameGraph/renderer、Jolt/physics、WorldStreaming、GraphDefinition、TransformGizmo；本轮检查位置和业务区间例外见 validation，尚未批量修改。
- [ ] 6.7 定位 Android Adreno VKDBGUTILWARN003 的具体 render-pass 触发条件，检查启动/重开阶段 dequeueBuffer 超时重试频率；不以场景可运行代替图形诊断完成。
- [x] 6.8 鸿蒙 host 的同进程 shutdown→configure 已在 Pura 80 Pro 验证两轮：临时 ArkTS 验收包与正式包的 11 个 Native 库逐一一致，PID 31886 内三次引擎会话均可呈现并继续碰撞/休眠/重置，采集窗口内引擎域 E/F 为 0。验收入口只在忽略的生成目录中，完成后已恢复工程源及正式 HAP；过期回调高压场景仍归 6.1/6.2。
- [x] 6.9 验收用户已确认的 OHOSHost 收拢：实现由现有 Framework platform/ohos 唯一编译，跨 DLL Singleton 稳定 receiver 与 PIMPL 管理会话/worker/VSync/队列/generation/stop；launcher 保留 N-API/XComponent 注册及在 worker 内初始化的 GameApplication 工厂。核验窗口共享 lease 不重复 SDK retain、reference/unreference 使用长期共享 mutex，host 独立于 OHOSPlatform 且由外部 N-API owner Stop/join；新实现 41 项 Python、真实 API 26 Framework/no-undefined 链接、正式签名 HAP 和冷启动/前后台/同进程重开通过。初轮 Windows 240 项，含统一输入框的最终回归更新为 270 项；过期回调注入与短压力结果见 6.10，长期范围保留 6.1/6.2。
- [x] 6.10 最新 API 26 Core/Framework 的独立 Host fixture 在 Pura 80 Pro 完成 30/30 具名断言；另用真实 SDK VSync 和每 600 ms 移除/重建的 XComponent 执行 65 秒压力，48 次接入、6 对前后台、1806 ticks、0 failures，Stop 后 Host NativeWindow 引用 48/48、Application 1/1、VSync 1/1 配平。RSS 从 75092 增至 92744 KiB（约 75 秒采样），稍后 94428 KiB；fd 权限拒绝，不宣称无泄漏。该 fixture 不运行 Renderer，不替代 GPU/present 退休或长期测试；完成后已恢复正式 HAP。
- [x] 6.11 统一原生 UI 验收后，临时共享 probe 源按字节恢复、临时头与注册删除；公开 package 重新生成纯正式 APK/HAP，审计无验收二进制且 ELF 依赖闭包 missing=0。Android 9 个 `.so`、PID 21895、2400×1080；OHOS 11 个 `.so`、80 个 rawfiles 逐字节匹配、PID 41424、2848×1276。两端彩色方块/地面目视确认且 Home 后同 PID 恢复；OHOS 28 bodies/27 contacts、12 秒 reset、采集引擎 E/F=0。中间缺 CLT/三方环境的失败已保留，补全参数后成功，不称首轮全部通过。

### 当前运行前置

- [x] 下载官方 Emulator 26.0.0.402 / HarmonyOS 7.0.0.107（API 26）手机镜像，创建 SkyEngineHarmony26。
- [x] 按用户“现在启用，暂不重启”授权启用 Windows Hypervisor Platform / Virtual Machine Platform，均为 EnablePending。Windows 11 家庭版缺完整 Hyper-V 角色；未执行重启。
- [ ] 用户方便时重启后，实测模拟器是否能启动；记录 guest ABI、Vulkan API/扩展，再构建相应 x86_64 依赖与 HAP。当前 ARM64 包不能作为 x86 模拟器运行证据。

## 7. P6 — 后续可选能力（未排期）

解锁条件：P5 最小场景通过，并明确具体产品需求。每项单独细化验收后实施；不计入首版支持完成条件。

- [ ] 7.1 音频：确认 OHOS 输出后端，接入现有 audio 接口并测试前后台中断/恢复。
- [ ] 7.2 物理与导航：Jolt 真机场景按用户 2026-11-01 的测试工程请求提前进入任务 5.7；Recast 场景仍未排期，两者分别验收。
- [ ] 7.3 网络/Python：按需求验证 ENet/socket 权限、Python/runtime 依赖与生命周期。
- [ ] 7.4 HarmonyOS 多 ABI、多设备/API、性能功耗与正式签名分发；新增设备形态逐项提供实际验收记录。
