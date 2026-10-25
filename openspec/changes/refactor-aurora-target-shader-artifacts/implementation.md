# 实施记录 — 2026-10-23

2026-10-23 实施阶段完成代码应用，任务进度 38/43；五项尚待跨平台原生验收，不能据此宣称全部平台完成。后续提交、rebase 与复验状态见文末记录。

## 当前实现

- 单目标 Slang 编译：`AuroraResource(set,binding,owner)` / `AuroraField(path)` 作为唯一正式声明身份通道，Metal 不再借辅助 SPIR-V layout 或短名字关联。
- Stage artifact 绑定精确代码字节、source/native entry、stage、规范化目标环境、实际 Slang build tag、adapter/反射规则版本、完整声明/活跃集合与内容摘要。DXIL producer 另外绑定部署的实际 dxcompiler/dxil 库内容摘要，固定加载目录并持读句柄保护同一库；路径不进入持久身份。缺失或加载失败在开发缓存查找前拒绝，失败可重试。ShaderFunction 持有不可变 artifact，Shader 合并匹配产物并建立 BindingPlan。
- 各平台产物独立：Vulkan/SPIR-V 1.5、DXIL/SM6.5、Metal/MSL2.3；macOS/iOS 的平台身份不可互换。`minimumVersion` 表达目标语言版本；MSL2.3 原生运行另要求 macOS11/iOS14。Metal 按显式 codeFormat 选择 source/metallib 并核对 payload，不猜测或降级格式。
- CPU schema 与 GPU ABI 分离；Shader 缓存 UniformUploadPlan，处理支持的 scalar/vector、MAT4 major/stride、FLOAT4 array stride、padding 与完整覆盖快路。Pass、材质与 FrameContext 参数写入使用相同计划规则；随后 `remove-unused-legacy-rendergraph` 移除了仅旧模板使用的 batch/global helper。
- **统一 Name 事务**：`ShaderParameterTransaction` 是 ResourceGroup Name encoder 与生产 PassResourceBindings 的共同前端，统一 alias/handle、类型/count、重复/重叠、inactive、必需参数、失败锁存与打包。它冻结 uniform bytes 和 adapter 资源 token。原生 materializer 经注入上下文发布新 group；图 materializer 声明 image 读取并在现有 lease 下上传/替换 draw。每 block 编码、上传各一次，RHI 不认识资产或图句柄。
- Material v3 / MaterialInstance v2：显式参数 alias、typed 值/sampler、稀疏实例覆盖、普通2D贴图的精确资源版本与 sRGB/线性语义。活跃材质引用保护已释放独立订阅的旧图片版本；缺失图片抑制绘制，不能偷偷取最新版本。inactive 仍验证声明类型、形状与原生资源类别。
- READ_ONLY / READ_WRITE、RAW / STRUCTURED、stride 全链路保留；DX12 选择 SRV/UAV 与 register space，Vulkan 保留 native set/binding 和已有 descriptor template/数组能力，Metal 使用链接时冻结的逐 stage 稀疏 direct plan。
- Cache v6、usage/distribution v2：开发编译依赖当前源与 producer；PRODUCTS 仅使用发布索引与 ABI/目标兼容检查。blob 先于 index 原子替换发布，失败保留旧记录。项目 cook/package 和可反射 export preset 传递完整 shaderEnvironments。
- Metal specialization 为每 Shader 创建私有 function，默认值也通过 empty constantValues 实例化；未知/重复 id、不支持类型、bool 非0/1和跨 stage 类型冲突明确失败。旧 iOS render-barrier API 通过 availability guard 与 checked submit 拒绝，不静默 no-op；timeline host wait 保留纳秒超时协议。
- UI 与 Windows 编辑器消费完整 stage artifact，移除旧 uint32 长度假设与编译结果字段；UI round shader 使用自己的零活跃资源布局，切换 pipeline 后重绑所需资源，ResourceGroup 在编码期间保活。
- 材质编辑器补齐 sampler 消费：设置数据在 adaptor 注册反射，视图使用通用 ReflectedFormView 和 CommandService；typed 编辑/撤销/重做写回源数据，保留 alias，保存失败保持 dirty。关闭或重建视图后旧历史命令失效。
- Windows renderer GPU fixtures 改用生产 Slang stage artifact 与显式动态 uniform policy，删除 D3DCompile/DXBC 及手工不完整反射。Indexed fixture 的 HLSL register-space 标注仅 DXIL 生效；Slang2026.9.2 在 MSL 实际生成槽冲突时明确 Unsupported，不改写地址。合法 MSL 自动分配仍使用实际目标反射。

没有新增 Aurora target、模块装载入口、业务 plugin 或大范围目录搬迁。

## 验证结果

宿主为 Intel x86_64 macOS，GPU 为 Intel Iris Plus Graphics 655；Vulkan 原生回归使用 MoltenVK。Slang 为2026.9.2。

| 验证 | 结果 |
|---|---|
| Aurora.RHITest | 184/184，含8项共享事务、6项命名 ResourceGroup、Metal specialization、稀疏地址、动态 offset、两槽在途、uniform 编码和原生读回 |
| Aurora.CoreTest / ShaderTest | 91/91、51/51；新增目标相关 downstream 身份/缓存失效及 MSL 原生冲突回归 |
| Aurora.PipelineTest / AdaptorTest / CookTest | 40/40、44/44、44/44 |
| Renderer.ModuleTest / StaticModuleTest | 10/10、11/11；材质产品冷加载加强后动态/静态各2/2再次通过 |
| Renderer CPU 相关目标 | ChunkPrepare9、Coordinator18、ParallelPrepare7、Prepare3、Products12、Resource37、State14、WriteView13通过，Contract可执行程序exit0 |
| Python / ExportPreset C++ | 61/61、6/6 |
| UIShaderTest | SPIR-V/MSL 6/6；DXIL 3项在非Windows宿主明确跳过 |
| 编辑器及 UI | SandboxModule/SandboxEditor 构建成功、8帧有界 Metal 运行 exit0；UI Metal/MoltenVK 原生 pipeline 初始化/关闭通过；真实材质及实例各1条 headless sampler 编辑/Undo/Redo/保存/销毁历史路径通过 |
| Windows GPU fixture 源 | 17个 stage × SPIR-V/MSL，共34次实际编译及 artifact/reflection 校验通过；DXIL原生执行仍待Windows |
| relocated source-free shipping | Core1/1；Metal scene、compute/fullscreen、material3/3；实际 dyld依赖不含Slang/ShaderCompiler |
| Android交叉构建 | arm64-v8a/API31，最终112+3步骤，指定Foundation/Core/Adaptor/Render/Vulkan/ShaderCompiler.Static/UIRender全部成功 |
| iOS交叉构建 | arm64/SDK18.5/deployment13，最终105+3步骤，隔离输出且无availability警告；未覆盖Mac产物 |
| 移动目标产品 | iOS/Metal、Android/Vulkan离线cook及产品校验成功；正确平台可stage，错误平台明确拒绝 |

真实材质测试覆盖 Material/Instance/Texture 保存为产品、释放内存对象、按依赖冷加载、读取图片bulk，再经SceneAssetSource和生产图桥进行四帧采样：实例覆盖与继承、sRGB转换、未就绪/缺失图片、新旧显式版本并存及旧图片保活。在剥离的shipping宿主中重跑相同流程，不加载作者shader源码或Slang。测试中的显式revision2发布使用既有资源版本协议，不代表资产通用热重载已实现。

声明通道 gate 覆盖三目标×VS/FS/CS×两宏×O0/O3共36组布局：SPIR-V/MSL24组完成代码生成，DXIL12组只完成目标layout，macOS缺少downstream DXC，未宣称DXIL实编译。实际Metal graphics reflection另验证四组VS/FS pipeline：逐stage活跃category/index与Slang metadata一致；block size192、成员offset0/8/16/80/144、MAT4与FLOAT4数组stride16匹配。Metal反射会把标准array显示为包装struct，检查其元素布局，不以降低后的成员名字建立身份。结构体alignment16和native buffer alignment32分开处理。非对称矩阵compute正确结果为(104,314,568,858)，错major负例为(306,362,414,478)。

当前Slang SDK公共API探针验证downstream搜索实际请求为绝对目录/dxil及目录/dxcompiler；相同目录的失败会被缓存，清空后重设目录可重新探测。该结果验证路径与重试协议，不代表Windows DXIL代码生成已执行。

## 可重复入口

```sh
cmake -S . -B build
cmake --build build --parallel 8 --target Aurora.RHITest Aurora.CoreTest Aurora.ShaderTest Aurora.PipelineTest Aurora.AdaptorTest Aurora.CookTest Aurora.Renderer.ModuleTest Aurora.Renderer.StaticModuleTest ShaderCookTool Aurora.ShaderCompiler FrameworkTest SandboxModule SandboxEditor UIShaderTest
python3 -m unittest discover -s python/tests
python3 python/tests/run_shader_package_tests.py --build-dir build --bin-dir output/bin --tool output/bin/ShaderCookTool --target metal
OPENSPEC_TELEMETRY=0 openspec validate refactor-aurora-target-shader-artifacts --strict
```

GPU测试需要正常桌面GPU访问；受限沙箱不能枚举Metal/MoltenVK设备，不能将这种环境失败当成原生回归结果。现有跨平台build目录复用原toolchain配置，iOS输出固定到`build_ios/target-artifact/bin`，deployment13交叉编译不改变MSL2.3的iOS14运行门槛。

当前会话日志在`/private/tmp/`：`sky-target-*-all-final-11.log`、`sky-target-shipping-reload-final-12.log`、`sky-target-material-reload-native-12.log`、`sky-target-static-material-reload-native-12.log`、`sky-target-shared-transaction-{android,ios}-{final,increment}.log`、`sky-target-mobile-product-identity-final.log`、`sky-native-graphics-artifact-gate-final.log`、`sky-target-python-all-final-7.log`、`sky-target-export-preset-final.log`。这些是当前机器诊断日志，不是产品依赖。

消费方收口日志：`sky-target-consumers-final-build-20.log`、`sky-target-producer-core-native-18.log`、`sky-target-final-{shader,adaptor}-test-20.log`、`sky-target-final-shipping-21.log`、`sky-target-final-ui-shader-21.log`、`sky-target-producer-{android,ios}-final-19.log`、`sky-target-consumers-{android,ios}-increment-20.log`、`sky-target-editor-native-smoke-20.log`、`sky-ui-native-artifact-smoke-final.log`、`sky-material-editor-probe.log`、`sky-gpu-fixture-source-gate-final.log`、`sky-slang-dxc-path-probe.log`。

## 尚未验收的边界

- 5.4/5.5：当前无Android设备，xcdevice只列本机Mac；移动GPU读回、应用部署与无编译器消费待实机。
- 5.6：Windows的实际DXIL/SPIR-V编译、D3D12 debug layer SRV/UAV读回及两条Vulkan raster路径待Windows宿主。当前Mac/MoltenVK结果不能替代该验收。
- 5.9：已有Linux preset与Vulkan产品选择矩阵已核查；当前Core/Framework平台源选择没有Linux分支，既有宿主/异步原语基础和实际Linux执行为前置，未扩大平台移植。
- 5.10：真实材质Name采样在Metal开发及shipping通过；三后端整体实机验收仍未完成。
- 有界编辑器运行仍输出 SkyCocoaView/SkyCocoaWindowDelegate 被 executable 与 SandboxModule 同时实现的 ObjC 类重复告警；本轮 exit0，shader change 未调整这组既有静态模块链接归属。UI MoltenVK 初始化另有 VS 未被 FS 消费输出的性能提示，无 validation error。
- Metal direct descriptor arrays、argument buffers、typed/counter buffers、嵌套uniform及本期未支持字段/材质资源形态继续显式Unsupported。MSL文本shipping仍使用Apple原生Metal编译器，剥离的是作者源文件与Slang/DXC，不等同于强制离线metallib。

旧shader index/blob、usage/distribution和材质产品必须迁移并recook。未自动commit、archive或同步archive所管理的主spec；后续操作继续遵循AGENTS.md确认规则。

## Rebase origin 后复验 — 2026-10-23

`dev_refactor_rhi` 从 `861a4eb6` rebase 到 `origin/dev_refactor_rhi` 的 `b5247705`，吸收三个上游提交：相机/操纵器控制、Vulkan surface 查询优化、Develop 配置及独立诊断开关。当前 HEAD 与该远端引用一致，没有额外本地提交。

原234个已修改文件及23个未跟踪文件在操作前完整备份；恢复后，未交叠且未额外修复的244个文件逐一核对 SHA-256 一致。六处冲突保留双方行为：构建输出目录、FrameScheduler 两类回归、SandboxModule 和三个 EditorRenderer 文件。其余自动合并的 Vulkan 生命周期检查、Cocoa modifier、shader artifact 与产品部署逻辑另行复核。所有改动恢复为未暂存状态，没有 commit/archive。

本轮修复：

- Develop 输出目录从已解析的基础目录派生，保留显式配置覆盖；shader staging 跟随单配置及多配置的实际目标目录，补真实 staging 回归。
- 修复上游 Develop flags 初始化：真实 C/C++ 工程已存在的空 cache 原先阻止默认 flags 生效；现在填充空值并保留非空覆盖。真实编译器配置回归验证 `-O2 -g -DNDEBUG` 及用户覆盖。
- 新增相机设置路径的 `PropertyModel` 显式传入 `PropertyObject`，消除 Clang 构造歧义，保留通用反射与撤销行为。
- 修复 AnimationPose、VulkanFence/Queue、AuroraModule、RendererHost 的 printf 参数类型，Android 最终增量中对应警告已消失。

| 本轮验证 | 结果 |
|---|---|
| macOS Release 构建 | 所有指定目标构建成功；最终 dry-run 无编译/链接 dirty，仅资源 staging 与 shader header check 的既有 phony 任务 |
| Release foundation / editor | Core306、Framework196、UICore190、UIShader6、EditorCore177、EditorRender8、EditorShell136 全通过；UIShader DXIL3项非Windows跳过 |
| Release Aurora | RHI184、Core91、Shader51、Pipeline40、Adaptor44、Cook44、Module10、StaticModule11 全通过；合计本轮 Release1494项通过 |
| 隔离 Develop | 指定目标263步构建成功；Core306、RHI184、Core91、Shader51，共632项全通过且无跳过；实际优化/调试 flags、生效断言和resource name ABI均已核对 |
| shipping 重 cook | 剥离作者源码、Slang/DXC 的宿主 Core1/1、Metal场景/计算/全屏/真实材质3/3通过 |
| 编辑器 | 新增camera/gizmos与frame pacing相关单元测试通过；SandboxEditor 8帧有界Metal运行exit0 |
| Android | arm64-v8a/API31，首轮260步及最终10步增量通过；编译/链接产物均更新 |
| iOS | arm64/SDK18.5/deployment13，首轮247步、重配后的248步及最终6步增量通过；输出保持隔离，无availability警告 |
| Python / OpenSpec / diff | Python67/67；本change及两个接续change的strict、`git diff --check`通过 |

Develop 程序、后端及资源位于 `build_rebase_develop/runtime/Develop`；该验证树仅加入本机 `.git/info/exclude`，没有改用户 `.gitignore`。原文件备份保留在 `/private/tmp/sky-origin-rebase-20261023T015358Z`，可恢复 stash 为 `cc700f1759f01c35daf728588d36c6f53a38c062`。

本轮环境边界：Android三方strict预检通过；iOS三方strict仅因既有tool-only `ios-cmake` 无安装输出目录而失败，实际toolchain位于intermediate且无missing/stale包。第三方 `ios-cmake` 在重新配置时会重复追加cache flags，已确认这是iOS额外全量重编的原因；最终检查无编译/链接或命令dirty，不把每次执行的phony staging描述成无工作。没有可用移动真机；Xcode列出模拟器和不可用的iOS设备，ADB列表为空。Windows/Linux及移动实机五项原生验收继续保持未勾选。既有Cocoa重复ObjC类和三方链接版本告警仍见前述限制，本轮未扩大模块归属调整。

本轮日志在 `/private/tmp/sky-rebase-*`：`macos-build-final.log`、`macos-no-compile-final.log`、`test-<suite>.log`、`develop-{configure,build,core,rhi,aurora-core,shader,policy-tests}.log`、`{ios,android}-format-final.log`、`{ios,android}-no-compile-final.log`、`python-final.log`、`shipping-final.log`、`editor-smoke.log`。

## Windows 验证交接 — 2026-10-23

准备推送到 `origin/dev_refactor_rhi_back` 供 Windows 原生验证。交接前再次 `fetch --prune origin`，并执行 `rebase --autostash origin/dev_refactor_rhi`；远端基线仍是 `b5247705`，没有新增上游提交。rebase 前后逐文件内容摘要一致。FrameworkTest 改写的 `engine/framework/test/assets/assets.db` 已备份并恢复，不作为实现修改提交；Python 67/67 再次通过。

在 Windows 使用现有 VS2022 构建树。先关闭编辑器，预检三方库，再重新配置；本次新增源文件需要重新配置才能进入 GLOB。以下命令在仓库根执行，各步骤成功后再继续：

```powershell
taskkill /IM SandboxEditor.exe /F
python python/third_party.py -p Win32 -o D:/Code/sky3rd_win32_output --check
cmake -S . -B cmake-build-debug -DSKY_BUILD_TEST=ON -DSKY_BUILD_TOOL=ON -DSKY_BUILD_SANDBOX=ON
cmake --build cmake-build-debug --config Debug --parallel
python -m unittest discover -s python/tests
python python/tests/run_shader_package_tests.py --build-dir cmake-build-debug --config Debug --target vulkan --target d3d12
$env:SKY_SHADER_PACKAGE_ROOT = (Resolve-Path cmake-build-debug/shader-package-test/relocated/shaders).Path
ctest --test-dir cmake-build-debug -C Debug --output-on-failure
```

三方预检失败时先按输出更新对应包，尤其确认 Slang 与部署的 `dxcompiler.dll` / `dxil.dll` 完整匹配。GPU 测试需要支持目标能力的真实桌面设备、Vulkan 驱动和 D3D12 debug layer；应记录 skip 的具体测试及原因，不能将其算作通过。shipping runner 会重新 cook Vulkan/DXIL 并运行剥离源码及编译器的 Core/renderer 消费方；缺 DLL、编译失败、原生 skip 或读回失败都会令该入口失败。

优先检查 `Aurora.RHITest`、`Aurora.ShaderTest`、`Aurora.Renderer.GpuSubmissionTest`、`Aurora.Renderer.ModuleTest` 和 `UIShaderTest`，再启动 `output/bin/Debug/SandboxEditor.exe --console` 验证场景、材质、UI、浮动窗口与关闭/缩放。目前真实 Material/Instance/Texture 冷加载与 Name 更新的完整四帧采样 fixture 仅在 macOS 编译，Windows 常规测试及 shipping runner 不能冒充这项跨三后端验收；5.6/5.10 继续待 Windows 原生证据。

交接前整体 review 修复两处 P2：DX12 numeric descriptor 的 uniform 逻辑范围必须覆盖实际反射 block，不能依靠 256 字节 CBV backing 对齐掩盖短范围；新增 Windows 回归使用真实 DXC 的 192 字节 block，同时检查 static/dynamic、短范围拒绝、失败锁存与下一事务恢复。编辑器及 launcher 不再把普通 `ViewportFailed` 升级为全局终态；需要替换的编辑器 surface 保留 native lease，等待精确 index/generation 的退休回执后递增 generation、重建并同步 UI 引用，关闭交错不重建。终态由 `RendererFailure` 驱动。

上述修复经第二位 reviewer 独立复核；`EditorRender`、`EditorRenderTest` 和 `Aurora.AdaptorTest` 增量构建成功，EditorRenderTest 11/11、AdaptorTest 45/45 通过，覆盖重试、终态、背压、错误回执及关闭/替换交错。运行日志为 `/private/tmp/sky-precommit-editor-render.log`、`/private/tmp/sky-precommit-adaptor.log`。Windows 新增的 DX12 用例仍需实机执行：`output/bin/Debug/Aurora.RHITest.exe --gtest_filter=ResourceGroupTestD3D12.*`。当前预览编辑器保持运行，最终修复未重链接其已加载模块；没有将旧预览进程计作这些修复的运行验证。

## 再次 rebase 与全量复验 — 2026-10-24

起点为已提交并推送到 `origin/dev_refactor_rhi_back` 的 `61519579`。本轮同步 origin 后，将本地 `dev_refactor_rhi` rebase 到 `60079ac7`（viewport picking / selection outline），amend 前的重放提交为 `ba6be13c`。恢复引用为 `codex/pre-origin-rebase-20261024`。本轮未推送，也未执行新的 OpenSpec archive。

八处冲突合并保留了双方语义：typed material parameters 与 image version pins、供 selection 使用的精确 entity identity、EditorShell 原生测试依赖、跨平台 editor/picker 接入，以及既有 viewport retry/replace/native lease 状态机。新 outline 路径改用 `UniformData` / `Apply(context)`，两个 Selection 声明头通过当前 exporter 生成正式属性与目标寄存器规则；Windows 新 fixture 同步迁移到 Name aliases。材质纹理 reload 的 payload 和引用使用同一刷新 revision，回归验证新旧精确版本和旧像素快照独立。

| 验证 | 本轮结果 |
|---|---|
| macOS Release 全量构建 | 568 步成功，包含编辑器、工具、全部已启用模块和测试目标 |
| Release CTest | 49/49 套件通过，合计 1997 项 gtest 通过；UIShader 的 3 项 DXIL 编译用例在非 Windows 跳过；ContractTest 为非 gtest 可执行程序且成功 |
| 新上游相关套件 | Core311、EditorRender50、EditorShell141、Pipeline43、Adaptor50、Module11、StaticModule12 通过；均计入上行统计 |
| 真实 Metal selection outline | 64×64 四帧读回通过：baseline、exact entity selection、清除、stale generation；背景外沿增加描边，物体中心不变，清除/过期选择恢复原图；该回归计入 Module/StaticModule |
| 隔离 Develop | 175 步构建成功；Core311、RHI184、Aurora Core91、Shader51、Pipeline43，共 680 项通过、无跳过 |
| source-free shipping | 重新 cook 包含新 Selection shader 的产物；剥离作者源和 Slang 的 Core1/1、Metal scene/compute/fullscreen/material3/3 通过 |
| Android 交叉构建 | arm64-v8a/API31，128 步指定 Foundation/RHI/Core/Pipeline/Adaptor/Render/Vulkan/ShaderCompiler.Static/UIRender 构建成功 |
| iOS 交叉构建 | arm64/SDK18.5/deployment13，249 步指定对应目标构建成功；输出保持 `build_ios/target-artifact/bin` 隔离 |
| 编辑器启动 | SandboxEditor 8 帧有界 Cocoa/Metal 运行 exit0 |
| Python / OpenSpec | Python67/67，三个关联 change strict、生成 header 检查通过 |

Python 的声明库存断言由 11 更新为 13，并明确检查两个新 Selection export 的正式属性。本轮额外加入的 Metal 描边回归、库存断言修复与本记录一并纳入本次 amend 清单；上述验证覆盖 `ba6be13c` 及这些补充改动。测试改写的 `engine/framework/test/assets/assets.db` 已备份并恢复，未留下生成数据。

平台边界不变：本机不能执行 Windows 原生 DXIL/D3D12 验收，本轮也未执行移动真机测试。iOS 三方预检没有 missing/stale 包，但既有 tool-only `ios-cmake` 安装输出目录缺失，实际 toolchain 文件可用；MSL2.3 的 iOS14 运行门槛不因 deployment13 交叉编译而降低。既有 Cocoa 重复 ObjC 类、Apple API deprecation 与三方库 deployment/link 告警未扩展处理。

日志和完整测试输出位于 `/private/tmp/sky-oct24-*`：`build.log`、`ctest.log`、`ctest-full.log`、`ctest-summary.json`、`develop-{build,ctest,ctest-full}.log`、`develop-summary.json`、`{android,ios}-build.log`、`metal-outline.log`、`shipping.log`、`python-final.log`、`editor-smoke.log`。CTest JUnit 的成功输出有 1024 字节截断，gtest 总数由保存的完整 LastTest 输出统计。


## 2026-10-25 四组资源与三域变体审计修复

基线为 `69e33334` 上用户的暂存/未暂存工作树；本轮按授权修复 R1–R12，保留既有 Aurora foundation、target 和装载入口。完整问题映射与历史失败见 [渲染审计](../../../docs/aurora/rendering-design-audit-2026-10-25.md#同日修复记录2026-10-25)。未 commit/archive。

当前 index/blob **v9**（usage/distribution v2、Material v3、instance v2）：发布选择 schema 的 Material/Vertex/Pipeline 域、Vertex 条件及 Pipeline 预留位。运行时从真实 mesh 属性/PER_INSTANCE 流、材质值和 View Pipeline 设置选择；无源码模式不再吞掉布局错误。stage 分别规范化自己的 schema，specialization 命名值保留到 DXIL lowering。

完整 Material set 2 与 Batch set 3 分离，SceneSurface/UI/旧 Execute 对齐；带 context 的统一事务保留 storage 类型与精确范围。入口校验和预算覆盖所有执行 variants。RendererEvent 全类型交付恢复。

异步任务拥有 compiler/module/cache root/writer，重复调度只执行一次。真实 DLL 验证揭示并修正最后一次 release 的控制块自卸载问题：registry 分别持 compiler/module；消费侧分配 lease，compiler/deleter 的销毁先于模块释放。既有同步借用仍要求调用方负责生命周期。

Windows 最终定向构建成功。Core100、Pipeline55、Adaptor62、Cook52、Shader57、Resource39（合计365）、RHI154、UI Shader9、动态/静态模块生命周期6/7均通过。宿主/Runtime/骨骼原生回归177项：首轮174通过，另3项过期 set 2 断言修正后全部复验通过；不宣称完整GPU suite已完成。真实发布包剥离源码/编译器后Core1、Vulkan2、DX12 2通过；无源SceneAssetSource的SHADOWS/IBL四组合测试包含在Adaptor62中。

Python shader_headers10项、导出check、当前change strict与diff check通过。最终证据在 `cmake-build-debug/aurora-fix-{build-final,close-*,package-native,headers-final,openspec-final}.log`。原生测试先部署 ShaderResources.Stage，避免把旧部署资源与当前C++混用。静态Batch期望值和异步测试夹具已迁移；完整拾取交互仅冷启动M/V/P编译阶段使用60秒时限，后续交互不变。

Metal compute四组和raster storage代码已修复，本轮无Apple原生构建/执行证据；原跨平台、全材质端到端gate及6.7保持未完成。2026-10-23及后续历史记录不改写为本轮结果。旧v8及更早shader产品必须recook。
