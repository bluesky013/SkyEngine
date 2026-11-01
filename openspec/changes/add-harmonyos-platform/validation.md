# 集成验证记录

更新：2026-11-01（Asia/Shanghai）。API 26 鸿蒙签名 HAP 已在 Pura 80 Pro 验证横屏 Vulkan 物理场景，同工程 Android APK 已在小米手机验证。SDK 编译、设备基础功能与未完成的长循环/失败路径分别记录；本 change 保持 active，未 archive。

以下按阶段保留历史 SDK、未签名 HAP 和初轮失败记录；各段结论只描述该阶段，当前范围以文末及 tasks.md 为准。

## 产品目标澄清

用户确认：“我需要的是纯血鸿蒙系统适配”。本 change 的产品目标已锁定为华为原生 HarmonyOS（HarmonyOS NEXT 技术体系），对应名称由 `add-openharmony-platform` 调整为 `add-harmonyos-platform`。已有 SDK probe 的路径/符号保留为实际执行证据；SDK 内的 `openharmony/native`、`__OHOS__` 和 Vulkan OHOS 扩展仍是有效的底层技术标识。

用户随后指示“开始集成”，并明确“跟安卓使用的三方库保持一致”“也需要新起 platform”。开始实施时尚未获得具体设备，后续已锁定 Pura 80 Pro / API 26。API 20 是历史预检，当前编译/打包及最低兼容配置使用 API 26。

## Git 基线

- 工作分支：`dev_refactor_rhi`，跟踪 `origin/dev_refactor_rhi`。
- 初始：`60079ac7`，工作区干净。fetch 后差异为本地独有 0、远端新增 15。
- 执行：`git rebase origin/dev_refactor_rhi`，成功且无冲突；结果 `2f8e9dff`。
- SSH 代理原先由 PowerShell 解释 `exec connect` 而失败；仅在该次 fetch 进程指定 Git Bash/SSH 后成功，未修改持久 Git/SSH 配置。
- 未 commit、push 或 archive。

## SDK 预检

| 检查 | 观察结果 |
|---|---|
| SDK 产品元数据 | HarmonyOS 6.0.0，API 20，版本 6.0.0.47 |
| Native SDK | DevEco Studio 安装下 `sdk/default/openharmony/native`，包含 `ohos.toolchain.cmake` |
| 编译器 | OHOS Clang 15.0.4，Windows host |
| 目标宏 | `--target=aarch64-linux-ohos` 同时定义 `__OHOS__=1`、`__linux__=1`、`__aarch64__=1` |
| SDK Vulkan headers | 1.4.309，包含 `vulkan_ohos.h`、`VK_USE_PLATFORM_OHOS`、`PFN_vkCreateSurfaceOHOS` |
| Surface 结构标识 | SDK 和仓库锁定的 Khronos v1.4.348 均为 `VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS = 1000685000` |
| Native 头文件 | XComponent、NativeWindow、NativeVSync、Rawfile、N-API、HiLog、Vulkan、C++20 `<span>`/`<thread>` 编译通过 |
| 交叉链接 | 使用 `--no-undefined` 链接最小共享库成功，其中真实引用 `OH_NativeXComponent_GetXComponentSize` |
| ELF 结果 | ELF64 / DYN / AArch64 |

SDK 本机位置：`D:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/native`。此路径只记录环境证据，不作为工程的固定默认值。

### 工具链版本核对

2026-10-31 重新读取本机二进制和元数据：DevEco Studio `6.0.0.858`；Native SDK `6.0.0.47` / API `20` / Release；`clang++ --version` 为 OHOS Clang `15.0.4`，LLVM revision `bb5cdf0f93bfb5316dd3cc2a80d12ab829e49b43`；SDK 的 `__config` 定义 `_LIBCPP_VERSION 15004`。

用户要求使用最新 SDK 后，实时访问[华为官方下载中心](https://developer.huawei.com/consumer/cn/download/command-line-tools-for-hmos)，页面列出的 Windows Command Line Tools 为 `26.0.0.865`（2026-10-31 发布）。这比搜索索引中的 `26.0.0.821` 更新。用户完成登录并提供官方下载的本机 ZIP，已解压到工程忽略目录 `build/harmonyos-tools/26.0.0.865/command-line-tools`；7-Zip 报告 `Everything is Ok`（100106 个文件）。

- ZIP：`commandline-tools-windows-x64-26.0.0.865.zip`，2722330971 字节。
- 本机 SHA256：`FEDD06DCCFD6134E93CA7727B6D202FCF47C3BC9C3A26A61AE7F2CCB97CF159A`。官方页面复制接口未返回期望值，因此仅记录本机摘要与压缩包完整性检查，不宣称已与官方摘要匹配。
- 新 Native SDK：`sdk/default/openharmony/native`，HarmonyOS `26.0.0` / API `26` / Release，版本 `26.0.0.105`。
- 实际编译器：OHOS Clang `15.0.4`，LLVM revision `00dcd9ccdb860908f60ff11b32239084b0908a08`；`_LIBCPP_VERSION 15004`。主版本不变，厂商修订已经更新。
- C++20 默认配置仍未提供 `<memory_resource>`、string/optional 的 `<=>`；NEON intrinsic 编译成功。
- ranges 默认关闭，但 SDK 自带 `libc++experimental.a`，以 `-fexperimental-library` 编译并链接含 `std::ranges::sort` / `std::views::reverse` 的共享库成功。OHOS CMake 先检查默认支持，再检查并启用此选项；两者都失败时配置明确失败。该选项由 [LLVM 15 libc++ 文档](https://releases.llvm.org/15.0.0/projects/libcxx/docs/UsingLibcxx.html) 定义，不能把默认关闭误写成 SDK 完全没有实现。

新三方输出为 `build/harmonyos-thirdparty-26.0.0.865/HarmonyOS`，独立于旧 SDK 产物；全部 15 包已完成构建/安装并通过 `--check --strict`。新 Core/Framework 完成全部生产对象的编译和 `--no-undefined` 共享库链接，产物 `build/harmonyos26-framework-probe/libHarmonyOSFrameworkProbe.so`。生产 Aurora.Vulkan 和 `build/harmonyos26` 根工程默认 OHOS runtime 目标均编译通过；SIMD 开启，音频/ENet 关闭，Python plugin 保持默认关闭。三方包可构建不代表每个 plugin 已设备验证。

临时产物位于已忽略的 `build/openharmony-preflight/probe.o` 和 `build/openharmony-preflight/libsky_ohos_probe.so`，不纳入提交。预检通过 stdin 提供源码，未新增生产 C++ 文件或 CMake target。

编译使用下列参数（`<native-sdk>` 替换为实际路径）：

```text
clang++ --target=aarch64-linux-ohos --sysroot=<native-sdk>/sysroot
  -std=c++20 -DVK_USE_PLATFORM_OHOS -DVK_NO_PROTOTYPES -fPIC
  -x c++ - -c -o build/openharmony-preflight/probe.o

clang++ --target=aarch64-linux-ohos --sysroot=<native-sdk>/sysroot
  -shared -Wl,--no-undefined build/openharmony-preflight/probe.o
  -o build/openharmony-preflight/libsky_ohos_probe.so
  -lace_ndk.z -lnative_window -lnative_vsync -lrawfile.z -lace_napi.z -lhilog_ndk.z

llvm-readelf -h -d build/openharmony-preflight/libsky_ohos_probe.so
```

stdin 源码：

```cpp
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include <native_vsync/native_vsync.h>
#include <rawfile/raw_file_manager.h>
#include <napi/native_api.h>
#include <hilog/log.h>
#include <vulkan/vulkan.h>
#include <thread>
#include <span>
#ifndef __OHOS__
#error Expected an OpenHarmony compiler target
#endif
static_assert(sizeof(void *) == 8);
static_assert(VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS == 1000685000);
extern "C" __attribute__((visibility("default"))) int SkyOpenHarmonyProbe(OH_NativeXComponent *component, OHNativeWindow *window)
{
    uint64_t width = 0;
    uint64_t height = 0;
    PFN_vkCreateSurfaceOHOS createSurface = nullptr;
    (void)createSurface;
    return OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
}
```

动态依赖检查列出 `libace_ndk.z.so`、`libnative_window.so`、`libnative_vsync.so`、`librawfile.z.so`、`libace_napi.z.so`、`libhilog_ndk.z.so`、`libc++_shared.so`、`libc.so`。这些是 SDK stub 的链接证据，不代表对应函数在目标设备上已运行。

首轮编译曾因 PowerShell 参数解析和沙箱临时目录对象文件重命名失败；改为参数数组、显式对象文件输出后编译和链接通过。未因此修改 SDK、系统权限或持久环境配置。

## P1 实施与可重复验证

- 新增 `HarmonyOS` bootstrap、`harmonyos` CMake preset、`SKY_PLATFORM_OHOS` 和 `PlatformType::HarmonyOS`；保留已有有效平台值，`UNDEFINED` 保持末尾。代码检索未发现 sentinel 的数值持久化；平台相关资产入口使用字符串。
- Android 与 HarmonyOS 共享 15 个包和版本；先继承 Android 包选项，再应用 HarmonyOS 专用覆盖。输出使用 `<output>/HarmonyOS`，构建目录/依赖 key 包含 SDK、arm64 ABI 和 shared STL 身份。
- CMake 拒绝错误 ABI、STL、其它平台的缓存/依赖元数据；不会读取 legacy 平台不明的 cache。
- Core 使用现有 Unix 线程/信号量，日志接入 HiLog。修复 SDK libc++ 下 `AccessKey`/`DependencyEdge` 默认比较需显式 `std::strong_ordering`、`ScalerVisit` 返回类型缺 `typename` 的编译问题，保持既有比较顺序和算法。
- `python/tests/fixtures/harmonyos_core` 复用唯一生产 `Core` target，通过 whole-archive + no-undefined 检查其全部对象。此 fixture 是 Core 编译/链接验收入口，不代表根工程、Framework 或 HAP 已通过。

```powershell
$env:OHOS_SDK_NATIVE = '<native-sdk>'
$env:PATH = "$env:OHOS_SDK_NATIVE/build-tools/cmake/bin;" + $env:PATH
python python/third_party.py -p HarmonyOS --ohos-sdk $env:OHOS_SDK_NATIVE -o <absolute-output> -j 8

cmake -S python/tests/fixtures/harmonyos_core -B build/harmonyos-core-probe -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$env:OHOS_SDK_NATIVE/build/cmake/ohos.toolchain.cmake" `
  '-D3RD_PATH=<absolute-output>/HarmonyOS' -DOHOS_ARCH=arm64-v8a `
  -DOHOS_STL=c++_shared -DSKY_MATH_SIMD=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/harmonyos-core-probe --parallel 8
```

本机实测输出目录为 `build/harmonyos-thirdparty`。Core Debug/NEON、Release/scalar 两种构建和全对象链接通过；后者构建目录为 `build/harmonyos-core-scalar`。`llvm-readelf -h -d build/harmonyos-core-probe/libHarmonyOSCoreProbe.so` 显示 ELF64 / DYN / AArch64，动态依赖为 `libhilog_ndk.z.so`、`libc++_shared.so`、`libc.so`。编译时检查 OHOS 不落入 Android/Linux，并检查 SIMD 选择 NEON。

测试：`python python/tests/test_harmonyos_platform.py`（6 个，通过）；`python python/tests/test_cmake_source_ownership.py`（26 个，通过）。沙箱临时目录权限导致的首次 Python 失败，改用工作区 `build/test-temp` 后通过，未改变测试判断。

三方包的逐项构建结果与宿主回归结果在下方记录；不能用已完成的 Core 链接代替完整三方或引擎支持声明。

### Windows 回归

使用既有 `build` 缓存（VS 2022，`3RD_PATH=D:/Code/Engine/sky_3rd/Win32`），关闭编辑器后重新配置并构建 `CoreTest Framework`，通过。首次沙箱构建出现 MSVC `C1902` PDB 服务错误；在正常工具链环境重试后通过。日志为 `build/harmonyos-win32-configure.log`、`build/harmonyos-win32-build.log`。

`output/bin/Debug/CoreTest.exe`：315 项中 314 通过、1 跳过；跳过项为 `FileScanTest.InternalFileSymlinkRetainsItsLogicalSourcePath`，原因是当前进程没有创建符号链接权限。日志为 `build/harmonyos-coretest.log`。未宣称 Windows 全引擎或 Android/iOS 回归完成。

## API 26 全量编译与 HAP

- Android 同集合/版本共 15 包：SFMT 1.5.4、RapidJSON 1.1.0、GoogleTest 1.17.0、VulkanHeaders 1.4.348、VMA 3.2.1、Recast 1.6.0、Jolt 5.6.0、LZ4 1.10.0、zlib 1.3.1、Tracy 0.11.1、FreeType 2.13.3、Slang 2026.19、ACL 2.1.0、OpenSSL 3.0.18、CPython 3.13.9。
- Header 包按配置/安装计；普通二进制包完成 Debug/Release，OpenSSL/CPython 自定义 builder 为 Release。CPython 目标 archive 含 209 个对象，stdlib ZIP 已安装。日志：`build/harmonyos-tools/rebuild26-final.log`、`openssl26-build.log`、`cpython26-build.log`。
- Core/Framework fixture 与 Aurora.Vulkan fixture 复用生产 target，完成 no-undefined 链接。根工程配置 `cmake --preset harmonyos -B build/harmonyos26 -D3RD_PATH=<new-output>/HarmonyOS -DSKY_MATH_SIMD=ON`，默认目标全部通过；日志 `build/harmonyos-tools/engine26-build.log`。
- SDK Clang 15 编译暴露的兼容问题均局部修复：dependent typename、结构化绑定显式 capture、aggregate 经 make_shared/make_unique 构造、显式比较返回类型。保持算法和数据字段语义。
- HarmonyOS shader platform 使用独立值 6，Android=4/Linux=5 不变；Vulkan shader environment、序列化上界及 Python cooker 映射同步，12 个 shader Python 测试通过。
- 通过公开 `create` 创建 `build/harmonyos-launcher-probe/HarmonyProbe`，使用工程自有 launcher、生成的 Hvigor API 26 工程和真实引擎 target 构建 Debug 开发 HAP（`SKY_DEVELOP=ON`）。ArkTS 与 N-API/XComponent/NativeVSync 的 ARM64 Launcher 均编译，Launcher 使用 no-undefined 链接。HAP 路径：`build/harmonyos-launcher-probe/HarmonyProbe/build/harmonyos/entry/build/default/outputs/default/entry-default-unsigned.hap`。
- HAP 大小 246156959 字节，含 21 个 AArch64 `.so`（Launcher、游戏模块、Aurora.Vulkan、共享 STL 等），以及 `resources/rawfile/configs/modules_game.json` 和 `products/bundles.json`。ELF 的 DT_NEEDED 全部在 HAP 或 SDK system stub 库名集合中解析；缺失 0，错误架构 0。审计：`build/harmonyos-tools/hap-elf-audit/report.json`。该静态检查不能代替设备 linker/namespace 验收。
- Hvigor 要求 `src/main/cpp` 被识别为 Native source set；只设置外部 CMake path 会跳过构建并生成无 `.so` 的包，已通过薄 CMake 入口接入工程 `native/`。公开 package 现在拒绝缺 Launcher/STL、错误 ELF 架构或缺必要 Rawfile 的 HAP。
- 当前 HAP 使用开发资源部署，未运行完整 shipping cook/package；宿主 AssetTool/ShaderCookTool 的 HarmonyOS 更新/回归按用户要求留到 OHOS 工作收尾。不得把此包标成 cooked 场景验收通过。
- 已实调现有 `package_project.py` 的 HarmonyOS 构建/校验/收集函数，OHPM 与 Hvigor 成功，并生成 `build/harmonyos-launcher-probe/HarmonyProbe/build/harmonyos/HarmonyProbe-debug-unsigned.hap`；日志 `build/harmonyos-tools/hap-package.log`。这验证打包后半段，不代替上条未执行的完整 cook 链路。
- 非阻断警告：Hvigor 的 strip 会碰到 Jolt 模块旁的 `licenses/Jolt.txt`，提示不是对象文件；HAP 构建成功，许可证也在 Rawfile 中。没有伪造签名或签名安装结果。

本次 Python 验证：HarmonyOS 包测试 4、平台配置 8、create smoke 4、preset schema 2、shader 12 通过。早前 HarmonyOS 构建契约 6 与源码归属 26 通过。`test_native_package_artifacts.py` 在 Windows 上有 2 failures/6 errors/5 skips，涉及其 POSIX 路径、可执行位及 symlink 前提；未更改无关 Apple/Android 实现以适配测试宿主，也未宣称全套通过。Windows C++ 回归仍按用户要求最后执行。

## 模拟器与主机状态

- 官方 Emulator `26.0.0.402 Release`；下载 HarmonyOS `7.0.0(26.0.0)` 手机镜像，镜像软件版本 `7.0.0.107`，元数据 ABI 为 `x86`；实际 guest ABI 尚待启动查询。
- 镜像目录 `build/harmonyos-tools/emulator-images/system-image/HarmonyOS-7.0.0/phone_all_x86`；实例 `SkyEngineHarmony26`，位于 `build/harmonyos-tools/emulator-instances`，4 GB RAM / 12 GB storage。
- 首次启动报 `00801001`（未开启 Hyper-V），新版 HDC 尚无设备。主机为 Windows 11 家庭版中文版 10.0.26200，CPU i7-12700KF，BIOS 虚拟化和 SLAT 均开启。
- 用户明确授权“现在启用，暂不重启”。完整 `Microsoft-Hyper-V` 角色因家庭版不存在而启用失败；随后使用系统自带的 `HypervisorPlatform` 与 `VirtualMachinePlatform`，两项成功进入 EnablePending / RestartNeeded=true。未重启、未升级 Windows edition、未安装第三方补丁。日志 `hypervisor-platform-result.json` / `hypervisor-after.json` 位于 `build/harmonyos-tools`。
- 重启后仍需实测模拟器能否启动；不能把上述组件启用等同于模拟器已运行。Windows x86 镜像还需独立 ABI 的三方和 HAP 构建；ARM64/NEON、真实 GPU 驱动验收仍需真机。

## 待验证边界

- Framework 的窗口/生命周期/Rawfile 和 Aurora OHOS Surface 已有实现及 SDK 编译证据；N-API 资源生命周期、Surface 重建、前后台时钟、VSync 排空和 GPU 退休均未在设备执行。
- primary-touch、鼠标、键盘与 committed text/IME 尚未接通；不能宣称 P2 完成。
- 调试签名安装与启动已通过；Vulkan 交换链已创建，但设备缺少 checked presentation 所需 maintenance1 扩展。无限重建已修复为明确 UNSUPPORTED / 停止，clear/triangle/cooked 场景未验收。
- 本轮验收真机/API 已锁定如下，更低 API 兼容性未验证；Tracy profiling、线程/日志/NEON、Python/OpenSSL 的设备运行未验收。

## 2026-10-31 真机连接检查

- HDC 3.2.0f 报告 USB Connected，调试授权有效。系统参数返回 HUAWEI Pura 80 Pro / LMR-AL00，软件版本 `LMR-AL00 7.0.0.109(SP6C00E105R7P5)`，API 26，ABI arm64-v8a；内核输出 aarch64 / HongMeng Kernel 1.13.0。当前 ARM64/API 26 HAP 与这两个安装前提匹配。
- 系统 RenderService 的 `gles` 诊断报告 GPU 为 HUAWEI Maleoon 920、OpenGL ES 3.2；`vktextureLimit` 返回 16384 × 16384。该系统诊断不是 Aurora 的 Vulkan feature query，不能据此勾选 timeline/synchronization2/maintenance1 或首帧验收。
- 用最新 SDK 编译独立 ARM64 C 探针（仅依赖 libc/libvulkan，查询 Vulkan 并执行一次 NEON 加法），HDC 成功发送到任务专用临时路径，但执行被手机拒绝（Permission denied）；没有取得 Vulkan 特性或 NEON 运行结果，未调整设备安全设置。
- 使用 `hdc install` 安装本任务的 `HarmonyProbe-debug-unsigned.hap`，设备返回 `9568320: no signature file`。安装没有成功。日志 `build/harmonyos-tools/device-install.log`；GPU 诊断日志 `device-gpu-system.log`。
- 本地 DevEco 旧 debug profile 的 bundle 为 `com.example.testhm`，有效期截止 2026-04-07 00:55:02（Asia/Shanghai），不适用于当前 `com.skyengine.harmonyprobe`。仅检查 profile 的 bundle/有效期，没有读取或复制私钥材料；需要新的有效调试签名和对应设备授权。
- 按用户要求把新版 SDK 的 `openharmony/toolchains` 目录加入 Windows 当前用户 PATH：保留原条目、确认只有一项对应目录，并广播 Environment 更新。HDC 实测版本 3.2.0f；已有终端/IDE 可能需要重新启动才能继承新环境。

## 2026-10-31 调试签名与统一工具目录

- 用户在 DevEco Studio 26.0.0.865 配置新调试签名；Profile 对应 `com.skyengine.harmonyprobe`、一台授权设备，有效时间戳 `1793456673` 至 `1824992673`。补齐 `products.default.signingConfig=default`，将材料配置保存到探针工程被 Git 忽略的 `export_credentials.json`，导出 preset 使用 `signing=auto`；未将密码或私钥写入日志或受版本控制文件。
- 官方命令行工具完成 `SignHap`，输出 `HarmonyProbe-debug-signed.hap`，HDC 返回 `install bundle successfully` 和 `start ability successfully`。首次进程 PID 20822；日志 `build/harmonyos-tools/hap-sign-package.log`、`device-signed-install.log`、`device-start.log`。
- 首次进程日志确认 Vulkan swapchain 为 1276×2617、format=37、4 images、FIFO；随后持续重建，不能记为首帧通过。已停止应用，日志 `device-app-startup.log`。手机 `snapshot_display` 只接受 `.jpeg`，首次使用 `.png` 失败，没有取得截图。
- 按用户要求统一到 `D:/Program Files/Huawei/`：IDE 为 `DevEco Studio`、命令行工具为 `command-line-tools`、共享 SDK 为 `sdk`，版本均匹配 26.0.0.865 / SDK 26.0.0.105。命令行工具的 `sdk` 子目录及工程内旧工具路径保留 junction；SDK 元数据、Clang 与 HDC 的迁移前后 SHA-256 一致。
- IDE 安装目录由管理员拥有，原 SDK 保留；通过用户 `DevEcoStudio26.0/idea.properties` 的 `ide.harmonyos.sdk.location` 指向共享目录。用户关闭 IDE 后已重开，新的 HDC 进程实际来自共享 SDK，USB 设备连接正常。
- 当前用户环境变量 `HARMONYOS_COMMAND_LINE_TOOLS`、`DEVECO_SDK_HOME`、`OHOS_SDK_NATIVE` 和 hdc PATH 已更新。迁移记录在 `build/harmonyos-tools/sdk-relocation.json`；旧终端可能需要重开。三方产物仍为先前实编结果，本次目录迁移不代表重新编译全部三方。

## 2026-10-31 platform 拆分与真机能力失败路径

- Core 的 HiLog 实现独立在 `platform/ohos/PlatformLog.cpp`，公共 Logger 只调用私有 `WritePlatformLog`；非 OHOS 选择 `platform/common` 的 stdio 实现。OHOS 继续复用 Unix async，未把其它 Unix 服务整体当作 OHOS 平台。
- Vulkan 的 OHOS 扩展检查和 Surface 创建移入 `platform/ohos/VulkanSurface.cpp`，仍由现有 Aurora.Vulkan target 编译；公共函数表保留条件声明/加载。Framework 的窗口、Rawfile、路径和生命周期位于 `platform/ohos`，ArkTS/N-API 仍在 launcher 模板。
- 使用共享 SDK 构建签名 HAP 成功，最终增量构建耗时 33 秒；实际 Ninja 图确认 Core 唯一编译 OHOS 日志实现，Aurora.Vulkan 唯一编译 OHOS Surface 实现。日志 `build/harmonyos-tools/hap-platform-split.log`、`hap-platform-split-final.log`。
- `python -m unittest python.tests.test_harmonyos_platform python.tests.test_harmonyos_package python.tests.test_cmake_source_ownership`：36 tests / OK。首次运行因 sandbox 临时目录 ACL 失败，改用工作区 `build/harmonyos-tools/test-temp` 后通过，未改测试断言。日志 `platform-split-tests.log`。Windows C++ 构建仍未执行。
- Pura 80 Pro 的 Vulkan loader 和 physical device 均报告 1.3.309，GPU 为 Maleoon 920，生产 Renderer frame context 初始化成功。实例列表缺少 `VK_EXT_surface_maintenance1`，设备列表缺少 `VK_EXT_swapchain_maintenance1`；设备的 `VK_KHR_maintenance1` 是不同扩展，不能代替前述呈现能力。当前 checked SwapChain 依赖 present fence 与 acquired-image release，因此首帧受阻。
- 原先 QuerySurface 返回 UNSUPPORTED 后，Renderer 将其按普通窗口不可用重试，持续创建交换链。已让 UNSUPPORTED 成为终态，保留 native 错误码 `VK_ERROR_FEATURE_NOT_PRESENT (-8)`；终态不再创建输出，仍执行原有 shutdown/收尾。launcher 对 STOPPED 显示停止状态，避免无提示黑屏。
- 最终签名 HAP 安装/启动成功，PID 63476 的日志只有一次 `swapchain created`、一次明确能力错误，随后 `renderer host failed` / `Renderer host stopped`。日志 `device-platform-split-final-install.log`、`device-platform-split-final-start.log`、`device-platform-split-final.log`。前一轮 PID 61125 的截图 `device-platform-split.jpeg` 已目视确认显示 `Engine stopped. See device logs.`。测试后 force-stop 本应用；未更改手机锁屏/调试设置。
- SDK 迁移后的该次只读 `--check --strict` 报告 15 stale / 0 output-missing，原因是现有缓存 key 包含 SDK 绝对路径；迁移前后 SDK 内容校验一致，且相同三方产物参与了上述真实 HAP 链接。未改元数据冒充重编，当时新路径重建保留为任务 2.9；后续已实际重建全部 15 包并严格预检清零，见文末 canonical SDK 闭环。历史日志 `thirdparty-after-relocation.log`。
- 此轮证明平台分离后仍可构建、安装、调用 HiLog/Surface，并在能力不足时停止，不代表 clear/triangle、持续渲染、输入或生命周期恢复验收完成。

## 2026-11-01 真机复测

- 按用户“再次测试”要求，从当前源码重新构建调试签名 HAP，Hvigor 24 秒完成；安装与启动均成功。本轮未增加无 maintenance1 的兼容实现，复测的是既有能力检查和退出路径。
- Pura 80 Pro 进程 PID 16169，Maleoon 920 / Vulkan 1.3.309。启动后观察 10 秒：交换链创建 1 次、缺 maintenance1 日志 1 次、UNSUPPORTED renderer failure 1 次、Renderer stopped 1 次，无持续重建。截图目视确认显示 `Engine stopped. See device logs.`，首帧仍未通过。
- 记录位于 `build/harmonyos-tools/hap-retest-20261101.log`、`device-retest-20261101{,-install,-start}.log`、`device-retest-20261101.jpeg`；`device-retest-20261101-summary.json` 保存本轮 HAP SHA-256、UTC 时间和上述计数。测试后已 force-stop 本应用。

## 2026-11-01 maintenance1 可选协商与 core WSI 真机呈现

- 按用户要求实现能力选择：先检查依赖和扩展名，再查询 `swapchainMaintenance1` feature，满足后才加入设备扩展列表；没有该能力时采用实际的 core WSI 路径，不伪造 feature 或调用 EXT 入口。
- core WSI 的 render-finished semaphore 按交换链图像持有；应用帧退休不销毁仍可能被 presentation engine 使用的资源。resize 保留旧 generation，后续 generation 的已呈现图像再次 acquire 完成后回收旧资源；放弃未呈现图像使 generation 失效。resize 能力查询失败保留现有交换链；未回收 generation 上限为 8，超限显式失败。
- 无后续 acquire 的最终关闭采用队列排空惯例。core WSI 无独立的最后一次 present 完成证明，此路径不冒充 maintenance1 present fence，也不宣称 GPU 完成等于屏幕显示完成。正常 resize 不调用全设备 WaitIdle。
- 最终 HAP 构建/签名通过，日志 `build/harmonyos-tools/hap-core-presentation-final.log`（Hvigor 20.928 秒）。本地产物修改时间为 00:31:27，设备 bundle 的 updateTime 为 `1793464298774`（00:31:38.774）；之后两次启动成功。签名材料保持在忽略目录。
- 使用工程自有、Git 忽略的 HarmonyProbeModule 通过现有 `ILauncherSceneSession` / `IFrameGraphFeature` / RendererHost 提交蓝绿交替 clear pass。每次实际 GPU completion 回调达到 600 帧后请求引擎正常退出；它是诊断工程，不是已验收的 cooked 场景或 triangle。
- 同一诊断模块调用生产能力协商 helper，执行三项 gtest：扩展/依赖缺失时不查询或启用、扩展存在但 feature=false 时不启用、feature=true 时启用。两轮日志均为 `Vulkan capability tests: 3 passed, 0 failed`。这些是协商单测，不表示设备实际支持 maintenance1。
- Pura 80 Pro / HarmonyOS 7.0.0.109 / API 26 / Maleoon 920 / Vulkan 1.3.309，实际选择 `core swapchain per-image semaphores`，创建 1276×2617、4 images、FIFO 交换链。首轮 PID 38556 从 00:34:26 运行到 00:34:37，记录 120/600 帧 GPU 完成及正常 Renderer stopped。
- 第二轮 PID 39233 从 00:34:52 运行，00:34:56 收到 onBackground、00:34:58 收到 onForeground，恢复后继续呈现，00:35:05 达到 600 帧并正常销毁 Vulkan device/instance、停止 Renderer。全程同一 PID，GameApplication 和 HarmonyProbeModule 仅初始化一次；未看到引擎 A00000 域的 E/F 日志。测试结束后才 force-stop ArkUI 应用进程。
- 已目视核对蓝色、绿色和恢复后的蓝色画面：`build/harmonyos-tools/device-clear-{a,b,resume}.jpeg`。日志为 `device-clear-presentation.log` 与 `device-clear-lifecycle.log`；产物 SHA-256、计数和范围记录在 `device-clear-summary.json`。截图来自实际 Vulkan 清屏，不是 ArkUI 模拟画面。
- 设备未提供 Vulkan validation layer；驱动仍输出 loader interface、Mapper 符号及退出时 `base munmap` / `process destroy 4g fail` 警告。此次未出现引擎失败或卡死，但不能据此宣称 validation-layer clean 或长期资源稳定性通过。
- 已补仓库内协商测试，以及强制 core WSI 下逐图像 semaphore 复用、旧 generation 延迟销毁、放弃帧、resize 查询失败的原生回归代码。Windows 原生测试尚未运行，按用户要求留到 OHOS 后统一回归；triangle/上传、输入/IME、重复 resize/Surface 重建与压力验收仍未完成。

## 2026-11-01 HarmonyPhysics 测试工程与真机物理场景

- 用户要求创建可在手机运行的物理测试工程。通过公开 `package_project.py create --dir examples --name HarmonyPhysics` 生成独立工程，源文件保留在 `examples/HarmonyPhysics`，生成物、缓存和签名凭据由工程 `.gitignore` 排除。复用现有调试 profile 的 `com.skyengine.harmonyprobe` 身份，安装覆盖先前 HarmonyProbe；私钥/密码未入库。
- 场景 `assets/scenes/physics_playground.world` 含 31 个 actor：27 个动态立方体、静态地面、方向光、点光和相机。使用现有 World schema、Physics subsystem、JoltPhysicsModule 和 Aurora 默认 World 渲染入口，没有新增引擎/插件 target。工程模块仅依赖 Framework/Physics 接口，通过 IPhysicsSystem 获取后端，保存初始动态刚体快照并每 12 秒恢复。四个 `.mat` 资产和稳定的 `assets.jsonl` ID 区分地面及橙/蓝/绿三层方块；演示关闭阴影，不把阴影质量列入验收。
- 因当前宿主工具旧于源码，配置现有标准位置 `cmake-build-debug`（此前该目录不存在；VS 2022、`D:/Code/Engine/sky_3rd/Win32`），仅构建此次打包需要的 AssetTool/ShaderCookTool 及真实依赖。修复 AssetTool 在 `SKY_BUILD_AUDIO/TERRAIN=OFF` 时仍尝试加载遗留 builder DLL 的问题；启用的 builder 仍按原规则加载并检查失败。补全模板 `asset_cook.jsonc` 的 HarmonyOS preset/distribution。
- 通过公开 `python python/package_project.py package -p examples/HarmonyPhysics --preset HarmonyPhysics-harmonyos` 完成资源 cook、shader cook、Hvigor 编译及签名。使用前述共享 API 26 SDK，`SKY_3RD_PATH=build/harmonyos-thirdparty-26.0.0.865/HarmonyOS`，`JAVA_HOME` 指向 DevEco Studio 的 jbr；目标为 ARM64、Debug、SKY_DEVELOP=OFF、NEON 开启。
- 最终 shader cook 为 56 条；HAP 包含预编译 Vulkan `.bin` shader 产品而非散装 `.spv` 文件，11 个 `.so` 均通过 ELF64/AArch64 检查。当前 HAP 为 `examples/HarmonyPhysics/build/harmonyos/HarmonyPhysics-debug-signed.hap`，202,570,273 bytes；SHA-256 与验收计数在 `build/harmonyos-tools/physics-summary.json`。
- 首次完整 Native 构建的 Hvigor 为 88.619 秒，最终资源/签名增量为 16.183 秒。首次 DoNativeStrip 对同目录的 `licenses/Jolt.txt` 报非 object 警告，许可证仍保留，构建/签名/安装均成功；未将该 SDK 警告称为编译错误。日志 `physics-host-{configure,build,rebuild}.log`、`physics-package{,-final}.log`。
- Pura 80 Pro / HarmonyOS 7.0.0.109 / API 26 / Maleoon 920 / Vulkan 1.3.309：首次运行 PID 51360，最终配色运行 PID 53989。两轮都报告 `Default World ready ... (31 actors)`、27 个动态刚体与 1 个静态刚体；第一秒采样的底层方块中心 Y 为约 0.42–0.45，落稳后约 0.430（初始 3.5），接触数达到 27，activeBodyCount 从 27 降到 0。12 秒恢复快照后 activeBodyCount 再次为 27，随后重新落稳。
- 最终运行在同一 PID 完成一次 Home/返回，日志各记录一次 onBackground/恢复 onForeground，模块初始化仅一次，恢复后继续推进模拟并执行重置。最终观测日志中引擎 A00000 域 E/F 数为 0。测试未改手机锁屏设置，也未在结束时主动关闭演示应用。
- 已目视验证 `build/harmonyos-tools/physics-start.jpeg` / `physics-settled.jpeg` 的下落与堆叠；最终配色及恢复截图为 `physics-final.jpeg` / `physics-resume.jpeg`。对应日志 `physics-device-first.log` / `physics-device-final.log`。这是实际 Jolt 模拟和 Vulkan mesh 呈现，ArkTS 仅叠加说明文字。
- `python -m unittest python.tests.test_cmake_source_ownership python.tests.test_harmonyos_package`：30 tests / OK，记录 `physics-contract-tests.log`。完整 Windows 原生回归、纹理资产的专门场景、缺资源失败路径、输入/IME、重复 Surface 重建与长时间压力测试仍未完成。

## 2026-11-01 ShaderPlatform 显式边界

- 按用户 review 意见在 `ShaderPlatform` 末尾增加 `END`，保留已有平台值（LINUX=5、HARMONYOS=6）。缓存 Reader 对带 END 的枚举使用自身边界，环境归一化同步拒绝 END 及更大值，不再拿具体平台名表示范围上限。
- 新增 Linux/HarmonyOS 缓存往返与 END/END+1/UINT32_MAX 拒绝测试，检查失败不覆盖原输出。现有 VS 2022 配置构建 `Aurora.CoreTest` 后，定向执行 `ShaderCacheStoreTest.*`，8 tests / PASSED；日志 `build/harmonyos-tools/shader-platform-end-{configure,build,tests}.log`。本次只验证平台无关的缓存逻辑，不代表完整 Windows 平台回归已执行。

## 2026-11-01 枚举边界审查与游戏全屏

- 继 ShaderPlatform 后，再为同一 shader 读取链的 17 个连续枚举追加 END，原数值不变。Reader 统一先校验 uint32 原值再转型，去掉接受具体最大枚举值的重载；ShaderArtifact/Resolver/Usage/Variant 与 MaterialAsset/MaterialReflection 的边界同步更新。vertex scalar 的 BOOL 拒绝规则、ShaderStage 位标志校验保持独立。
- AGENTS.md、OpenCode/Copilot 的 skyengine-coding-rules 加入枚举边界规则和 review 清单：连续枚举用排他 END/COUNT，稀疏枚举用白名单，flags 用合法掩码；未知值、支持子集、窄化和序列化兼容分别检查。这是 agent review 规则，不宣称已新增通用 C++ 静态检查器。
- 现有 VS 2022 构建 Aurora.CoreTest、AssetTool、ShaderCookTool 成功。42 项 ShaderCacheStore/Resolver/Usage/Companion/Material 定向测试通过；包括重算 fingerprint 后的 END/END+1/底层整数最大值拒绝、schema ownership/type、Linux/HarmonyOS 往返及 vertex BOOL 子集。22 项 Python HarmonyOS package/platform/shader-project 测试通过。日志：enum-bounds-build.log、enum-bounds-tests-build.log、enum-bounds-tests.log、fullscreen-package-tests.log（均在 build/harmonyos-tools）。完整 Windows/GPU 回归仍待最终统一执行。
- 非全屏的原因在鸿蒙 EntryAbility：原实现直接 loadContent，XComponent 的 100% 只覆盖系统内容区，GameApplication 本身接收已有 OHNativeWindow。模板和 HarmonyPhysics 源入口现在先设置 setWindowLayoutFullScreen(true)、setWindowSystemBarEnable([])、setSpecificSystemBarEnabled('navigationIndicator', false)，成功后加载页面；失败显式记录并显示错误，stage 销毁后忽略异步完成。平台逻辑仍留在鸿蒙 launcher。
- 公开 package 命令使用 API 26 SDK 完成 Native/ArkTS 编译和签名，日志 physics-fullscreen-package.log。HAP 已更新安装到 Pura 80 Pro；首次安装后因锁屏拒绝启动，用户解锁后于 01:17:51 启动 PID 6872。交换链实测由旧内容区 1276×2617 扩展为 1276×2848，与屏幕相同，WindowManager 窗口起点为 (0,0)，截图确认系统栏/手势条不可见。
- 27 动态方块/静态地面的物理堆叠、休眠、12 秒重置继续正常；一次 Home→恢复后仍为 PID 6872，module init 仅一次，记录到第 5 次重置。未发现该进程 A00000 域的 E/F 错误。证据：physics-fullscreen-device.log、physics-fullscreen-resume.log、physics-fullscreen-window.txt、physics-fullscreen.jpeg、physics-fullscreen-resume.jpeg。
- 横竖屏是独立的 launcher 配置：项目源 native/harmonyos/entry/src/main/module.json5 的 EntryAbility.orientation；landscape 固定横屏，auto_rotation_landscape 支持两个横向方向。用户本轮询问配置位置，当前工程仍保持未指定方向，未改成横屏。

### 历史同类写法（审查结果，待分模块处理）

| 范围 | 已发现位置/例子 |
|---|---|
| RHI / FrameGraph | rhi/interface/src/Instance.cpp:38（API::DX12）；rhi/common/FrameContext.cpp:194（VulkanFrameGraphMode::AUTO）；FrameBindings.cpp、FrameGraphAnalysis.cpp、FrozenFrameGraph.h 的布局、访问、状态边界 |
| Renderer | RendererProvider.cpp（RendererVulkanMode/API）；ResourceCatalog.cpp（ResourceBindingSlot::MATERIAL）；RenderCommands.h（RenderLightKind::SPOT）；RenderRasterData.h（Filter/MipFilter/WrapMode） |
| Physics / Jolt | PhysicsContract.cpp:110、JoltWorld.cpp（BodyKind::Kinematic）；PhysicsAssetCodec.cpp（ShapeType::Compound、IndexType::U32）；JoltConstraints.cpp（Motor/Constraint）；JoltSimulation.cpp（SnapshotScope）；PhysicsBodyComponent/Resources（ShapeSource） |
| World / Animation / Editor | WorldStreaming.cpp:25/69（WorldCellTarget::Active）；plugins/animation/core/src/GraphDefinition.cpp:124（GraphComparison::GreaterEqual）；TransformGizmo.cpp 的全范围上限需与轴/面子区间区分 |

上述搜索候选已保存在 build/harmonyos-tools/enum-bound-review-candidates.json。PixelFormat 的压缩格式/整数格式子区间、PreparedRaster 的类型分组、Gizmo 的轴/面区间属于业务分类，不按整个枚举上限机械替换；候选列表不代表每项都是缺陷。本轮完成 shader 链路修复，历史调用链尚未批量迁移（任务 6.6）。

## 2026-11-01 共享 orientation 与资源部署审查

- 共享 ExportPreset 增加 orientation，默认和 default 别名均为固定 landscape；提供 portrait 和三种 auto_rotation 策略。C++ 数据/反射/JSON、schema、Android manifest、HarmonyOS mainElement ability、iOS 导出 plist 同步，原生项目源文件不被导出覆盖。iOS 老 launcher 需消费 SKY_IOS_INFO_PLIST；迁移方式见 docs/features/game-project-build.md。CLI requirements 固定 json5==0.17.3，处理真实 JSON5 而非以正则替换 manifest。
- FrameworkTest / AssetTool 与真实依赖构建成功，7 项 ExportPreset C++ 测试通过。Python 最终组合中 54 项 orientation、资源 owner、源码归属和创建/生成测试通过；另 10 项 module_resources 在显式 Ninja/Ninja Multi-Config 下通过。该模块在 Windows 默认 VS generator 下有既有 Release 输出目录假设失败，不计为通过。日志 orientation-cpp-{build,tests}.log、orientation-unified-final-tests.log、orientation-module-resources-ninja-tests.log。
- 更广的 native_package_artifacts 检查仍有 8 项 Windows 基线失败：两项 POSIX 路径预期、五项 Apple 可执行权限、一项无特权 symlink；已用 HEAD 原代码/原测试在同环境复现（orientation-native-baseline.log）。后续组合中的三项 GBK 解码报错通过 -X utf8 消除。Android SDK aapt 对真实资源打包过滤检查单独通过（build/android-tools/aapt-resource-test.log）。未把这些平台契约测试视为 Android/iOS 完整原生打包验收。
- 初次横屏 HAP 在 Native 编译期间丢失 bundles.json：项目模板 Deploy ALL 和 Python 导出同时重建同一资源目录。审查确认 Android 也重复执行，只是 Gradle mergeAssets 依赖外部 Native 构建避免了并发；Apple package 同样在 Native 部署后重做收集。
- 最终将 configs/products/开发态 shader 源统一为 stage_runtime_resources，Android/HarmonyOS 共用 cook/stage 分支，原生构建不写 assets/rawfile；Apple package 关闭 Native 资源部署并在 app 完成后调用公共入口。独立桌面/Apple IDE 构建仍通过 stage-runtime 使用该入口；移除项目模板单独的 shader staging。没有保留临时 Hvigor 排序任务，也未关闭并行。旧 APK/HAP/IPA/ZIP 退役与 cook 失败停止路径统一覆盖。
- 公开 package 命令构建及签名成功：physics-landscape-package-unified.log，Hvigor 29.184 秒。HAP 202,621,300 bytes，80 个 rawfile 与 staging 逐一 SHA-256 相同，包内 main ability 为 landscape。再次单独构建真实 OHOS Launcher 后 80 文件内容和 mtime 均未变化；Native cache 为 SKY_DEPLOY_RUNTIME_RESOURCES=OFF。证据 physics-rawfile-before-native.json、physics-native-resources-check.log、physics-landscape-summary.json。
- Pura 80 Pro 解锁后 01:48:01 启动 PID 31283，真实 swapchain 为 2848×1276。截图确认横屏全屏、立方体和地面可见；27 个动态体落稳至 Y≈0.430、active=0、contacts=27，观测至少 6 次 12 秒重置。一次 Home→恢复保持 PID 31283，恢复后继续碰撞/重置，采集窗口内无引擎 A00000 E/F 日志。系统 Vulkan 层仍输出此前已记录的 shared-mode 不支持信息，不宣称 validation clean。截图 physics-landscape-running.jpeg、physics-landscape-resume.jpeg；日志 physics-landscape-{device,resume}.log。首次即时截图包含系统启动过渡，不作为最终场景证据。

## 2026-11-01 同工程 Android 真机与应用身份归属

- SDK 遵循仓库基线：compileSdk 34 / targetSdk 33 / NDK 28.1.13356709（Clang 19），Native 与三方为 arm64-v8a / API 31，本工程 minSdk 31；AGP 8.8.2、Gradle 8.10.2、JDK 17。三方完成 14/15，CPython 因缺 frozen_modules/importlib._bootstrap.h 与 getpath.h 失败；测试包未启用 Python，完整三方预检仍待完成。
- 修复项目 CMake 入口直接接收 Windows 反斜杠路径导致 FindThreads try_compile 解析失败；模板和 HarmonyPhysics 均将 ENGINE_ROOT 转为 CMake 路径。实际 Android Native 编译通过，80 个 assets 在原生编译前后 SHA-256 与 mtime 完全相同。
- 应用标识仅由 export preset.bundleId 提供，移除 ProjectDescriptor 的 applicationId 字段、解析、默认值和写回。Android manifest/Gradle namespace 从工程自有 SkyEngineActivity.java 的 package 声明读取，不再将安装 ID 当作 Java namespace。HarmonyPhysics Android 为 com.skyengine.harmonyphysics，HarmonyOS 保留签名身份 com.skyengine.harmonyprobe。新工程为各 preset 写入独立初值；旧工程缺少有效 bundleId 时在 cook/生成之前返回失败，不隐式改包名。
- 参考依据：UE 的 Platforms/Android APK Packaging 与 Platforms/iOS Bundle Information 分开配置（https://dev.epicgames.com/documentation/en-us/unreal-engine/android-settings-in-the-unreal-engine-project-settings ，https://dev.epicgames.com/documentation/en-us/unreal-engine/ios-settings-in-the-unreal-engine-project-settings）；Cocos 3.8 的平台 Build Task/Android Game Package Name（https://docs.cocos.com/creator/3.8/manual/en/editor/publish/build-panel.html ，https://docs.cocos.com/creator/3.8/manual/en/editor/publish/android/build-options-android.html）；Godot 的 export_presets.cfg 与 Android package/unique_name（https://docs.godotengine.org/en/stable/tutorials/export/exporting_projects.html ，https://docs.godotengine.org/en/stable/classes/class_editorexportplatformandroid.html#class-editorexportplatformandroid-property-package-unique-name）。这些产品组织形式不同，比较结论仅为安装身份属于平台导出配置，并非均使用相同文件结构。
- 最终公开 package 成功（physics-package-verified.log），APK 82,647,369 bytes，SHA-256 ec6efd0b935762fbf51133e951c8933963f32c14d3f2dbca4d2c8cbe40f59a8a，APK v2 签名验证通过。包内 80 个 assets 与 staging 完全一致、9 个 ELF64 AArch64 库；最终包与设备已测包的全部 ZIP 条目内容一致，仅容器字节不同。证据 build/android-tools/physics-apk-audit.json、physics-apk-{signature,badging}.log。
- 小米 23049RAD8C / Android 15 API 35 / Adreno 725 / Vulkan 1.1.128。实际选择 render-pass2（无 dynamic rendering）、maintenance1 present fences；swapchain 2400×1080。设备 USB 安装被拒后由用户开启授权，安装与冷启动成功。27 动态方块+静态地面共 28 bodies，落稳 Y≈0.430、active=0、contacts=27，持续 12 秒重置。
- 初轮发现 Back 后同进程重开材质缺失：Application 析构销毁 AssetManager，但 metadata definition 不重注册，导致处理器丢失。改为在任务/回调收齐后 ResetRuntimeState，保留进程处理器及代码 lease，清除会话缓存、索引、bundle、工作文件系统和临时服务；未重新反射或加幂等守卫。新增真实产品加载/重开、在途 cook 拒绝 reset 回归，30 项定向 C++ 通过（runtime-restart-cpp-tests.log）。Framework、AssetTool 实际重编通过。
- 修复后 PID 23852 于 02:25:32、02:25:58、02:27:00 启动三次引擎会话；两次 Back 结束后均记录 Renderer host stopped，随后同进程重开可见材质并继续碰撞/重置；第二会话 Home 后停留三秒，恢复仍为该 PID 和原会话，继续到 cycle 4。日志 physics-fixed-{first,reopen,resume,final}.log；截图 physics-fixed-reopen.png、physics-fixed-resume.png，已目视核对。初轮失败日志保留，不将其重开计为通过。
- Adreno 驱动持续发出 VKDBGUTILWARN003，系统还输出 gralloc 格式探测和 Surface 退出信息；设备没有 Khronos validation layer，不宣称图形验证 clean 或长期稳定性完成。完整 Windows GPU 回归仍按用户要求留到最后。
- 56 项 Python orientation/资源归属/preset/源码归属测试通过（preset-identity-tests.log），Apple cook/native 顺序测试另 1 项通过；ProjectDescriptor/ExportPreset 10 项 C++ 通过并包含在最终 30 项中。更广 Windows 平台夹具的既有 8 项限制沿用上节记录。
- 鸿蒙的“物理实验场”说明文字由该示例 Index.ets 的 ArkUI 层添加，未进入共享 World/引擎 UI；Android 无该文字是示例壳差异。尚未将 UI 迁到共享游戏层，也未在 Android 再复制一套原生文字层。

## 2026-11-01 提交前整理与回归

- 三路只读审查覆盖 Vulkan/帧协议、OHOS 平台与三方构建、打包与示例。修复两项发现：按 Android、HarmonyOS API 26 SDK schema、Apple 各自规则校验 bundleId；鸿蒙 LauncherHost 在 Stop/join 后清除会话状态，保持 VSync generation 单调递增，区分会话关闭与最终 N-API env cleanup，并保留单活动 owner env 约束。未引入重复反射或测试生产源码副本。
- 项目格式整理只作用新增 C++ 文件和既有修改行；移除示例 Windows main.cpp 的 UTF-8 BOM，新增/修改 C++ 的非 ASCII、throw/try/catch/dynamic_cast 扫描结果为 0。凭据、SDK、APK/HAP、日志与本机 cache 均排除在提交集合外。
- 保留 cmake-build-debug 的 Visual Studio 17 2022 / 既有 Win32 3RD_PATH。FrameworkTest、Aurora.RHITest、Aurora.CoreTest、Aurora.AdaptorTest 及真实后端重编通过。GPU target 还需要 EditorRender/EditorShell，原 cache 的 SKY_BUILD_SANDBOX=OFF 不满足该依赖；启用既有 Sandbox targets 后 Aurora.Renderer.GpuSubmissionTest 构建通过，没有绕过 include/源码归属边界。
- Framework 资源会话与项目配置 30 项通过；Aurora shader 52 项通过、1 项因未准备 relocated shader package 跳过；Vulkan 能力协商 16 项通过；RendererHost adaptor 4 项通过。日志 prepare-commit-{framework,shader,rhi,adaptor}-tests.log。它们是定向原生回归，不代表编辑器全量测试已完成。
- Windows RTX 3080 的 FrameOutputs 最终完整运行 132 项：119 通过、13 个 Vulkan 专属用例在 DX12 参数下跳过，0 失败，含两个 Vulkan compiler 的三项 core WSI 实测。初轮两条旧“缺维护扩展必须失败”断言已按可选扩展契约改写，在创建 swapchain 前注入缺失入口并用 RAII 恢复函数表；最终全组重跑通过。日志 prepare-commit-gpu-tests-final.log，首轮 GPU validation error 行数为 0，不将所有平台称为 validation clean。
- Python 组合 69 项通过，覆盖平台身份、orientation、公共资源 owner、shader 与源码归属；OHOS 平台/打包契约另跑 10 项通过（与组合有部分重叠，不相加宣称唯一用例数）。日志 prepare-commit-python-tests.log、host-restart-contract-tests.log。
- 修复后的真实公开 HarmonyOS package 构建/签名成功（112.528 秒），已安装 Pura 80 Pro。PID 24960 初次物理场景能碰撞、休眠并重置；通过正常返回键和 aa start 重开，命令成功且 PID 仍为 24960。采集重开后日志/截图时 HDC 断开，不能仅据启动成功认定 Stop→Configure 完整通过，任务 6.8 仍保留待验收。证据 prepare-commit-hap-build.log、prepare-commit-device-first.log；断连失败日志另存，不作为通过证据。
- 重新连接后，PID 27565 的正式包前后台恢复、2848×1276 画面与物理继续运行通过。但返回键实际仅切入后台，未调用 shutdown，因此不能计为 native host 重启验收。进一步仅在被忽略的生成 EntryAbility.ets 中加入临时 onNewWant 验收入口，先卸载 XComponent，再调用真实 engine.shutdown/configure/setActive 并重新创建 XComponent；正式 Native 源码未增加诊断入口，验收包与正式包的 11 个 Native 库逐一 SHA-256 一致。
- 临时验收包 PID 31886 于 11:41:55、11:42:59 两次记录 shutdown=true/configure=true/active=true；每次均先 Renderer host stopped，再由新 worker 初始化同一游戏模块和物理场景。第二次重启后持续到 cycle 3，28 bodies、active=0、contacts=27，采集窗口内引擎 A00000 域 E/F 为 0。已目视核对重启后 mesh/material 画面。证据 prepare-commit-native-restart-{test,final}.log、prepare-commit-native-restart-proof.jpeg。该结果不扩大为人工构造过期回调或长期压力验证。
- 临时 ArkTS 文件已恢复为工程自有 EntryAbility.ets 的内容，正式生成输出重新构建通过（14.420 秒），手机重新安装原正式签名 HAP。临时入口、日志及验收产物均在忽略目录，不加入提交。
- Android CPython frozen-header 问题经 HEAD 对比确认为既有构建限制，本轮未引入；继续记录 14/15，不宣称全部依赖完成。Adreno 驱动诊断/高频 acquire 超时、OHOS 重复输入/Surface/失败路径与长时间运行仍未验收，change 保持 active，未 archive。

## OpenSpec 严格校验

初次 `add-openharmony-platform` 的严格校验通过。目标澄清并更名后，`openspec validate add-harmonyos-platform --strict --no-interactive` 再次通过，status 显示 proposal/design/specs/tasks 全部齐备。OpenSpec artifact 完整不等于平台实现已完成。


## 2026-11-01：OHOSHost 宿主收拢与重新验收

- 用户确认后，将事件队列、worker、NativeVSync、generation 和 Stop/join 从项目 launcher 收拢到现有 Framework 的 `platform/ohos/OHOSHost.cpp`；不新增 target。launcher 保留 N-API/XComponent 接线，以及在引擎线程创建并初始化 GameApplication 的工厂。入口由 426 行缩为 195 行。
- OHOSHost 使用 Environment 共享的 Singleton 作为跨会话稳定 receiver，PIMPL 只重置会话、不销毁 receiver。Host 独立于 Application 析构时关闭的 OHOSPlatform，外部 N-API owner 在释放 ResourceManager/VM 引用前停止并 join worker。XComponent 注册失败路径已补 CleanupSession，绑定拒绝不会误关已有会话。
- Surface 在入队前只创建一次 SDK 引用，Platform、Window 和 RHI 复制同一 lease；ref/unref 共享跨会话的 mutex，deleter 持有该 mutex 的共享所有权。代码审查核对了锁顺序与 GPU lease 保活；没有以互斥锁代替 renderer 退休协议。
- `python -X utf8 -m unittest python.tests.test_harmonyos_platform python.tests.test_harmonyos_package python.tests.test_runtime_resource_staging python.tests.test_cmake_source_ownership`：41 项通过（24.736 秒）。新增契约覆盖公开 create 的薄桥接归属与 generate 保留项目自有入口。日志 `host-contract-python-tests.log`。
- 使用 D 盘共享 API 26 Native SDK（26.0.0.105、Clang 15.0.4）从真实 Core/Framework target 编译 `HarmonyOSFrameworkProbe`，whole-archive/no-undefined 链接通过；fixture 只编译 probe，不重复编译生产源码。日志 `host-refactor-framework-build.log`。该 probe 是编译/链接检查，不作为线程行为测试。
- 公开 `python python/package_project.py package -p examples/HarmonyPhysics --preset HarmonyPhysics-harmonyos` 完成 cook、Native 编译和正式签名；Hvigor 1 分 34.147 秒，资源仍由公开导出流程准备。日志 `host-refactor-package.log`。编译保留 SDK 的 unused gcc-toolchain 参数及原有 ArkTS fullscreen API 诊断，没有新增 C++ 异常处理。
- Pura 80 Pro 正式包 PID 4266：2848×1276 横屏场景可见，28 bodies、27 contacts、休眠及 12 秒自动重置正常；Home 三秒后恢复保持 PID，采集窗口内引擎 A00000 域 E/F 为 0。日志 `host-refactor-{formal,background}-device.log`，截图 `host-refactor-formal.jpeg`。
- 仅在忽略的生成 EntryAbility 中增加临时 onNewWant 验收入口，不增加生产 Native 导出。验收 HAP 的 11 个 `.so` 与公开正式 HAP 逐一 SHA-256 一致（`host-refactor-native-equality.json`）。PID 5990 内 12:30:21、12:30:36 两次完成 shutdown=true、second=true、configure=true、active=true，重复 Configure 均返回 false。每次重开后重新初始化渲染器并继续物理模拟，第二次已运行到 cycle 1，采集窗口内引擎 E/F 为 0。日志/摘要 `host-refactor-restart-{device.log,summary.json}`，截图 `host-refactor-restart.jpeg`。
- 临时 ArkTS 文件已按字节恢复为工程自有源，正式生成输出再次构建通过（19.905 秒）；手机已重新安装公开正式签名 HAP，最终 PID 7084。日志 `host-refactor-formal-restored-build.log`、`host-refactor-formal-final-device.log`。验收脚本、日志、SDK、HAP 和本地示例均不加入提交。
- 末尾保留现有 cmake-build-debug 的 VS2022 generator 和 3RD_PATH，Framework/FrameworkTest 构建通过，FrameworkTest 完整 240 项、47 套件全部通过（8.321 秒）。日志 `host-refactor-win-{environment,build,tests}.log`；只有既有 SFMT/gtest 缺 PDB 的 LNK4099。此次不重复未受影响的 GPU 回归。
- 该次宿主收拢验收当时未注入人工延迟/过期 VSync 或 SDK 注册失败；后续独立断言与 65 秒组件压力见下节。长期 GPU/Surface 压力与最终输入/IME 验收仍单独记录。以上历史记录不替代 6.8，也不扩大为本轮后续测试的结论。

## 2026-11-01：canonical SDK 三方重建闭环

- canonical Native SDK 为 `D:/Program Files/Huawei/sdk/default/openharmony/native`，元数据 26.0.0.105 / API 26、Clang 15.0.4。使用公开流程实际重建/安装 Android 同集合、同 pin 的全部 15 包；输出 `build/harmonyos-thirdparty/HarmonyOS`，中间目录 `build/harmonyos-thirdparty/intermediate`。这次实际重建取代迁移初期的 15 stale 状态，未手改元数据 key 冒充重编。
- 可复现入口为 `python -X utf8 python/third_party.py -p HarmonyOS --ohos-sdk "D:/Program Files/Huawei/sdk/default/openharmony/native" -i build/harmonyos-thirdparty/intermediate -o build/harmonyos-thirdparty -j 6`，SDK 自带 `build-tools/cmake/bin` 置于 PATH 前端，进程显式设置 `CMAKE_BUILD_PARALLEL_LEVEL=6`。公开 `-j` 不能自行覆盖自定义构建脚本的环境；OpenSSL/CPython 已尊重该显式并行值，未更改 Android pin 或删去缺元数据的 CPython。
- 用同一 canonical `--ohos-sdk` 和输出目录执行 `third_party.py --check --strict`，结果 **0 missing/stale、0 output-missing，exit 0**。日志 `build/harmonyos-tools/closure-thirdparty-{build,check,contract-tests,audit}.log`，SDK 元数据/内容指纹记录在 `closure-thirdparty-environment.json`；构建前后指纹一致，三方/SDK 契约 6 项通过。

| 包 | 实际 pin |
|---|---|
| sfmt | 1.5.4 |
| rapidjson | v1.1.0 |
| googletest | v1.17.0 |
| vulkan_headers | v1.4.348 |
| vma | v3.2.1 |
| recast | v1.6.0 |
| jolt | v5.6.0 |
| lz4 | v1.10.0 |
| zlib | v1.3.1 |
| tracy | v0.11.1 |
| freetype | VER-2-13-3 |
| slang | v2026.19 |
| acl | v2.1.0 |
| openssl | openssl-3.0.18 |
| cpython | v3.13.9 |

- Slang host codegen 和目标 Debug/Release、OpenSSL Release、CPython frozen headers/目标对象/stdlib ZIP 实编完成。只读产物审查确认 12 个 shared ELF 为 ELF64 AArch64、48 个静态归档的首成员为 ELF64 AArch64；不把首成员检查称为逐成员审计。CPython 导出包含 `PyInit__ssl` / `PyInit__hashlib`，python313.zip 共 549 项且包含 encodings 与 ssl.py。header-only 包按配置/安装记录，不宣称存在独立二进制。
- 上述结果不证明设备 Python import、TLS 握手或所有三方运行能力；这些仍归可选运行验收。Android 的 CPython 14/15 历史构建限制也不由 HarmonyOS 闭环代替。

## 2026-11-01：Host 设备断言与真实组件短压力

- 独立 fixture `python/tests/fixtures/harmonyos_host_acceptance` 链接真实 Core/Framework target，API 26/arm64-v8a、`--no-undefined` 增量编译通过。最终测试 `.so` 仅用链接包装控制 SDK VSync 的请求/失败/回调时机；正常 Create/Destroy 及 NativeWindow 引用调用真实 SDK。窗口来自两个真实 XComponent，不伪造窗口，不重复编译生产源码。
- 隔离 HAP 由 `prepare_hap.py` 生成到全新的忽略目录，复制既有本地签名配置而不打印签名材料，Hvigor 真实编译/签名通过。Pura 80 Pro / HarmonyOS 7.0.0.109 / API 26 上多次运行均得到 `{"passed":30,"failed":0,"ticks":2,"requests":7}`。最终冷启动 PID 9133 的具名日志确实包含 **30 PASS / 0 FAIL**，证据 `host-acceptance-thirty-assertions.log` 与 `host-acceptance-thirty-layout.json`；不能以只编译 fixture 当作设备通过。
- 30 项覆盖 VSync Create/Request 与 application factory 失败、Stop 后重试、foreign XComponent、零尺寸、后台/前台、两真实 Surface 替换保留 WindowID、Destroy 内回调、Stop 后和跨 Configure 的迟到回调、旧共享 lease 延迟释放，以及 application/VSync/window 引用配平。负向 tick 检查观察 50 ms，正向转换等待 worker 观测；不声称覆盖所有线程交错。
- 另一个 **65 秒**压力会话直接使用 SDK VSync 回调，ArkUI 每 600 ms 实际移除/重建 XComponent，EntryAbility 的真实前后台事件投递给 Host。HDC 通过 Home/重新启动 Ability 形成 6 对前后台。PID 6855 最终原生/UI JSON 一致：48 次 Surface 接入、47 次运行期间 destroy 回调、0 failures、6 foregrounds、6 backgrounds、1806 ticks；Stop 后 `status=STOPPED`，Host NativeWindow retain/release **48/48**、Application 创建/销毁 **1/1**、VSync 创建/销毁 **1/1**。最后一个 live Surface 由 Stop 退休，因此不把 47 次 destroy 回调改写成 48 次。
- RSS 初始 **75092 KiB**，约 75.17 秒采样为 **92744 KiB**，稍后为 **94428 KiB**；线程数初始 45，压力采样 48–52，后置采样 53。`/proc/6855/fd` 返回 Permission denied，未取得文件描述符计数。必须保留 RSS 增长事实，不能仅凭 Host 显式引用配平称进程无泄漏；短测不能证明长期内存稳定。
- 此 fixture 的 Application 只记录 tick/窗口状态，**不运行 Renderer**，不替代持续 GPU 呈现、acquire 失败或 GPU/present 退休验收。旋转、长期运行、纹理和缺资源/图形失败路径仍未完成。
- 原始证据位于 `build/harmonyos-tools/host-acceptance-stress-{commands,metrics,hilog}.log`、`host-acceptance-stress-final-layout.json`，汇总 `host-acceptance-device-evidence.json`。测试结束后安装并启动最新正式 `HarmonyPhysics-debug-signed.hap`（203884792 bytes、PID 10716），已目视确认横屏物理场景与中文 UI；恢复记录 `host-acceptance-restore.log`、截图 `host-acceptance-restored-formal.png`。临时包、日志、SDK 和凭据均位于忽略输出，不加入提交。

## 2026-11-01：统一系统输入框与 Windows 回归

- 公共 `NativeTextInputOptions` 统一 text/placeholder/title、Unicode 标量 maxLength（0 不限）、multiline/password；multiline+password 拒绝。CHANGED 为 committed 完整快照且去重相同文本，CONFIRMED/CANCELLED/ERROR 终态一次，回调在 engine owner 线程；token 隔离迟到 UI 回调，Hide/关闭排空，Shutdown 先 disable 防止重入打开。初始请求不被接受时返回 false 且无回调，已接受的 native 失败通过 ERROR 收尾。
- Android Activity Dialog/EditText、HarmonyOS ArkUI TextInput/TextArea + CustomDialogController、iOS UIKit UITextField/UITextView 复用同一数据/事件契约，保留各自原生 UI 线程调度。OHOS Host 的 Dialog 派发先于 inactive frame gating，后台也能收齐取消。此项为原生 UI，不是引擎手绘编辑控件；窗口 key/committed-delta 事件仍走原有输入接口。
- UTF-8/NUL/标量长度校验覆盖 options 和 UI 快照。非法 CHANGED 被拒绝且保留请求；原生 UI 可能已结束的非法终态转为 ERROR，保留最后合法快照，使用 -2 或原 ERROR 的非零 nativeError。`Fail(token, nativeError)` 在同一锁内读取包括 queued CHANGED 的最后合法快照并排队一次 ERROR，防止 JNI/N-API 转换失败以空串覆盖输入；旧 token、非法 enum、重复终态拒绝。终态清除 callback/active/旧快照后再回调，错误回调内也可开启下一请求。正文和密码不写日志。
- 测试回调只捕获 fixture 存储，TearDown 先 disable 再 Cancel，避免 ASSERT 提前退出后访问已析构 locals。**17 项公共状态行为测试**覆盖 owner 派发/错误线程、去重、Cancel 排空、重入 Show、并发终态、Unicode 标量与非法快照、错误桥接、旧 token 和下一请求恢复。
- 保留 `cmake-build-debug` 的 `Visual Studio 17 2022` generator、`3RD_PATH=D:/Code/Engine/sky_3rd/Win32` 和 `SKY_BUILD_TEST=ON`，关闭 SandboxEditor 后真实构建 Framework/FrameworkTest。最终完整 **270/270、51 suites** 通过（9.147 秒），日志 `native-text-input-fail-win-{build,tests}.log`；之前两轮 267/269 是阶段数量，不冒充最终结果。clang-format、ASCII/no BOM 与异常/RTTI 检查通过，未引入异常处理或输入正文日志。
- OHOS 输入策略为 **13 项桌面测试**（8 pointer、2 keyboard、3 text），涵盖取消/按钮状态、分数滚轮、边界、重复键与 surrogate/embedded-NUL 转换。`ohos-input-framework-tests.log` 的当次完整桌面回归为 253/253；这 13 项不是手机 IME 或 Native UI 运行结果。Android/OHOS 的真实 UI 与原生 IME 结果另见后续两节，不与桌面数量混算。
- UIKit 原生编辑器、CMake source owner 与设备测试/runner 源码已提供；用户明确目前没有 Mac，先完成 OHOS/Android，**iOS 编译和 UIKit 运行 deferred**。本机 37 项 Python owner/runner 检查仅验证源码/组织/runner 契约，不能称 UIKit 编译或运行通过，也不作为本轮 OHOS/Android 验收的阻塞条件。

## 2026-11-01：Python 组合检查与 Android 隔离原生 UI

- 本轮额外 Python 组合实际运行 **96 项，2 failures / 6 errors / 5 skipped**，不能称全 Python 通过。8 个失败/错误均落在既有 `native_package_artifacts` Windows 夹具限制：2 项 POSIX 路径断言、5 项 Apple app executable 权限和 1 项无特权 symlink，与前述 `orientation-native-baseline.log` 已记录的问题一致。当前完整日志 `build/harmonyos-tools/native-input-final-python-tests.log`；本轮未新增修补这些无关基线。
- 排除该既有模块后，8 个可移植模块 **68 项全部通过**（37.735 秒），日志 `native-input-portable-python-tests.log`。部分 runner 测试会打印 mock 构建步骤，不把其输出当作 macOS/Xcode/UIKit 真编译。37 项 UIKit owner/runner 检查和这次 68 项可能包含交集，不相加为独立测试总量。
- Android 独立 fixture 用 NDK 28.1 / compile SDK 34 / JDK 17 执行真实 Gradle/native no-undefined 构建，在已连接手机运行 native **121/0**、UI **20/0** 检查，共 **9 个终态**：6 CONFIRMED、2 CANCELLED、1 ERROR。覆盖单行、多行、Unicode 标量限制、密码、取消/后台、重开及实际 JNI null jstring 转换失败；错误使用 nativeError=-1 并保留最后合法快照，下一 retry 确认可用。该错误由隔离 fixture 触发，不为生产库新增测试 API。
- 原始证据为 `build/android-tools/native-ui-summary.json`、`native-ui-native-events.log`、`native-ui-device.log`、`native-ui-apk-build.log`；IME 展开后的编辑区/按钮 bounds 记录在 `native-ui-window-case*.txt`，截图 `native-ui-{multiline,unicode-limit,jni-recovery}.png`。这些是 Android 实际 Native UI 结果，和公共状态/桌面策略测试分开计数。
- 该阶段的第一次公开 package 包含共享临时 OHOS NativeUIProbe，`formal-summary.json` 当时明确 clean=false / restorationComplete=false；fixture finally 仅恢复了此前备份旧 APK，没有把此状态记为新正式包恢复通过。共享 probe 源按字节恢复、临时头删除后，两端已重新公开 package 并完成纯正式包检查、安装，最终证据见文末；不以隔离 UI 通过代替正式恢复。

## 2026-11-01：OHOS 最终原生 UI 与 IME 真机验收

- N-API 文本读取失败的终态收尾修复后，在 Pura 80 Pro 最终 PID **31461** 运行完整 **7 cases / 0 fail**，回调计数为 **19 CHANGED / 5 CONFIRMED / 2 CANCELLED**。覆盖单行、多行、maxLength=3 保留 `A😀B`、密码、取消保留已有文本、Home 后取消和重开；密码内容不在结果正文记录，secure 截图为全黑。首轮 phase 3 曾被取消，未计通过；随后两轮完整 7 cases 均通过，这里引用最终轮。
- 所有 7 个编辑区与确认/取消动作在实际键盘展开时保持可见，编辑区/动作高度至少 100 物理像素。原始自动化及设备日志 `build/harmonyos-tools/native-ui-acceptance-final-run.log`、`native-ui-acceptance-device.log`，共享摘要 `native-ui-acceptance-summary.json` 的 PID 为 31461；不要使用早期不完整轮次替代最终结果。
- 原生窗口 IME 与上述 dialog 回调分开验证。先前 PID 26036 实测键盘提交 U/v、拼音候选“你”，观察期间没有把 `ni` 预编辑作为 committed 文本。最终 PID 31461 在 **14:33:07.031** 精确收到 pointer down (2000,300)，触发显式 StartTextInput；点击前 `native-ime-hidden.jpeg` 已确认无键盘，`native-ime-reopened.jpeg` 确认重新可见，**14:33:19.696** 收到 `native committed=你`。原生 key 2017 映射 scan 4；输入坐标、键事件与 committed 文本分别有日志，不靠 UI 外观推断回调。
- 证据 `native-ime-{device,reopen-device}.log`、`native-ime-preview.jpeg`、`native-ime-hidden.jpeg`、`native-ime-reopened.jpeg`。隔离验收仅记录合成测试输入，密码不记录；生产平台路径不记录输入正文。多接触触控压力、更低 API、多设备和长期 IME 循环不由这些单机样本推断。
- 临时共享 probe 的 cpp 已按字节恢复、header 删除；两端纯正式包随后完成公开重建、无 probe 审计及实际安装/启动，见下节，任务 6.11 依据这些独立产物/设备证据关闭。

## 2026-11-01：最终纯正式包重建、审计与恢复

- 两端均从已还原的正式模块源重新执行公开 `python python/package_project.py package -p examples/HarmonyPhysics --preset <对应平台 preset>`，不是仅恢复旧备份或重新签隔离验收包。共享正式模块源码 SHA-256 为 `eab4e30fa75837ee392c5461c4d522a0b28d7043863b31ef97cf4696b9dc8285`；临时 header/注册已删除，产物不含 NativeUIProbe/HostAcceptance 验收二进制，运行中无 probe 日志。
- HarmonyOS 中间尝试曾因未补齐 CLT/`SKY_3RD_PATH` 等环境失败，保留 `build/harmonyos-tools/native-input-formal-missing-dependency-env.log`；补全显式参数后公开 package 成功，**不能写首轮全部成功**。最终 Hvigor **33.487 秒**（Native 阶段 11.210 秒），签名 HAP **203897479 bytes**，SHA-256 `1ea7f710f24019e0442b36f6ec3cf1df6e30fa2c6d29c7e05d8f9561f286f578`。使用 SDK 工具审计 **11 个 ELF64 AArch64 `.so`**，DT_NEEDED 对包内/SDK 系统库闭包 missing=0，**80 个 rawfiles 与 staging 逐字节匹配**。证据 `native-input-final-{formal-package.log,hap-audit.json}`。
- HarmonyOS 正式 HAP 安装启动 PID **41424**，目视确认 **2848×1276** 横屏彩色立方体/地面；28 bodies、27 contacts、12 秒 reset 可见。Home 三秒返回保持同一 PID 并继续物理场景，采集窗口引擎域 E/F=0。摘要 `native-input-final-formal-summary.json` 的 restorationComplete/visualSceneVerified/sceneAfterResumeVerified 均 true；设备日志 `native-input-final-{formal-device,formal-resume-device}.log`，截图 `native-input-final-formal.jpeg`、`native-input-final-formal-resumed.jpeg`。
- Android 公开 package 的真实 Native/Gradle 重建约 **13 秒**成功，最终纯正式 APK SHA-256 `3c95adfe34e8bbad3dc54cee641268e7718196fc32f2d97efa0230c7f0855b11`；安装包与新产物 SHA-256 一致，Activity 与最终模板一致，新 boolean JNI 注册可用且无 JNI 警告。APK 签名验证通过，**9 个 ELF64 AArch64 `.so`** 闭包 missing=0；PID **21895** 的 **2400×1080** 横屏彩色物理方块/地面目视确认，27 个动态体推进，Home 返回保持 PID 和场景，无 probe 自动弹窗/日志。
- Android 证据 `build/android-tools/native-ui-formal-{summary.json,package.log,device.log,resume.log,apk-audit.json,signature.log}`，截图 `native-ui-formal-new.png`、`native-ui-formal-resumed.png`；summary 的 formalCleanPackage/restorationComplete/installedMatchesNewApk/visualSceneVerified/visualSceneAfterResume 均 true，sharedNativeProbePresent=false。最终 APK/HAP 恢复已完成，不把此前 clean=false 的阶段摘要当作最终状态。
- 任务 6.11 仅表示验收后纯正式包已恢复。纹理、缺资源/图形失败矩阵、旋转、持续 GPU/present 压力、长期 RSS 行为与 deferred UIKit 运行仍按既有未完成项保留；本 change 仍 active，未执行 commit 或 archive。
