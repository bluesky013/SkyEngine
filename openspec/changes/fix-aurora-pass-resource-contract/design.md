## Context

> 以下偏离表记录 2026-10-20 的纠偏背景；生成器相关选型已被第 1、2 节的共享头方案替代。当前实现无需恢复该生成链。

用户于 2026-10-20 明确：Global / Pass（View 可选）/ Batch；Pass 定义 header，CMake 阶段生成，shader 与引擎消费同一份源。此要求与 `archive/2026-09-12-aurora-shader-header-codegen` 的三层不变、Slang 单一事实源和构建生成一致。

偏离证据：

| 位置 | 现状 | 纠偏 |
|---|---|---|
| `core/src/renderer/graph/PreparedRaster.h/.cpp` | 手写 View/Object 结构，固定 Material=set 1，按 set 推导 dynamic | 消费 Pass 提供的布局/数据，保留资源解析与生命周期职责 |
| `core/src/renderer/RendererRuntime.cpp` | 强制三个 UBO、固定 80/64 字节、三个 set 必须齐全 | 校验实际 Pass/Batch 声明与 shader reflection，不规定业务数据形状 |
| `pipeline/src/{OpaquePass,ScenePass,TextureToScreenPass}.cpp` | 手写 `RgBlockDesc` 字段 | 从对应 Pass 的 Slang 定义生成 |
| `shader/CMakeLists.txt` | 手动运行工具、提交 Global 生成头 | CMake 明确输入/输出/依赖，生成到 build tree |
| `pipeline/CMakeLists.txt` | 引用不存在的 ShaderHeaders target | 真实生成依赖，消费方编译前完成生成 |

B08 的归档记录说明其为“首版固定 UBO ABI”，不能解释为授权取代三层资源模型。归档历史保留；当前规则文档明确记录这段实现正在纠偏。

## Goals / Non-Goals

**Goals**

- 保持 Global=0、Pass=1、Batch=2；View 是否使用由具体 Pass 决定，数据来源和更新频率不等于强制新增 descriptor set。
- 每个 Pass 拥有自己的声明源，C++ 类型、大小/偏移和 shader include 同源生成。
- 生产路径去掉固定示例 ABI，沿用已有图内资源声明、lease、版本 pins 和退休安全。
- 用两种不同 Pass 布局、无 View Pass、两 View/两帧在途和 native 读回证明契约。

**Non-Goals**

- 不将 Global/Pass/Batch 改名为 View/Material/Object；不将 Material 固定占用 Pass 层。
- 不重做 RHI、后端 compiler、Compute/Custom 同步模型或线程调度。
- 不恢复旧的隐式 frame ring，不移动目录，不创建 Aurora plugin/第二套 renderer 模块。
- 不在本 change 补齐 HDR、光照、完整材质资产系统或 async compute。

## Decisions

### 1. 共享声明与构建（由 shared-pass-headers 修订）

2026-10-21 用户确认以共享 `.h` 取代 Slang 反射生成 C++ 镜像。Global/Pass/Batch 层级、可选 View 与绑定语义不变。字段和资源列表写一份，C++ 直接得到 native struct/sizeof/offsetof 与槽位，shader 使用同一源；编译反射校验两侧布局。详细规则与验收见 `refactor-aurora-shared-pass-headers`。

### 2. 构建边界

Pipeline 直接包含共享头，仅依赖原有 Core/RHI。CMake 仅嵌入源码字符串；ShaderHeaders、镜像生成器与 host-tool 前置退出生产构建。Core 不引用具体 Pass；ShaderHeaderTool 仅保留独立 variant 功能。跨编译不再需要 host 工具获得 Pass C++ 类型。

### 3. Pass 定义语义，底层只处理声明

Global 是全局资源层；Pass 是具体 Pass 资源层；Batch 是绘制批次资源层。材质参数与对象参数是否组合、使用哪些 binding，由 Batch/具体 Pass 的 shader 契约表达，不能靠 `set == 1`、块名或字段名推导。

View 是可选输入：场景 Pass 可将相机相关数据填入它声明的块；Fullscreen/无相机 Pass 可不声明。不得要求所有 material 具备固定 View UBO，也不得把可选 View 实现为隐藏的第四层。

不可变 shader/layout/PSO 可以共享；可变 Global/Pass/Batch 数据按实际帧/Pass/View 使用分配并由 lease 保护。多 View 不通过反复覆盖共享常量实现。Batch 的动态偏移来自声明和反射，保持排序、对齐、范围与图内访问一致。

### 4. PreparedRaster 保留解析职责，退出布局决策

保留精确版本 mesh/material 解析、LOD/range/device epoch 校验、图内 InGraph 资源解析、pins 保活和 RasterDraw 组装。将生成常量与业务布局选择交给具体 Pass；移除 `PreparedViewConstants`/`PreparedObjectConstants` 作为通用 ABI 的地位。

`RendererRuntime::Validate` 仍检查 owned payload、shader/backend 一致性、声明重复/范围、反射与实际数据匹配，但移除：

- 固定要求三个 set 全有；
- set 0 必为 80 字节、set 2 必为 64 字节；
- set 1 必为 material bytes；
- set 0/2 必为 dynamic、set 1 必为 static 的硬编码。

不能仅删除检查而保持错误的数据组装。新的 Pass 数据入口、生产构图调用者和测试必须同批迁移后才切换运行路径。不同 shader 使用同一 set/binding 但布局不同，必须分别验证并绑定正确数据。

### 5. 验收按行为分批

- C01 原镜像生成验收是历史记录；当前验收改为共享头直接编译、文本嵌入增量更新和逐目标反射一致，见后续 shared-pass-headers change。
- C02：生产入口使用 Pass 提供的数据；至少一个有 View 的场景 Pass 和一个无 View Pass，两个不同 Pass 布局、多 View/两帧在途和 shader/layout 切换均有真实读回；错误布局被明确拒绝。
- 继续运行现有 Compute/Custom、内容定义、资源退休和宿主回归；Metal/移动端未运行的项目明确保持未验收，平台验收由 `aurora-renderer-platform-parity` 承接记录。

### 6. C02 生产接入方案（2026-10-20 已确认）

进一步检查确认，生产 `RendererRuntime` 在调用 feature 的 `BuildView` **之前**自行组装并提交默认场景 Raster；已有 `pipeline` Pass 尚未接入该路径。仅修改 `PreparedRaster` 参数不能完成纠偏，必须一并切换场景 Pass 的调用者。

推荐沿用现有 `IFrameGraphFeature` 和 `RegisterRenderFeature`，由 `Aurora.Pipeline.Static` 提供场景 Pass 实现与工厂，宿主显式注册。`Aurora.Adaptor.Static` 增加对 `Aurora.Pipeline.Static` 的单向依赖，提供 Launcher/Editor 共用的装配入口；不在 `RendererHost::Init` 内偷偷提交一帧或占用调用者的 sequence/feature ID。注册命令及 module lease 仍由实际宿主填入其首个 packet，沿用现有 feature 退休协议。

依赖变化仅为：

```text
Aurora.Render / EditorRender
  → Aurora.Adaptor.Static
    → Aurora.Pipeline.Static  [新增这条依赖]
      → Aurora.Core.Static
```

Core 不引用 Pipeline 工厂或具体 Pass 共享头；C++ 消费者无 shader codegen 前置。没有新 target、动态模块或目录搬迁。实际宿主路径需要调整 `AuroraModule::Start` 和 Windows `EditorRenderer::Start` 的 feature 装配；现有模块装载入口保持不变。

接口和职责按下表实施，命名可随现有代码风格细化，语义不得回退成固定三块常量：

| 位置 | 修改 |
|---|---|
| Core 公共 `IRenderFeature.h` / `RenderRasterData.h` | 在 borrowed context 上补充 prepared draw 解析入口、显式 uniform binding 类型及临时 attachment 创建；返回 owned 绘制数据与材质数据，不能把 catalog、native context 或借用生命周期传给宿主线程 |
| `PreparedRaster` / `FrameComposer` | 验证当前 prepared draw 的精确资源版本、owner、LOD、范围和 InGraph handle；组装 geometry/shader/状态，保留版本 pins；不生成相机/对象常量，不选择材质参数所在 set |
| Pipeline 场景 Pass | 使用自己的共享头填 Pass 和 Batch 数据；相机数据仅在该 Pass 的声明中需要时写入，材质和对象参数按该 Pass 的 Batch 布局组装；布局不匹配返回错误 |
| `RendererRuntime` | 移除 feature 前的固定默认场景 Raster；保留输出导入、资源准备、feature 调用、导出、提交和退休；资源到达时做通用校验，实际绘制时按 shader reflection 校验绑定 |
| Adaptor / Launcher / Editor | 通过共用装配入口显式注册场景 Pass，与现有 UI feature 使用不同的宿主分配 ID；无 draw 时的清屏也由场景 Pass 声明 |
| 生产测试与宿主 fixture | shader include 生成声明，CPU 使用同源镜像；迁移旧 View/Material/Object fixture，增加不同 Pass 布局、无 View、布局切换及在途帧的读回验证 |

生成声明的更新同时覆盖 CPU 写入和 shader 消费，不能只把原来的 View/Material/Object 改名。现有 Global 声明中的相机字段不自动成为新 Renderer 必需输入；C02 应检查并迁移相关消费者，使具体场景 Pass 的相机数据位于其 Pass 契约内。无 View Pass 可省略这些数据和绑定。

场景 Pass 未注册/被禁用时，不再通过 Core 的隐藏 fallback 出图；输出是否被生产由已注册 Pass 决定，未生产输出的 readback 保持显式失败。不同 feature 的覆盖顺序、共享输出 LOAD/CLEAR 和退休行为需纳入宿主回归。

该方案涉及新增 Adaptor → Pipeline target 依赖和显式场景装配；在展示方案及根 `AGENTS.md` 确认要求后，用户于 2026-10-20 回复“下一步”，据此实施本节方案。没有新增模块或改变装载入口。

落地接口为 `FrameGraphBuildContext::ResolveDraws`、显式类型的 `WriteUniform` 及 `CreateImage`。`ResolveDraws` 只解析当前 `BuildView` 的冻结快照，返回 owned geometry/shader/材质字节和实例变换；协调器继续把完整 catalog 与 feature owners 交给冻结图保活。具体 `SceneRenderFeature` 消费 `ScenePass` 和 `SceneBatch` 共享类型；其 shader 可省略 View 块，其他 feature 可定义完全不同的 Pass 布局。该场景 Pass 当前无需 Global 绑定，不要求填充一个空 Global 块；既有旧管线的 Global 声明不被强加给生产 Renderer。

### 7. C03 资源绑定补齐（2026-10-21）

用户已确认按完整 Pass 绑定、World 基础出图、编辑闭环、Shadow、IBL/HDR 顺序实施。C03 复用 ShaderReflection 作为资源形状事实源，共享声明展开 `<Pass>Bindings` 类型化槽位与 `Get<Pass>Reflection()`，与 native 常量类型和 shader 同源。独立 texture/sampler/storage 声明不得遗漏；未知类型、重复绑定和无法镜像的常量明确失败。完整资源描述不代表 raster 后端已支持 Cube、比较 sampler、storage 或 descriptor 数组。

Pipeline 的 PassResourceBindings 暂存当前构图范围的绑定值；通用装配按最终 shader 实际使用的资源匹配共享 schema，校验字段、类型、维度、count 和显式 dynamic 选择。声明但未被 shader 使用的资源可省略；shader 使用但未声明/未提供的绑定失败。装配先验证后替换 draw，失败不留下部分绑定。多组绑定通过完整 set/binding 匹配，禁止依靠 set 猜测业务数据。共享 schema 与调用方模板槽位不保存原生指针。

装配结果展开到现有 RasterDraw 的 uniforms/images/samplers，沿用图分析的子资源读写与版本保活；没有第二套 descriptor 池、全局可变资源表或跨帧缓存。具体 Pass 决定资源来自本帧常量、已解析的持久资源还是上游 Pass 输出。使用作用域为当前图/lease，生产场景 Pass 复用同一 View 内的公共常量。后续 descriptor 复用必须比较完整资源版本、范围、类型与布局，不能只按布局复用内容。

首个纹理消费者选现有 TextureToScreenPass 声明，使用真正的 2D 纹理和普通 sampler；GPU 测试通过图内渲染目标作为采样输入验收真实依赖。常量数组、嵌套结构及其缓存格式迁移在 Shadow 子阶段补齐，不以有枚举或反射记录作为完成证据。World producer 仍经唯一宿主 packet 通道追加 owned 命令；相关代码留 adaptor，编辑器接线留 sandbox。

## Risks / Trade-offs

- [只改名字仍保留固定 ABI] → 以删除生产硬编码和不同布局/no-View 读回为完成条件。
- [生成器与 Core 环依赖] → 生成类型由具体 Pass 所在模块消费，Core 只处理接口声明，不通过复制字段定义绕过。
- [目标布局不匹配] → 按目标反射检查 native C++ 布局，不用 Windows 成功替代 Metal 验收。
- [声明漂移] → 两侧直接消费同一共享源，反射不一致在绑定前明确失败。
- [改动混入 Compute/Custom] → 本 change 单独记录文件/测试；不撤回已验证的资源 kind/access 分离和同步修复。

## Migration Plan

1. 创建完整 proposal/design/specs/tasks，修正规则文档中对固定 ABI 的定位。
2. C01 的生成器链已被共享头方案替代；历史构建记录保留，当前验证见后续 change。
3. 确认第 6 节的 C02 单向依赖与装配方案，再实施 owned 数据接口，迁移固定 ABI 的生产调用点和测试。
4. 执行相关 CPU/GPU/宿主回归，回填各平台证据，更新当前说明与债务指向。
5. 用户确认后再 archive/commit；代码与该 change 的 spec/archive 同一提交。失败阶段保留未完成任务，不把文档已生成当成纠偏完成。

## Open Questions

- C02 依赖与接口方案见第 6 节，已确认并进入验证；真实平台验收证据以 tasks 为准。
- Pass C++ 类型不再依赖 host 生成器；shader cook 与原生 Metal 验收仍按各平台工具链进行。
