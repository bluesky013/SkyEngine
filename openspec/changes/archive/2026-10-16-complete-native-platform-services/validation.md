# Native platform validation

Validated on 2026-10-15 from `dev_refactor_rhi` rebased onto `origin/dev_refactor_rhi`
at `61f7ff0a`. The initial phase covers native platform services and shared application/worker
contracts. The package follow-up also changes Apple default backend selection and Metal framework
packaging; it does not change GPU algorithms or implement surface recovery.

## 修复与补齐问题总表

本表统一记录平台服务、UIScene 迁移和真实模板打包的问题、修复与验证边界。
实现导航见 [平台现状](../../../../docs/architecture/platform-support.md)，
产物、复现命令及哈希见 [模板打包验证](package-validation.md)。

### 共享运行时与三端服务

| 范围 / 问题 | 根因及修复 | 验证与边界 |
|---|---|---|
| 帧时间精度、Application 串扰、恢复后的巨大 delta | 每个实例使用 `FrameClock`；先做整数计数差再换算，处理零频率、回退和恢复 epoch | 30/120 Hz、大计数、多实例、暂停恢复回归 |
| 后台/无 surface 仍 Tick，handle 重建丢失窗口身份 | 增加 active/epoch/stop 契约；继续收事件但暂停业务 Tick；保留 C++ 窗口/WindowID，以 keyed `OnSurfaceChanged` 通知句柄替换 | Android/iOS 状态策略及 UIKit 重连；RHI 消费方和 GPU surface 恢复尚未实现 |
| 退出回调、排队任务访问已销毁对象/卸载模块 | 停止外部帧回调，等待资产任务并清空待执行派发，再卸载模块；再次等待/清空退出任务；解绑 native owner/delegate | shutdown、延迟回调和重入销毁回归；`Clear()` 是清空回调，不是执行回调 |
| Android semaphore 空实现与线程析构风险 | Unix/Android 使用计数信号量，禁止拷贝；`NamedThread` 先 join 再释放 semaphore | Core、三项 semaphore 回归、四项 ASAN 选择用例 |
| 滚轮精度、DPI 单位和配置来源不一致 | wheel 改 float；输入/resize/IME 使用 framebuffer pixels；工程/可写配置优先、包内 filesystem 回退 | 三端策略、原生和 config staging 回归；共享事件 ABI 需 exe/模块一起重编 |
| macOS 原生能力与关闭安全缺口 | 补齐移动/resize/焦点/键鼠、实时几何、坐标/DPI/光标、bundle filesystem 和文件展示；完善路径错误处理与 shell 退出状态；关闭只广播一次并解绑 view | 十项原生 Cocoa 用例，含回调内删除窗口和销毁后的延迟输入；getenv/剪贴板/对话框不是本轮新实现；真实多显示器/外设待测 |
| Android 时钟、Activity/input pump、路径/APK 语义不完整 | 接通 GameActivity/ALooper，monotonic 纳秒计时；主 pointer ID、取消/修饰键/滚轮/提交文本；内部可写路径；统一 APK 缓冲 stream 游标、短读/EOF 和逻辑路径 | 十四项 host 策略执行；四项 native SDK 用例及完整 FrameworkTest 编译/链接。未执行真实 Activity、IME、压缩 AAsset；当前 archive 缓冲整个文件 |
| GameActivity 旧版源码内联与 NDK 28 不兼容 | 改用官方 1.2.2 AAR/Prefab static target，移除 GameActivity/GameTextInput/glue 重复源码编译；launcher 检查初始化失败 | 正常依赖解析、严格链接及唯一 JNI/glue 符号检查；不是 Android 设备执行 |
| iOS 全局 UIApplication 生命周期与 scene 所有权不匹配 | launcher/test host 改单 UIWindowScene，经 `IOSSceneHost` 连接，按身份过滤回调；process-owned GameApplication 不重复初始化 | 初始 scene 由 UIKit 真实创建。模拟器验证平台窗口/ID；完整游戏重连仅审阅代码，未做系统后台往返 |
| iOS 断连、输入/IME/DPI 与延迟 native 回调缺口 | 分开 Attach/Detach 与永久 Close；保留窗口/ID，解绑旧 view；scene scale 统一像素单位，补主触点、UTF-8/退格、marked text、IME 矩形/布局 | 二十一项 scene suite，含 UITextView composition、迟到 layout/input 和重入销毁；不包含 GPU 重建或 iOS 27 验收 |

### 构建、包内依赖与资产工具

| 问题 | 根因及修复 | 验证与边界 |
|---|---|---|
| 首次配置覆盖 preset 插件 OFF，Android 遗漏 native 参数 | 首次空 manifest hash 不再 FORCE 用户 cache；转发 engine/packing/cache/编译/额外参数，使用单引号 Groovy argv | 真实 CMake 首配、参数转义/UTF-8、实际 Gradle cache；manifest 后续变化刷新策略保留，特殊路径的 runtime JSON 转义仍有限 |
| 模板模块硬编码 SHARED，iOS 静态 factory 缺失 | 使用 `sky_add_module`/tagged factory，iOS 默认 static packing，生成并链接注册 cpp | 实际 iOS 有五个静态 factory，含游戏模块 |
| Apple 语言/manifest/default 不匹配实际宿主 | 启用 ObjC++，修正 macOS AppKit plist，Apple DEFAULT 选 Metal、保留显式 override | SDK 构建、plist、默认/显式 Metal 启动；未修改 GPU 算法 |
| Apple 动态库布局和 loader 依赖开发目录 | iOS Metal 使用 embedded framework；相对 install name/RPATH、bundle loader 路径；macOS 每次部署内建/可见 imported runtimes | arm64 iOS framework/符号/RPATH 审计；macOS 十五个 Mach-O 路径闭包及移目录启动 |
| Slang 别名不满足真实 SONAME，GLSL companion 漏包 | 导入真实 compiler SHARED target；按 config/ABI 生成清单，AGP variant 接入 generated jniLibs；排除已链接目标和 Android 兼容别名 | 最终九个 ELF 依赖完整，compiler/GLSL 齐全；移除同字节 39,361,056 字节副本 |
| offline builder 混入移动 runtime | Content/Navigation builder 只配置于开发模式桌面，保留 runtime/adaptor | iOS 图与包检查；最终 macOS 也无 offline builder dylib |
| 资源根目录错误、未收 IPA、重复打包残留旧文件 | configs/products 放在 iOS app 根或 macOS `Contents/MacOS`；收集 Payload IPA/app ZIP；重建 staging/app 包目录并复用对象文件 | 十一项 cooked 产品；CRC、字节、权限、symlink、删文件/旧 bundle 回归 |
| AssetTool filesystem root 悬空 | raw pointer 经临时 CounterPtr 可提前释放 root，数据库只保留子 filesystem；改用 owning 指针再交给 managers | 真实进程集成用例；未修改资产转换算法 |
| builder 配置加载太早，cook 失败仍返回成功 | PostInit 在模块注册后合并工程优先/engine 回退配置；等待真实 completion，失败/无结果返回非零，package 传播错误 | 有效 PNG 真实落盘与配置覆盖通过；坏 PNG exit 1；三端最终各十一成功、零失败。工具缺失仍沿用 pre-cooked 工作流 |
| 工具链/并发限制和测试假成功 | 环境选兼容 JBR 21；smoke 用深度二 Ninja 共用池；checked-in scene host/runner 要求非零测试数和成功退出标记 | 环境规避与测试补充；AGP 直接调用 Ninja，单设 CMAKE_BUILD_PARALLEL_LEVEL 不足；空 filter 被正确拒绝 |

### 验证结果及剩余问题

测试数按阶段记录，不相加，也不把编译/包审计写成原生运行。

| 阶段 | 结果 | 适用范围 |
|---|---|---|
| 平台服务 | Core 276/276；ASAN 选择用例 4/4 | 未在后续打包阶段重跑全部 CoreTest |
| UIScene 迁移后的 host | Framework 最后 183/183，含十项 Cocoa、九项 iOS portable | 保留更早 182/183、isolated 19/20 的资产并发失败 |
| iOS simulator | iOS 18.6：21/21，九项 portable＋十二项 UIKit；空 filter 负例通过 | 初始 scene 真实连接；后续状态通过 delegate selector/帧回调驱动，非完整游戏/GPU 系统后台恢复 |
| Android SDK | Core/Framework、launcher、native cases、完整 FrameworkTest 编译/链接 | API 31 手工验证；十四项策略在 host 执行，无 Android native 执行 |
| 打包回归 | Python 27/27；真实 AssetTool 正负两个进程用例 | 未重跑全部 Core/Framework suite |
| 最终包 | APK：九个 ELF/二十二个资源文件、JNI/签名/对齐；IPA：scene/framework/静态 factory/资源/ZIP；macOS：闭包/权限/三处 symlink、默认/显式 Metal 各三帧退出 0 | APK min API 29/target 33；IPA arm64、SDK/minimum 18.5、unsigned；macOS x86_64、SDK/minimum 15.5、Intel Iris Plus 655 |

尚未完成：

- RHI 消费 surface 通知，完成 swapchain/GPU surface 恢复、无 drawable 帧取消和同步。
- macOS 捕获当前为 AppKit 拖动跟踪/应用内 monitor，非系统级独占捕获；显式 Vulkan 的 SDK/dlopen 分发路径未由 Metal smoke 验证。
- Android/iOS 真机后台、旋转、输入/IME、包内读取和 GPU 验收；iOS 27 SDK/runtime 验证。
- Apple 签名/profile/archive/export、公证及商店预检（launch screen 尚未配置）；collector 当前要求 signing=off，IPA 不可直接普通真机安装。
- 更多架构/旧系统版本、Windows 验证和 Linux native backend；Android 多 ABI（当前仅 abis[0]）、release 裁剪及 DX12 ASTC 防护。
- macOS/Android Slang metadata 与 split-debug 选项仍有差异，使用相同 2026.9.2 已安装库，不宣称三方预检全绿。
- `AssetManagerTest.ConcurrentLoadStress` 对应的并发产品发布竞态未修复；本轮 AssetTool 修复不包含该算法路径。

以下保留分阶段原始验证记录。此前平台测试以 iOS 13 target 编译，不代表整包最低系统为 13。

## Executed host cases

Host: Intel macOS, Xcode toolchain, existing `cmake-build-debug` Xcode configuration
and `build_3rd/MacOS-x86` dependencies.

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Debug --parallel 4 --target CoreTest FrameworkTest SandboxModule SandboxEditor -- -quiet
output/bin/Debug/CoreTest
output/bin/Debug/FrameworkTest
python3 -m unittest discover -s python/tests -v
```

- CoreTest: 276 cases passed, including counted permits and queued-signal destruction.
- FrameworkTest: 180 cases passed, including all 10 native Cocoa cases and the retained-view
  regression for delayed keyboard, text, pointer and wheel callbacks after owner destruction.
- Python: 4 cases passed, including both new APK startup-config staging cases.
- Android policies execute on this host: 14 cases for lifecycle/epoch, pointer IDs and
  cancellation, fractional wheel input, keys/modifiers, committed modified-UTF-8 text,
  logical APK paths, partial reads and early EOF.
- iOS policies execute on this host: 6 cases for activity/surface readiness, permanent stop,
  resume epochs, framebuffer conversion and primary touch cancellation.
- Shared Framework cases cover per-instance 30/120 Hz timing, large counters, background
  epochs, writable/bundle config precedence and asset-worker completion before module shutdown.

Native Cocoa cases require a logged-in WindowServer session; this run executed them without
skips. Framework, SandboxModule and SandboxEditor were rebuilt together after the shared
wheel-event ABI changed.

The Android/Unix semaphore production source was also compiled directly on the host with
GoogleTest: all 3 semaphore cases passed. An AddressSanitizer binary compiled that same Unix
source and the production NamedThread implementation:

```bash
clang++ -std=c++20 -fsanitize=address -fno-omit-frame-pointer -g \
  -I engine/core/include -I build_3rd/MacOS-x86/googletest/include \
  engine/test/core/AsyncTest.cpp engine/test/core/SemaphoreTest.cpp engine/test/core/main.cpp \
  engine/core/src/async/NamedThread.cpp engine/core/platform/unix/async/Semaphore.cpp \
  cmake-build-debug/engine/core/Debug/libCore.a build_3rd/MacOS-x86/googletest/lib/Debug/libgtest.a \
  -o /tmp/skyengine-platform-review/async_asan_tests
/tmp/skyengine-platform-review/async_asan_tests \
  --gtest_filter=AsyncTest.DestructionDrainsQueuedSignalBeforeDestroyingSemaphore:SemaphoreTest.*
```

All 4 selected cases passed with no AddressSanitizer findings.

## Android SDK and linking

Target: NDK 28.1.13356709, arm64-v8a, Android API 31, Release dependencies in
`build_3rd/Android`. GameActivity is the official `androidx.games:games-activity:1.2.2`
AAR, with its Prefab `game-activity_static` headers/archive. The direct CMake validation
used an imported target named `game-activity::game-activity_static` pointing to the extracted
official AAR; generated Gradle projects obtain this target through Prefab normally.

```bash
cmake --build build_android --target Core Framework --parallel 4
```

Core/Framework built successfully. Native platform sources and Android launcher compiled
against the real SDK. The 4 guarded Android native regression cases passed compile checks:

```bash
NDK_CXX=/Users/lizhen/Library/Android/sdk/ndk/28.1.13356709/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android31-clang++
GAME_ACTIVITY=/private/tmp/skyengine-platform-review/game-activity-1.2.2/prefab/modules/game-activity_static
"$NDK_CXX" -std=c++20 -D_DEBUG -fsyntax-only \
  -Iengine/core/include -Iengine/framework/include -Iengine/framework/platform/android \
  -Ibuild_3rd/Android/rapidjson/include -I"$GAME_ACTIVITY/include" \
  -Ibuild_3rd/Android/googletest/include \
  engine/test/framework/platform/android/AndroidNativePlatformTest.cpp
"$NDK_CXX" -std=c++20 -fPIC -shared -Wl,--no-undefined \
  -Iengine/core/include -Iengine/framework/include -Ibuild_3rd/Android/rapidjson/include \
  -I"$GAME_ACTIVITY/include" engine/templates/launcher/android/cpp/android_main.cpp \
  -Wl,--start-group build_android/engine/framework/libFramework.a build_android/engine/core/libCore.a \
  build_3rd/Android/sfmt/lib/Release/libSFMT.a \
  "$GAME_ACTIVITY/libs/android.arm64-v8a/libgame-activity_static.a" \
  -Wl,--end-group -landroid -llog -ldl -static-libstdc++ \
  -o /private/tmp/skyengine-platform-review/AndroidLauncher.so
```

The launcher linked with `--no-undefined`; symbol inspection found one copy each of
`android_main`, `GameActivity_onCreate` and the GameActivity JNI initializer. No third-party
source copies or diagnostic suppression were used to resolve upstream return-type errors.

The complete CMake FrameworkTest target also compiled and linked successfully, with the
generated linker using `--no-undefined`:

```bash
cmake -S . -B build_android -DSKY_BUILD_TEST=ON
cmake --build build_android --target FrameworkTest --parallel 4
```

`build_android/engine/test/framework/FrameworkTest` is an ELF64 AArch64 PIE executable.
Its checked-in Android entry supplies the SDK glue hook and defaults to native contracts,
Android policies and FrameClock cases. It is a headless CLI test host; it does not replace
Activity lifecycle integration on a device. The existing cache's test option was restored
to OFF after validation; the test executable was retained.

`adb devices -l` reported no device/emulator. Native Android execution, full Activity
background/surface recreation, compressed assets in an installed APK and real soft keyboards
remain device acceptance cases. Portable policy execution and SDK compilation are reported
separately above.

## Initial iOS SDK and native execution (before UIScene migration)

Device compile target: iPhoneOS 18.5 SDK, arm64, iOS 13 deployment target, Release dependencies
in `build_3rd/IOS`.

```bash
cmake --build build_ios --target Core Framework --parallel 4
```

Core/Framework built successfully; launcher and native `.mm` regression translation units
also passed device-SDK compile checks. No signing or device deployment is claimed.

The full CMake test App also compiled and linked against the device SDK, including the checked-in
UIApplicationMain entry and all native cases:

```bash
cmake -S . -B build_ios -DSKY_BUILD_TEST=ON
cmake --build build_ios --target FrameworkTest --parallel 4
```

The result is `output/bin/FrameworkTest.app/FrameworkTest` for arm64 iPhoneOS. The existing cache's
test option was restored to OFF after validation; the test App was retained.

Native execution used the installed iOS 18.6 simulator, iPhone 16 Pro, x86_64. The final
checked-in UIKit host run passed 12 cases: 6 portable state cases and 6 native UIKit cases. Native cases
exercise notification-driven pause/resume and epochs, window identity/extent/close, clipboard
and paths, pixel IME anchoring, UTF-8/backspace, real first-responder marked-text composition,
and stopping text input. The last native regression destroys its window from Close/Focus
listeners and delivers callbacks through the retained native view afterward.

FrameworkTest now has a checked-in UIApplicationMain entry point for iOS. Its default filter
selects iOS platform cases because desktop Framework tests assume host filesystem/process
services. The standalone simulator recipe compiles the same production sources, avoiding
RHI and device signing. The final checked-in-host invocation was:

```bash
python3 python/tests/run_ios_platform_tests.py \
  --runtime iOS-18-6 --work-dir /tmp/skyengine-ios-platform-final
```

All 12 cases passed with `SKY_IOS_TEST_EXIT=0`; the runner exited 0 after uninstalling its
test App and removing its temporary simulator. Compile/link/test logs are retained under
`/tmp/skyengine-ios-platform-final`. The native text-composition case used a real UITextView
first responder; real-device keyboard/rotation/background acceptance is still separate.

## UIScene migration validation

The launcher, native platform/window and native test host now use a single UIWindowScene.
The process-owned game initializes only after scene binding and survives disconnect/reconnect.
Native UIKit objects detach on scene loss while the C++ window/WindowID remain registered.
The new public `framework/platform/IOSSceneHost.h` keeps launcher code independent of private
backend classes. Global UIApplication activity observers, `UIScreen.mainScreen`, the old app
activity helper and redundant controller/layer/surface aliases were removed.

Device-SDK validation passed with iPhoneOS 18.5, arm64, iOS 13 deployment target:

```bash
cmake -S . -B build_ios -DSKY_BUILD_TEST=ON
cmake --build build_ios --target FrameworkTest --parallel 4
```

The complete test App linked successfully with the new shared scene manifest. Its configured
Info.plist contains one `SkyEnginePlatformTests` application scene, the native test SceneDelegate
and `UIApplicationSupportsMultipleScenes=false`. A final incremental build reported no work.
The production launcher also compiled and linked against the actual new Core/Framework archives:

```bash
xcrun --sdk iphoneos clang++ -std=c++20 -fobjc-arc -target arm64-apple-ios13.0 \
  -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS18.5.sdk \
  -Iengine/core/include -Iengine/framework/include -Ibuild_3rd/IOS/rapidjson/include \
  engine/templates/launcher/ios/main.mm build_ios/engine/framework/libFramework.a \
  build_ios/engine/core/libCore.a build_3rd/IOS/sfmt/lib/Release/libsfmt.a \
  -framework UIKit -framework QuartzCore -framework Foundation -framework Metal \
  -o /tmp/skyengine-platform-review/SceneLauncher
```

The result is a Mach-O arm64 executable. No signing, packaged-game launch or GPU validation is implied.

The initial real-scene simulator run passed 21 cases on a temporary iPhone 16 Pro with iOS 18.6;
that simulator was removed. After strengthening the late-layout case and adding empty-run
rejection, the final checked-in recipe was executed on an existing iOS 18.6 iPhone 16 Pro:

```bash
python3 python/tests/run_ios_platform_tests.py \
  --udid 11E7C579-01FC-4734-9245-A2E2C7C10ED9 \
  --work-dir /tmp/skyengine-ios-scene-final --timeout 600
```

All **21 cases passed**: 9 portable state/input cases and 12 native UIKit cases. Native output
confirmed `SKY_IOS_TEST_COUNT=21` and `SKY_IOS_TEST_EXIT=0`. Tests verify the real UIKit-created
scene/delegate/manifest, rejection of an unbound legacy window, activation/foreground gating,
ignored application-wide and foreign-scene activity, disconnect/reconnect with stable ID and
surface notifications, retained frame requests versus explicit stop, late layout/text callbacks,
geometry/IME units, real marked-text behavior and reentrant close/focus destruction.

The same compiled App was then launched on that simulator with
`--gtest_filter=NoSuchSkyEngineSceneTest.*`. It reported `SKY_IOS_TEST_COUNT=0` and
`SKY_IOS_TEST_EXIT=1`, and the runner rejected the run as expected. Native completion now requires
at least one passing/failing test plus successful launcher/host exit markers; zero and entirely
skipped runs cannot be reported as successful validation. Both test Apps were uninstalled.
The final reused simulator metadata records `initial_state=Booted`; its cleanup log records
uninstallation only. This run does not establish that the simulator was restored to Shutdown.

Final artifacts/logs:

- `/tmp/skyengine-ios-scene-final/{build-info.json,simulator-info.json,compile.log,link.log,test.log}`
- `/tmp/skyengine-ios-scene-zero/test.log` and `/tmp/skyengine-ios-scene-zero-run.log`
- `/tmp/skyengine-platform-review/scene-ios-build.log` and `scene-launcher-link.log`

Host validation passed **183/183 FrameworkTest cases**, including 10 native Cocoa and 9 portable
iOS policy cases. Python validation passed **12/12 cases**, including generated project scene
metadata and 7 completion-parser regressions. The original CoreTest 276/276 and ASAN 4/4 results
above remain applicable; no Core/worker implementation changed during this migration.

### Observed asset-test intermittency

An earlier full host run passed 182/183 with `AssetManagerTest.ConcurrentLoadStress` failing;
an isolated AssetManager suite passed 19/20 with the same failure. The final full run passed
183/183. These outcomes are retained in `scene-host-tests.log`, `scene-asset-suite-recheck.log`
and `scene-host-final-tests.log` under `/tmp/skyengine-platform-review`.

Read-only review found a matching pre-existing product-publication race: SaveAsset creates the
final file before its buffered writes are flushed, while LoadAsset can open and parse that file
before checking an in-flight cook. A failed/empty header then yields an unregistered type and
null result. `AssetManager.cpp` and `AssetManagerTest.cpp` have no diff from HEAD (last changed in
`f46edc84`). This migration does not change asset loading/cooking; the intermittent failure is
recorded separately rather than hidden by the passing rerun.

### SDK/runtime coverage

`xcodebuild -showsdks` and recipe metadata confirm that this workspace has the iOS 18.5 SDK and
iOS 18.6 runtime, with no iOS 27 SDK/runtime installed. The migration follows Apple's scene
requirements using iOS 13+ APIs and executes actual scene callbacks on the installed runtime.
It does **not** claim an iOS 27 SDK build, iOS 27 execution or device acceptance result.

## Final checks and limits

OpenSpec strict validation, clang-format checks on all 49 changed C++/Objective-C++ files,
ASCII source checks and `git diff --check` passed. Independent review repairs were included
in the final host run, both complete mobile FrameworkTest builds and the simulator run.

These platform-service tests establish platform contracts, not GPU rendering, distribution/signing,
real-device keyboard behavior or Windows binary compatibility. The subsequent template package
follow-up validates actual APK/unsigned IPA/macOS artifacts, bundle layout, static game registration
and Apple default backend startup; see [package validation](package-validation.md). RHI surface
recovery and signed distribution remain separate work.
Test-generated changes to the tracked `engine/test/assets/assets.db` are removed after runs.
The change remains active; no commit or OpenSpec archive was performed.
