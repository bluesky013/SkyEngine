## Context

基线为 `dev_refactor_rhi` 的 `2f8e9dff`（2026-10-31 已 rebase 到 `origin/dev_refactor_rhi`）。该批上游已包含 Linux X11 平台、Android 场景打包与生命周期修复，应复用最新实现。

当前缺口：

| 范围 | 现有入口 | OHOS 接入内容 |
|---|---|---|
| 平台识别 | `engine/core/include/core/platform/Platform.h`、`framework/src/platform/PlatformBase.cpp` | 独立宏/枚举/名称映射；避免进入 Linux 分支 |
| 构建 | `cmake/configuration.cmake`、`cmake/options.cmake`、Core/Framework CMake | `CMAKE_SYSTEM_NAME=OHOS`、移动端裁剪、PIC、三方目录选择 |
| 三方 | `python/third_party.py`、`cmake/thirdparty.json` | OHOS toolchain、ABI/STL/版本、与 Android 相同的三方包集 |
| 平台服务 | `engine/core/platform/unix`、`engine/framework/platform/*` | 线程/信号量复用审计；窗口、日志、文件、时钟和应用生命周期适配 |
| Vulkan | `VulkanInstance`、`VulkanFunctions`、`VulkanSwapChain` | OHOS Surface 扩展、入口加载、创建与呈现能力核验 |
| 宿主/打包 | `engine/templates/launcher/*`、`python/package_project.py` | ArkTS + Native 入口、HAP、资源部署、预设联动 |

用户已确认目标为华为纯血鸿蒙，即原生 HarmonyOS（HarmonyOS NEXT 技术体系）。初始 API 20 预检后，按用户要求切换到官方 Command Line Tools 26.0.0.865、Native SDK 26.0.0.105/API 26。`openharmony/native` 目录及 OHOS API 表示底层技术接口，不把产品目标扩大为开源 OpenHarmony 设备。当前 HAP 兼容 API 设为 26；已在 Pura 80 Pro / HarmonyOS 7.0.0.109（API 26）安装启动，更低运行 API 尚未验证，实际探测见 [validation.md](validation.md)。

## Goals / Non-Goals

**Goals:**

- 先支持原生 HarmonyOS 的 arm64-v8a 设备，从可重复的交叉编译走到签名 HAP 中的真实场景；具体设备形态在验收时锁定。
- 保持 Core / Framework / Aurora 现有职责，复用模块管理、资产加载和 RendererHost。
- 在可用窗口、生命周期、失败退出、资源退休与输入路径上遵循现有显式状态契约。
- 每个里程碑都有可以单独 review 的改动范围和验收记录。

**Non-Goals:**

- 开源 OpenHarmony 设备、32 位、移动端编辑器、GLES、分布式能力。用户追加的 Windows 模拟器验证需要独立 x86_64 构建；不据此承诺 x86 产品支持或 ARM64 NEON/GPU 行为一致。
- 首帧阶段启用全部 feature plugin；音频、物理、网络、Python 等另列后续里程碑。
- 新建 renderer 子体系、把 Aurora 移到 plugin、目录整理或重写 Android/iOS。
- 首轮商店发布；签名安装验收与正式分发分开记录。

## Decisions

### 1. 平台命名与 SDK

工程/导出 preset 使用 `harmonyos`，CMake 使用 SDK 的 `OHOS`，C++ 使用 `SKY_PLATFORM_OHOS` / `PlatformType::HarmonyOS`，三方 bootstrap 使用 `HarmonyOS`。产品名称与底层 ABI 名称在已有解析入口集中映射，未知名称显式报错。`HarmonyOS` 放在末尾的 `UNDEFINED` 前，保持既有有效平台值不变；检查未发现 `UNDEFINED` 数值持久化，已有资产入口使用平台名称。

直接使用华为 HarmonyOS SDK 内的 `openharmony/native/build/cmake/ohos.toolchain.cmake`，SDK 根目录由参数或环境提供；ABI 初始为 `arm64-v8a`，模块统一 `c++_shared`。记录 compile SDK 与最低兼容 API 的区别，暂不承诺低版本设备兼容。构建和三方缓存独立于 Android/Linux；检查既有 bootstrap 的 build/output/cache 命名是否完整区分 SDK、ABI 与配置，禁止复用其它平台二进制。

不借用 Android toolchain：OHOS 的运行库、宿主、资源和打包接口不同。`__OHOS__` 的判断必须在 `__linux__` 前；本机编译器已验证同时定义两者。

用户要求与 Android 使用相同三方库：共用 `thirdparty.json` 的包来源/版本和基础选项，先继承 `options_Android`，再应用必要的 `options_HarmonyOS` 差异。全部 15 个包（包括 Slang、OpenSSL、CPython）纳入移植，不用精简集合代替。构建产物独立位于 `<output>/HarmonyOS`；仅 OHOS SDK 接口和目标编译差异单独适配。包集合对齐、编译安装通过、设备运行通过是三个不同状态。

SDK 统一到 D 盘后，以 `D:/Program Files/Huawei/sdk/default/openharmony/native` 为本次 canonical 输入重新执行公开 third_party 流程，15 包实际构建/安装到 `build/harmonyos-thirdparty/HarmonyOS`，严格预检为 0 missing/stale、0 output-missing。SDK 路径和内容进入缓存身份，不能仅修改旧元数据冒充重编；自定义 OpenSSL/CPython 构建尊重显式 `CMAKE_BUILD_PARALLEL_LEVEL`，本轮设置为 6。严格预检及 ELF/归档检查只证明目标产物闭环，不证明设备 Python import、TLS 或每个三方功能可用。

### 2. 模块归属与装载入口（用户已指示开始实施）

| 代码/入口 | 拟归属 | target 方案 |
|---|---|---|
| OHOS 日志/平台基础适配 | `engine/core`，复用 `platform/unix`；必要的专用代码放 `platform/ohos` | 现有 `Core` |
| OHOSPlatform / OHOSWindow / Rawfile 文件系统 | `engine/framework/platform/ohos` | 现有 `Framework` |
| Vulkan OHOS Surface 扩展检查和创建 | `engine/aurora/rhi/vulkan/platform/ohos`；公共函数表只保留条件声明/装载 | 现有 `Aurora.Vulkan` |
| ArkTS UIAbility / XComponent / Native 入口 | `engine/templates/launcher/harmonyos` → 项目 `native/harmonyos` | 项目侧新增 `Launcher` SHARED，产物 `libsky_launcher.so`，由模板声明的 Native 库名装载 |
| 引擎运行期模块 | 现有 module targets | 首轮建议复用 `SKY_MODULE_PACKAGING=static` 将模块工厂装入 launcher；RHI backend 仍按现有动态装载契约打包 |

静态 module factory 不等于把 Vulkan 后端静态化。实施前核对当前装载器、产物名、HAP native 搜索路径和 transitive `.so` 依赖，保留唯一 Environment 与单一生产源码 owner。launcher 只链接实现 target，不直接重复编译其它模块 `.cpp`。若实际 SDK 装载限制需要改变现有 Aurora target 或入口，更新本方案后再确认。

选择薄 ArkTS 宿主，避免把 Ability/N-API 依赖扩散到引擎接口。对比独立 OHOS engine/plugin：现有平台抽象已足够，另起实现体系会重复职责。

按用户补充的 platform 拆分要求，HiLog 输出放在 `core/platform/ohos/PlatformLog.cpp`，公共 Logger 仅调用私有输出接口，非 OHOS 的 stdio 实现在 `core/platform/common`；OHOS 的线程/信号量明确复用 `platform/unix/async`。Vulkan 的扩展校验和 `OHNativeWindow` Surface 创建放在 `vulkan/platform/ohos`，公共 swapchain 只分派调用并核验 present queue。CMake 为各平台选择源文件，仍由既有 Core、Framework、Aurora.Vulkan 各自唯一编译；不新增 target，也不搬迁其它平台实现。

### 3. 生命周期、线程与平台服务

- UIAbility 提供沙箱路径、ResourceManager 和前后台事件；XComponent 提供 Surface 创建/变化/销毁与触控事件。
- Native 入口检查 N-API 返回值；依赖 JS VM 的对象和引用遵循所属线程约束。事件携带 session / surface generation，过期回调不能访问重建后的对象。
- 首轮单引擎窗口；保留稳定 WindowID。`GetNativeHandle()` 仅在可用期返回 `OHNativeWindow*`，失效先通知渲染宿主停止新 acquire。NativeWindow 的持有/释放必须按选定 SDK 契约验证，并与 GPU/presentation 退休分离。
- 复用已有外部帧回调入口接入 NativeVSync。回调投递到明确的引擎线程，不在 ArkUI 事件回调内运行阻塞 Mainloop；停止循环、收齐工作和完成通知后才销毁应用。前后台与 Surface 可用性共同控制 tick，恢复刷新 frame epoch。
- 资源只读数据经 Rawfile/bundle 文件系统读取，可写数据使用应用沙箱。模块库路径不能从 `/proc/self/exe` 的系统启动进程路径或当前目录推导。
- 指针使用 framebuffer pixels、稳定 contact id 和 primary-touch 映射；取消/失焦/后台释放按下状态。键盘文本与 IME 提交分离；所有不支持的服务以现有显式失败/Unsupported 契约返回。

尽量复用已有 Unix 线程/信号量，但先核对 musl 和 SDK 暴露接口；不能把 Linux X11 或 Android GameActivity 链入 OHOS。

#### 本轮宿主收拢（用户已确认，已完成独立验收）

`OHOSHost` 实现归 `engine/framework/platform/ohos`，由现有 `Framework` target 唯一编译，不新增 target。跨 DLL 稳定 receiver 使用 `sky::Singleton`；PIMPL 持有可替换的会话状态、worker、NativeVSync、事件队列、generation 和 stop/join。迟到回调访问稳定 receiver 并先校验 generation，不持有已销毁的会话对象。

项目 launcher 保留 N-API 导出、XComponent 回调注册及 `GameApplication` 工厂；工厂由 host worker 调用，应用也在该 worker 内初始化。ArkTS/SDK 回调只完成参数转换和事件投递，平台窗口接收共享 native-window lease，不再次调用 SDK retain。所有 native reference/unreference 经同一长期共享 mutex 串行，延迟释放的 lease 也遵守该契约。

`OHOSHost` 独立于 `OHOSPlatform`，不让 Platform 的关闭反向销毁仍在回调或 join 的 host。外部 N-API owner 负责 Stop、收齐并 join worker，再释放 owner；既有 Android/iOS 线程模型不随本轮改变。任务 6.8 保留为收拢前实现的历史证据；6.9 记录新实现构建、正式 HAP 和重开结果，6.10 记录 30 项真机失败/迟到回调断言与 65 秒真实组件压力。该压力 fixture 只运行轻量 Application，不调用 Renderer；不能据此推断 GPU/presentation 退休或长期无泄漏。

#### 统一系统文本输入框

`Platform::ShowTextInputDialog(options, callback)` / `HideTextInputDialog()` 是三端共用入口，保留既有 `StartTextInput` 等窗口文本事件能力。`NativeTextInputOptions` 包含初始文本、placeholder、title、Unicode 标量 `maxLength`（0 为不限）、multiline 和 password；multiline+password 明确拒绝。无效/不支持/忙碌请求返回 false 且不产生回调；已接受请求通过回调完成。

Android 使用 Activity 的原生 Dialog/EditText，HarmonyOS 使用 ArkUI `CustomDialogController` 与 TextInput/TextArea，iOS 使用 UIKit 编辑控制器的 UITextField/UITextView。确认、取消、密码、键盘避让和 marked composition 分别由各平台原生 UI 处理，三端不共用 UI 线程调度实现。OHOS 通过 N-API/TSFN 桥接 UI，Host 的 Dialog 事件在 inactive 跳帧判断之前派发，使后台取消也能在 owner 线程收齐。

Framework 私有 `NativeTextInputState` 是跨 DLL Singleton 稳定 receiver：UI 仅持 token，Post/Fail/RequestCancel 可跨线程入队，回调仅由接受请求的引擎 owner 线程 Dispatch。CHANGED 是已提交文本的完整快照，不包含预编辑且去重相同快照；CONFIRMED/CANCELLED/ERROR 恰有一个终态。终态先释放 active/callback/旧快照再调用用户代码，允许回调内打开新请求。Hide/关闭排空已排队变化和终态；Application Shutdown 先 disable，阻止清理回调重开。

初始文本、元数据和回传快照校验 UTF-8、NUL 与 Unicode 标量长度，禁止先按字节/UTF-16 单元截断。非法 CHANGED 不修改状态；原生控件可能已关闭的非法终态改为 ERROR，携带最后合法快照及 -2，原 ERROR 的非零 nativeError 保留。桥接转换失败使用内部 `Fail(token, nativeError)`，在同一锁内读取包括 queued CHANGED 的最后合法快照并入队 ERROR；旧 token、非法枚举、重复终态仍拒绝。密码和任意输入正文不写日志。

公共状态 17 项与 OHOS 输入策略 13 项为桌面行为测试，不代替原生 UI 验收。Android 隔离 UI 的 9 个终态、OHOS 修复后的 7 个完整流程及中文 IME 已通过，纯正式 APK/HAP 恢复另行记录，不能把临时 probe 的运行等同于正式包已清理。UIKit 源码、owner/runner 和设备测试已提供，但用户明确暂无 Mac，iOS 编译运行 deferred，37 项 Python 检查不能视作 UIKit 编译/运行通过，也不阻塞本轮 OHOS/Android 验收。

### 4. Vulkan 先检查能力再接首帧

使用现有 `Aurora.Vulkan`：编译宏 `VK_USE_PLATFORM_OHOS`、实例扩展 `VK_OHOS_surface`、`PFN_vkCreateSurfaceOHOS` 与 `VkSurfaceCreateInfoOHOS`。实例/设备扩展、函数指针和 present queue 支持都需显式检查，失败必须有可诊断结果。

仓库锁定 `Vulkan-Headers v1.4.348`，本机 SDK 头文件为 `1.4.309`；已核对两者 `VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS` 均为 `1000685000`。正式构建选用一致的 Vulkan 头文件来源，并检查 VMA/库接口，不能把两套头混用或用手写扩展声明掩盖版本缺口。

真实设备必须探测当前 compiler 模式要求的 API/features、timeline semaphore、synchronization2；`VK_KHR_surface` / `VK_KHR_swapchain` 等必需扩展先检查再加入列表。用户在 2026-11-01 明确要求根据支持情况启用扩展并实现兼容路径，maintenance1 改为可选，不切换到旧直接帧协议。

- 支持 surface/swapchain maintenance1 且 feature 为 true 时才启用，创建交换链时同时检查 release 命令。具备完整能力使用原 present fence / image release 路径。
- 无扩展时按 swapchain image 持有 render-finished semaphore，提交等待对应 acquire 信号后才能复用。`RetireFrame` 在 GPU 完成且 CPU 借用退出后释放应用帧槽，但 semaphore 继续由 generation 持有，不把 GPU 完成当成呈现完成。
- 未提交帧在 acquire fence 确认后废弃整个 generation，销毁其未消费 acquire semaphore，触发正常重建；不调用不存在的 image-release，也不提交未初始化图像。重建通过 oldSwapchain 衔接，旧 handle/semaphore 延迟到后继交换链的一次 present 被再次 acquire 并完成 fence 后回收。最多保留 8 个旧 generation，超过预算显式报错；正常 resize 不调用 WaitIdle。
- 最终 close 没有未来 acquire：无扩展路径采用 Khronos 说明的 core WSI 队列排空惯例并检查结果，释放 native 资源；它不是规范提供的独立 present fence 证明，不能宣称显示已完成。具备 maintenance1 的路径继续等待 present fence，正常 resize/close 不再额外全设备 WaitIdle。
- 所有选择和寿命处理在既有 Vulkan backend 内；OHOS platform 仅负责 Surface 接口。共享 RHI 的 retirement 说明及对应 delta 同批更新，维护双路径回归，Windows 实机测试按用户要求最后执行。

依据：[Khronos 信号量复用与关闭限制](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)、[旧交换链延迟回收示例](https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html)。

无 Surface、零尺寸、后台和 resize 都要允许安全跳帧；旧 generation 的输出不能被新帧消费。正常生命周期使用既有 lease/drain/退休协议，终态 teardown 保留完整资源清理。

### 5. 主机构建资产，设备只消费运行产物

通过宿主 AssetTool / ShaderCompiler cook `vulkan` shader 及目标平台的场景、材质、纹理，部署到 HAP rawfile。复用现有资产索引、bundle 和 shader environment 合约，审计 platform 字符串白名单和默认 shader 目标。设备不构建桌面 tools/editor；Slang 三方库与现有 shader target 保持 Android 的配置，host codegen 在构建机执行。feature plugin 的设备场景验收分阶段进行，不因此删减三方包集。

扩展 `package_project.py` 的 create/package 流程和 export preset；新字段同步 C++ 数据、反射、JSON schema 与 Python 读取。首轮尽量复用 bundleId、version、abis、signing 等字段；平台专属参数确认后再新增，UI 使用反射面板。

应用安装身份由各平台 export preset 的 `bundleId` 唯一提供，不读取 `.skyproj` applicationId。
Android Java namespace 属于工程自有 launcher 的 package 声明，生成 manifest/Gradle 时读取它；安装包名变化不重写 Java。
新工程仅在创建时为各 preset 写入初值；旧工程显式将已有身份迁入对应 preset，缺失/非法身份在任何输出改动前失败。

工程以华为 DevEco Studio 的 HarmonyOS 应用模板为准，采用 ArkTS Stage/UIAbility 宿主；HAP 工程包含 AppScope/module 配置、hvigor/ohpm 配置、ArkTS 页面及 Native CMake。SDK、hvigor/ohpm、编译/兼容 API 和签名材料可配置，产品与模块配置需明确选择 HarmonyOS。调试签名、应用身份和设备授权按华为工具链验证；签名密钥/密码保留在外部凭据配置，构建日志不能打印它们。构建失败不得沿用旧 HAP 报告成功。

### 6. 共享移动端方向配置与资源准备边界

`ExportPreset.orientation` 默认 `landscape`，显式 `default` 同样解释为固定横屏；另支持 portrait、auto_rotation、auto_rotation_landscape、auto_rotation_portrait。字段同步到 schema、C++ 反射/读写和 Python 导出，非法值在写入生成目录前拒绝。桌面只接受无窗口行为影响的 landscape/default。

Android 将策略写入生成 manifest 的 activity，HarmonyOS 使用 JSON5 解析工程自有 module.json5，仅覆盖 mainElement 对应 ability 的方向；依赖 json5 固定在 CLI requirements。iOS 从源 plist 生成导出副本，覆盖公共及设备限定的方向数组，通过 SKY_IOS_INFO_PLIST 指向 Launcher 的 MACOSX_BUNDLE_INFO_PLIST。三端都不重写项目 native 源文件；旧 iOS launcher 缺少该变量入口时报告迁移方式，不能静默忽略配置。

Android/HarmonyOS 的 configs、cooked products 和开发态 shader 源文件使用同一 Python generate/package 分支，在 Gradle/Hvigor 启动前准备；两者 Native CMake 都不写 assets/rawfile。Apple package 也使用该公共资源函数，但在 app 编译后收集；导出时强制 SKY_DEPLOY_RUNTIME_RESOURCES=OFF，避免 CMake 再做一次。独立桌面/Apple IDE 构建默认保留部署，经 stage-runtime 调用同一函数，Apple generate 显式恢复 ON。删除项目模板单独的 shader 源部署目标，资源只经过一次公共准备；原生运行时库收集保持各平台已有机制。

Android Studio/DevEco 直接编译消费最近一次生成的资源；资产、配置或 preset 修改后先重新 generate/package。既有项目须将 native/CMakeLists.txt 的资源部署块更新到新模板，否则旧项目不会识别部署开关。统一退役旧 APK/HAP/IPA/ZIP，cook 失败不保留看似本次成功的产物。不靠关闭并行或新增 Hvigor 排序任务掩盖重复资源 owner。

## Risks / Trade-offs

- [低版本兼容范围未验收] → 目前锁定 Pura 80 Pro / API 26；不把 API 20 初始编译预检作为低版本运行证据。
- [Vulkan 必需能力在目标设备缺失] → 首帧前做真实 capability probe；将兼容设计作为显式决策，不能以头文件存在代替支持。
- [回调与渲染线程交错导致旧窗口被访问] → session/generation 校验、按线程投递、持有契约和退休测试一起实施。
- [模块/资源路径沿用桌面假设] → 使用 HAP 内真实 native/rawfile 布局验证，覆盖从非项目 cwd 启动。
- [三方库和 host codegen 混淆] → 保持 Android 同集合/版本；构建机工具与目标库分离，记录每包 ABI/STL/产物。CPython 的 Android pyconfig 不能直接当作 OHOS musl 配置。
- [短压力不能代表长期资源稳定] → 已记录 65 秒 Host 真机压力与 75092→92744 KiB 的 RSS 增长、稍后 94428 KiB；原生引用配平不等于进程无泄漏，旋转、持续 GPU 呈现和长期测试仍须另验收。
- [平台回归覆盖不同] → Windows Framework/FrameworkTest 已重编且完整 270 项通过；UIKit 本轮无 Mac，编译运行 deferred。其它既有定向 GPU 回归与未完成范围分别记录，不把 owner/runner 检查当作原生运行。

## Migration Plan

按 tasks 的 P0–P6 依赖顺序推进。同一阶段的代码、spec 和测试一起 review，不为目录整理扩大范围；现有平台继续作为回归基线。未通过设备能力与生命周期验收前，不将 OHOS 标记为正式支持。

默认预设不切换既有项目平台；新平台关闭时现有 target 的源文件和依赖应保持原行为。回退通过恢复该阶段的平台选择/实现修改完成，避免影响已有资产格式。

提交和 OpenSpec archive 分别遵循根 AGENTS 的确认规则；本次未执行。

## Open Questions

1. 当前设备/API 与签名环境已实测；是否扩展到更低 API / 其它设备，待首台设备通过后确定。
2. Core/Framework/Aurora 归属和 launcher 方案已进入实施；如后续需要改变 target/装载契约，再说明具体差异。
3. 当前设备缺少 surface/swapchain maintenance1，已按用户要求实现第 4 节的 core WSI 兼容路径，真机完成可见清屏与前后台恢复。强制兼容路径已有 Windows 定向回归，Host 重复 Surface 重建已有设备短压力；最终关闭的 core WSI 同步限制、GPU/present 压力和更完整图形失败路径仍保留。

## References

- [OpenHarmony NDK 构建与工具链参数](https://github.com/openharmony/docs/blob/master/en/application-dev/napi/build-with-ndk-overview.md)
- [OpenHarmony NativeWindow / XComponent](https://github.com/openharmony/docs/blob/master/en/application-dev/graphics/native-window-guidelines.md)
- [Khronos VK_OHOS_surface](https://docs.vulkan.org/refpages/latest/refpages/source/VK_OHOS_surface.html)
- [锁定版本 Vulkan-Headers v1.4.348](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/v1.4.348/include/vulkan/vulkan_core.h)

外部资料查阅于 2026-10-31；最终 API 可用性以选定 SDK 和设备为准。
