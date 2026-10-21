> 当前状态：本 change 的 Slang→C++ 镜像生成链已由 `refactor-aurora-shared-pass-headers` 替代；以下完成记录保留为阶段历史，不能再作为当前构建规则。

Status: active（C01/C02/C03 Windows 实现与验收完成；2026-10-21 用户确认按资源绑定、World、编辑闭环、Shadow、IBL/HDR 顺序推进；macOS/移动端待 P02，archive/commit 待用户确认）。

## 5. C03 完整资源声明与绑定

- [x] 5.1 从同一 Slang 声明生成完整资源反射、类型化槽位及常量镜像；资源-only 声明可生成，非法/冲突声明明确失败
- [x] 5.2 增加 Pass 绑定装配，按最终 shader 的实际资源子集验证 Global/Pass/Batch，拒绝缺失、重复、类型与布局冲突
- [x] 5.3 场景 Pass 改用通用绑定装配，保留 View 可选和显式 dynamic 策略；TextureToScreen 定义真实纹理/sampler 输入
- [x] 5.4 CPU 测试覆盖资源生成、绑定负例和可选声明；Windows 三模式 GPU 读回验证图内生产/采样、多输出及在途隔离
- [x] 5.5 回填验证、格式检查与 OpenSpec strict；不把完整声明描述等同于所有后端类型均可执行

后续顺序：World/资产转换与编辑器接线归 aurora-scene-bridge；Shadow 的深度采样/纯深度附件、常量数组与灯光扩展，以及 IBL/HDR 能力分别在相应实施 change 验收。当前 C03 不改已有模块/target 归属，不改 PreparedRaster 的几何与版本解析职责。

### 2026-10-21 C03 验收

- ShaderHeaderTool 从完整 ShaderReflection 生成常量镜像、`<Source>Bindings` 与 `Get<Source>Reflection()`；资源-only 声明可直接生成。重复槽位、未知形状、无匹配 block 和未支持常量类型明确失败且不发布部分 header。
- `PassResourceBindings` 用完整 set/binding 匹配生成 schema 与最终 shader reflection；覆盖 Global/Pass/Batch 三组组合、无 View、可省略的反射外声明、字段/形状/范围及 dynamic 策略冲突。失败保留原 draw，成功沿用既有 graph/lease/native descriptor 路径。
- SceneRenderFeature 迁移到该装配器；TextureToScreenPass 同源声明真实 texture/sampler，新增生产 `TextureToScreenFeature` 接入 FrameGraphBuildContext。输入支持本帧 `ViewOutputRef` 或精确 `RenderResourceKey`，不存储跨帧图 handle。
- Windows Vulkan Dynamic Rendering、Vulkan RenderPass、DX12 三模式均通过新增 3 项 × 3 GPU 用例：上游生产/下游采样和多输出连续帧、旧 output revision 失败后 Discard/resize 恢复、InGraph 上传采样与新旧资产版本同时使用/释放保活。既有 no-View、static/dynamic Batch 和错误布局回归通过。
- 独立工具 fixture 验证资源-only 的 Cube 数组、comparison sampler、storage buffer：实际生成 `.gen.h/.gen.slang` 后 MSVC 编译并运行元数据检查成功。记录在 `cmake-build-debug/pass-contract-c03-fixture/verify.log`。这里只验收声明表达，尚不支持这些类型的 raster 执行；常量数组和嵌套结构仍待后续。
- 重新配置并构建 ShaderTest、PipelineTest、GpuSubmissionTest、SandboxModule 及依赖通过，使用本机临时 `_CL_=/Z7`。补充测试构建曾与 GPU 测试占用 DLL 冲突，测试退出后顺序重建成功；未修改工程构建配置。

| Windows 验证 | 结果 |
|---|---|
| Aurora.Renderer.GpuSubmissionTest | 867 passed / 3 skipped；870 cases；此前偶发失败未出现 |
| Aurora.PipelineTest | 32/32 |
| Aurora.ShaderTest | 45/45 |

- GPU 的三个 skip 均为 DX12 参数下的 Vulkan surface 专属用例。日志/XML：`cmake-build-debug/pass-contract-c03-gpu.*`、`pass-contract-c03-Aurora.PipelineTest.*`、`pass-contract-c03-Aurora.ShaderTest.*`；构建日志 `pass-contract-c03-build-final.log`、`pass-contract-c03-checks-build.log`。
- 修改范围 clang-format dry-run/Werror、git diff --check 与 OpenSpec strict 通过。macOS/Metal、移动端未运行，不将 Windows 结果作为实机对等证据。World bridge、编辑闭环、Shadow 与 IBL/HDR 尚未实施；本轮未 archive、commit、rebase 或 push。

## 1. 契约与归属

- [x] 1.1 对照用户要求、既有 shader codegen 方案和 B08 实现，记录偏离及受影响文件
- [x] 1.2 创建 proposal/design 和三份 capability delta，明确 Global/Pass（View 可选）/Batch、同源生成及验收范围
- [x] 1.3 修正规则/当前设计说明，将固定 View/Material/Object 标记为待纠偏实现，更新 extensions 归属
- [x] 1.4 OpenSpec strict 校验通过

## 2. C01 Pass 定义与 CMake 生成

- [x] 2.1 复用 ShaderHeaderTool 输出同源 C++ 与 shader include，显式处理生成失败
- [x] 2.2 为 Global 和已有 Opaque/Scene/TextureToScreen Pass 接通 build-tree 输出、输入依赖和消费者依赖
- [x] 2.3 将具体 Pass 的手写 RgBlockDesc 替换为各自 Slang 定义的生成结果，移除源码树 Global 生成头的消费
- [x] 2.4 配置显式交叉编译 host 工具，缺失工具明确失败，不执行 target 平台二进制
- [x] 2.5 验证干净生成、无变更不重生成、声明变化触发重生成、非法声明阻止构建
- [x] 2.6 验证 C++ 镜像与 shader reflection 的字段/大小/绑定一致，并运行相关 Shader/Pipeline 测试

## 3. C02 生产 Renderer 契约纠偏

- [x] 3.1 固定具体 Pass 到 renderer 的 owned 数据入口和接口差异，复用现有 feature/context；如涉及模块/target 职责调整，先完成根 AGENTS 要求的方案确认
- [x] 3.2 让具体 Pass 使用生成定义提供 Global/Pass/Batch 数据，View 按需提供，移除 PreparedRaster 对三块常量的布局决策
- [x] 3.3 RendererRuntime 按实际声明/反射校验，移除强制三个 set、80/64 字节、Material=set 1 及按 set 推导 dynamic
- [x] 3.4 同步迁移生产入口、最小场景与宿主测试，不保留无提示的固定 ABI fallback
- [x] 3.5 覆盖有/无 View、不同 Pass 布局、两 View/两帧在途和布局切换的实际 GPU 读回
- [x] 3.6 覆盖错误布局/范围的显式失败，验证 InGraph、精确版本 pins、slot 和退休保活

第 3.1 项的具体方案已写入 `design.md` 第 6 节：复用 feature/context，Pipeline 定义场景 Pass，新增 Adaptor → Pipeline 单向依赖并由宿主显式装配。用户于 2026-10-20 回复“下一步”确认继续该方案；C02 Windows 验收证据见下。

## 4. 回归与收口

- [x] 4.1 Windows 相关目标构建及 CPU/GPU/宿主回归，包含现有 Compute/Custom 测试
- [ ] 4.2 macOS/移动端按可用环境验证；未执行项目保留未验收并记录明确的 P02 接续
- [x] 4.3 更新当前说明与验收证据，OpenSpec strict、格式和 diff 检查
- [ ] 4.4 用户确认后 archive/commit；实现与本 change 的 spec/archive 同一提交

### 2026-10-20 C01 验收

- 生成器与消费者沿用现有库：`Core → ShaderHeaderTool → ShaderHeaders → Pipeline`，未新增运行时模块、目录搬迁或 Core 到 Pipeline 的反向依赖。
- Global 与 Opaque/Scene/TextureToScreen 的 `.gen.h/.gen.slang` 从同一 Slang 声明生成到 build tree；三个 Pass 不再手写 RgBlockDesc，Global 分配使用生成类型的 sizeof，不再重新计算镜像布局。
- 初次跨目标测试发现仅有 `vk::binding` 时 DXIL 将 Pass 分配到 space 0；在同一声明补充 `register(space1)` 后，SPIR-V/DXIL 的绑定、块大小和字段偏移均通过。未修改测试预期掩盖差异。
- 关闭编辑器后重新配置并构建 `Aurora.PipelineTest`、`Aurora.ShaderTest` 及依赖，通过；沿用本机临时 `_CL_=/Z7`，未修改工程调试选项。沙箱内 MSBuild 停在 Checking Build System，停止后在获准的执行环境构建成功。
- `Aurora.PipelineTest` 22/22，`Aurora.ShaderTest` 42/42。日志：`cmake-build-debug/pass-contract-build.log`、`pass-contract-pipeline.log/.xml`、`pass-contract-shader.log/.xml`。
- 独立 CMake fixture 验证六项通过：干净生成、no-op 时间戳不变、声明变化更新两侧、非法声明阻断本次及再次构建、恢复、缺少跨编译 host 工具显式失败。记录：`cmake-build-debug/pass-contract-fixture/summary.json`、`verification.log`。
- 以上只证明 C01 与 Windows 编译/反射结果；未执行 Metal/iOS/Android 实机。C02 的生产固定 ABI、不同布局真实读回及宿主回归仍未完成，不将第一批结果视为整体纠偏完成。

### 2026-10-20 C02 验收

- Pipeline 的 `SceneRenderFeature` 消费生成的 ScenePass/SceneBatch 类型及反射布局；ScenePass 的相机块按 shader 声明可省略，Batch 同时覆盖 static/dynamic 绑定。同一 View 内相同类型的 Pass 常量复用，跨 View/帧按 lease 分配。
- Core 新增通用 `ResolveDraws` / 显式类型 `WriteUniform` / `CreateImage`；移除 PreparedRaster 的 View/Object struct、固定 UBO 写入与 RendererRuntime 的三个 set/80/64 字节校验。准确的 mesh/material 版本、LOD/range、InGraph 与完整 catalog/feature owners 保活沿用原协调器协议。
- `RegisterScenePass` 由 Launcher/Windows Editor 首个 packet 显式调用，持真实 ModuleManager lease；Adaptor 单向链接现有 Pipeline。没有新增 runtime target、目录搬迁或 Core 反向依赖；缺少场景 feature 时不隐式出图。
- 生成头新增只读反射布局 accessor，用于比较真实 shader 的字段类型/大小/偏移，避免另写字段表。宿主 shader fixture 改为 include 生成 Slang，CPU 使用同源 SceneBatch 镜像。
- 新增 5 项 × 3 后端模式的 GPU 用例全部通过：80 字节 ScenePass 与 16 字节另一 Pass 同帧切换、双 View/连续排队帧读回、无 View shader/static Batch、错误生成布局及失败后恢复、uniform 范围不足、无场景 feature 时无 fallback。双在途 slot 的原 `FrameSlots.TwoSlotsKeepCommandsAndOwnedPrepareProductsUntilCompletion` 和 `FrameAllocations.TwoInflightSlotsSurviveEmptyAndSkippedOutputs` 也在全量回归中通过。
- 范围负例发现并修复 `WriteUniform` 绑定对齐分配长度的问题：现按实际输入 bytes.size 绑定，分配 padding 不再掩盖参数不足。资源隔离用例先等待 mesh/material Resident，再连续提交两帧；首次 InGraph 上传和迟到/热更新资源继续由原回归覆盖。
- 关闭编辑器后重新配置，相关目标（GpuSubmissionTest、PipelineTest、ShaderTest、ModuleTest、StaticModuleTest、SandboxModule）构建通过；沿用本机临时 `_CL_=/Z7`，未修改项目调试选项。

| Windows 验证 | 结果 |
|---|---|
| Aurora.Renderer.GpuSubmissionTest | 858 passed / 3 skipped；861 cases，包含既有 Compute/Custom 与宿主用例 |
| Aurora.PipelineTest | 23/23 |
| Aurora.ShaderTest | 42/42 |
| Aurora.Renderer.ModuleTest | 5/5 |
| Aurora.Renderer.StaticModuleTest | 6/6 |
| Aurora.RHITest | 133/133 |

- 模块测试的旧 HostWindow fixture 最初未提供 ModuleManager，不能取得场景 feature 的代码 lease；补上真实 manager 后重建，动态/静态两组全部通过。未放宽生产 lease 校验。
- 日志/XML：`cmake-build-debug/pass-contract-c02-Aurora.*.log/.xml`；构建日志 `pass-contract-c02-build.log`、`pass-contract-c02-modules-build.log`；新增用例 `pass-contract-c02-new.log/.xml`。完整 GPU 回归的 3 个 skip 是 DX12 参数下的 Vulkan surface 专属用例。
- OpenSpec strict、修改 C++ 的 clang-format dry-run/Werror、git diff --check 均通过。Metal native、iOS/Android 尚未运行，已明确转接 `aurora-renderer-platform-parity` 8.11；Windows 的 MSL 编译测试不等于 Metal 设备验收。
- 本轮未 archive、commit、rebase 或 push；原 Compute/Custom 阶段修改仍保留，各自 change 的代码/spec 归属需要在提交时一并核对。
