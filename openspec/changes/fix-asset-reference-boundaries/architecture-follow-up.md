# 独立架构整改方案（已确认，实施已移交）

用户已于 2026-11-15 确认本方案。实现与验收状态由独立 change `../refactor-aurora-asset-editor-boundaries/` 维护；本文件保留已批准的方案依据，不重复记录完成状态。资产挂载、manifest、材质依赖和导航加载行为仍由当前资产修复 change 负责。

仓库 `AGENTS.md` 要求“新增或调整模块归属、target 和装载入口须先说明方案并取得用户确认”。本次已批准的范围是一个最小资产实现 target、现有两个模块的 metadata 分工，以及既有编辑器扩展接口的职责扩展。独立架构 change 的代码与其 spec 一起 review；不搬迁现有目录，不创建 Aurora plugin，不调整 FrameGraph compiler/executor 归属。

## 1. Aurora Cook 与 Adaptor 解耦

### 已核实的问题

- `engine/aurora/cook/CMakeLists.txt` 的 `AuroraCook.Static` 直接链接 `Aurora.Adaptor.Static`。
- `engine/aurora/adaptor/CMakeLists.txt` 对整个 `src/*` 递归收集，将资产 codec、材质定义解析、组件反射、RendererHost、SceneAssetSource 和模块实现编入同一个 owner。
- `engine/aurora/adaptor/src/MaterialAsset.cpp` 同时实现 `AppendMaterialShaderOptions`、`MaterialDefaultValue`、`ResolveMaterialDefinition` 与需要 Device/AssetManager 的 `CreateMaterialFromAsset`、`CreateMaterialInstanceFromAsset`。
- `ImageAsset.h` 混合持久化结构与 `CreateTextureFromAsset` 声明；`src/ImageAsset.cpp` 是 GPU 创建/上传实现，不能因名字带 Asset 而整体划入离线库。
- `src/SkinAsset.cpp` 混合 SkinAssetData codec 与 `FetchOrCreateSkinByAsset`、`MakeSkinTargetLayout`。`MeshAsset.cpp` 和 `src/assets/LodGroupAsset.cpp` 则主要是数据校验/读写。
- `runtime/src/AuroraMetadata.cpp` 当前调用全量 `AuroraReflection`，同时注册资产、组件类型与组件工厂。只替换 Cook 的直接链接，工具仍可能经 Aurora.Metadata 装入整个 Adaptor，不能算完整闭环。

### 最小 target 方案

只增加 **`Aurora.Assets.Static`**，在现有 `engine/aurora/adaptor/CMakeLists.txt` 中定义。文件仍放现有 `adaptor/include/aurora/adaptor/assets`、`adaptor/src` 和 `adaptor/src/assets`；现有 include 路径不整体改名。

| Target | 职责与依赖变化 |
| --- | --- |
| 新 `Aurora.Assets.Static` | CPU 资产 schema、codec、材质定义解析、资产 handler 和持久化数据 metadata；链接 Framework、Core、Animation 的基础算法 target，以及现有 Aurora.Core.Static 所提供的资源数据/ShaderResolver 能力；不得链接 Adaptor、Pipeline、Editor、Cook 或具体 RHI 后端 |
| `AuroraCook.Static` | 用 Aurora.Assets.Static 替换 Aurora.Adaptor.Static；保留当前 importer/第三方依赖和 Physics.SoftBody.Interface |
| `Aurora.Adaptor.Static` | 链接 Aurora.Assets.Static；只保留 World/组件/运行资源转换、SceneAssetSource、RendererHost 和现有集成实现 |
| `Aurora.Metadata` | 只链接 Aurora.Assets.Static 和 Framework，保留模块名称和入口文件；不经链接回到 Adaptor |
| `Aurora.Render` | 继续链接 Adaptor；新增运行组件 metadata 的唯一注册职责，Init/Shutdown 的 renderer 服务职责不变 |
| `Aurora.Render.Editor` | 继续经 Adaptor 使用运行组件，配置明确依赖 Aurora.Render 与 Aurora.Metadata 的定义/会话契约 |

这一步不宣称 Aurora.Core.Static 已拆为完全独立的 CPU shader/data 库。该 target 当前同时承载 renderer/core 实现，但属于现有下层 foundation；本方案删除的是 builder → adaptor/runtime/editor 的反向依赖。若未来要求链接图完全不含 Core 中的 renderer 对象，须另外量化链接产物后提出领域 target 拆分，不能顺手建立另一套 renderer/core/adaptor 体系。

### 文件切割清单

| 现有文件 | 实施方式与唯一 owner |
| --- | --- |
| `adaptor/src/MeshAsset.cpp` | 原路径保留，整文件由 Aurora.Assets.Static 编译 |
| `adaptor/src/assets/LodGroupAsset.cpp` | 原路径保留，由 Aurora.Assets.Static 编译 |
| `adaptor/src/MaterialReflection.cpp` | JSON codec 和资产反射由 Aurora.Assets.Static 编译 |
| `adaptor/src/MaterialAsset.cpp` | 保留 GPU/runtime material 创建部分归 Adaptor；将纯材质定义处理拆到同目录新增 `MaterialDefinition.cpp`，归 Assets |
| `adaptor/src/ImageAsset.cpp` | 保留 GPU texture 创建/上传，继续归 Adaptor，不复制给 Assets |
| `adaptor/src/SkinAsset.cpp` | 原路径保留 CPU codec，归 Assets；运行资源构造/骨架绑定转换拆到同目录新增 `SkinResource.cpp`，归 Adaptor |
| `adaptor/include/aurora/adaptor/assets/MaterialAsset.h`、`ImageAsset.h`、`SkinAsset.h` | 分离 CPU 数据/codec 契约与运行转换声明；GPU 转换声明放同领域的明确 runtime conversion 头，修改实际使用方；不得用新的 umbrella 头把 GPU 转换重新带回 Cook |
| `adaptor/src/AuroraReflection.cpp` | 保留运行组件类型/编辑属性/工厂反射，归 Adaptor；持久化 Data、资产 schema/handler 注册拆入新增 `src/assets/AuroraAssetReflection.cpp` |
| `adaptor/include/aurora/adaptor/components/*Component.h` | 持久化 Data 与 SkeletalPoseMode 拆入同目录 CPU-only `AuroraComponentData.h`；原组件头 include 它，类型名、反射名、序列化 UUID 和布局含义保持不变 |
| `adaptor/include/aurora/adaptor/AuroraReflection.h` | 明确运行注册入口；新增 `assets/AuroraAssetReflection.h` 公开资产/持久化注册入口，不保留生产“两个 owner 都可调用”的全量入口 |

Assets target 的源清单明确列举。Adaptor 的现有 GLOB 必须排除这些确切源文件；不能让两者编译同一个 `.cpp`。测试通过链接复用，不把生产源加入测试 SOURCES。不通过公开多个实现目录掩盖缺少的 include 契约。

`Asset<T>` 当前含 `CounterPtr<T>` 运行缓存，因此部分资源类型的完整定义仍可能需要现有 Core 头。此阶段不引入第二套 asset tag 或替换 Asset<T>，也不把 GPU resource 构造实现放进 Assets；以实际符号/链接检查证明不会反向依赖 Adaptor。

### Metadata 唯一 owner 和装载时序

复用现成的 `ComponentFactory::RegisterPersistentDataType`，不是新增注册表。已有参考实现：

- `engine/animation/assets/src/AnimationComponentData.cpp`
- `plugins/softbody/core/assets/src/SoftBodyAsset.cpp`

`framework/src/world/WorldAsset.cpp::CookWorldJson` 需要持久化 schema，但不必创建运行组件。因此：

1. **Aurora.Metadata 的 definition/RegisterMeta** 注册 Mesh/Image/Material/MaterialInstance/Skin/Skeleton/LodGroup schema 与 asset handlers、AuroraComponentData、SkeletalPoseMode，以及 component ID → persistent data type 映射。保持原注册名称推导的 component ID，禁止生成新的身份。
2. **Aurora.Render 的 definition/RegisterMeta** 只注册具体组件类型及其属性 getter/setter、运行 ComponentFactory creator；不重复注册上述 Data/enum/asset handlers。它依赖已经创建的 Aurora.Metadata definition。
3. **Aurora.Cook 的 definition/RegisterMeta** 只注册 cook 设置和 builders；Init/Shutdown 对称维护 MeshAuthoringCompiler 服务。
4. **Aurora.Render.Editor 的 definition/RegisterMeta** 只拥有编辑器设置/文档 metadata；会话 Init 将扩展交给 EditorExtensionRegistry，宿主绑定时 Register，退出时 Unregister。
5. Init 失败重试、窗口重建、runtime 重启不得重跑类型反射。不得增加 static bool/Once/查表跳过以掩盖 owner 冲突。

需调整 `runtime/src/AuroraMetadata.cpp`、`runtime/src/AuroraRegistry.cpp`、`runtime/CMakeLists.txt`，并核对 `configs/modules_asset_tool.json`、`modules_editor.json`、`modules_game.json`。AssetTool 复用 Content.Builder.Module 及其 metadata；Aurora 侧仅需 Metadata 和所需 Cook 模块即可处理含 Aurora 组件的 World，不能靠开启 renderer 或加载 UI 来补 schema。编辑器现有 Aurora.Render.Editor 依赖应明确增加 Aurora.Render，避免只有宿主碰巧先加载 renderer 才能创建组件。

所有 `AuroraReflection` 测试调用点同步迁移：`adaptor/test/main.cpp`、`cook/test/main.cpp`、`core/test/renderer/GpuTestMain.cpp`、`core/test/renderer/RendererModuleTest.cpp`、`sandbox/test/WorldEditorStateTest.cpp`、`plugins/softbody/builder/test/Main.cpp`。每个 executable 的 main 或 gtest Environment 唯一注册所需 metadata；cook-only main 不注册运行组件。

## 2. WorldEditorState 去除 Aurora 编辑策略

### 复用现有机制

- `EditorActorCreation` 已承接 Cube/Plane；Point Light、Directional Light、Camera、Environment 创建直接加入现有 `AuroraEditorExtension::Register/Unregister`，不建新 registry。WorldEditorState 只保留 Empty Object 和通用 world/document 操作。
- `IEditorPropertySource::MakeEditCommand` 和 `ReflectedForm::SetEditCommandFactory` 已提供属性编辑入口。
- `EditorExtensionRegistry` 负责跨 DLL 暂存，`EditorExtensionHost` 负责实际实例所有权和 Register/Unregister。现有接口尚无属性策略与组件添加策略，应扩展这两个已有类型，不创建泛化 EditorManager 或全局 extension service。
- Animation 的 `IAnimationInstanceControl::GetComponentType()` 已明确提供反射 `skeleton` UUID 的 authoring 契约。Aurora.Editor 可以通过该接口识别 animation component；不 include Animation.World 的具体组件或 plugin 类型。

### 具体交互契约

在既有 EditorCore 的 `EditorExtension.h` 中增加两个默认返回“不处理”的领域 hook，参数/结果可放同目录小型 `EditorEditContext.h`。接口只涉及 Framework Actor/ComponentBase、PropertyDescriptor、Any、UndoCommand，禁止 Aurora/UI/具体 animation 类型。

- **属性编辑 hook**：接收当前 Actor、实际 property binding 和新值，返回 NotHandled、Ready(owned UndoCommand) 或显式失败。Aurora.Editor 只在实际 binding 属于 SkeletalMeshComponent 的 mesh 字段时处理：从 generated defaults 推导 skin/skeleton，构造完整 before/after 命令。不能仅凭字段名称 `mesh` 劫持其它组件。
- **添加组件 hook**：接收 Actor 与尚未 attach 的 ComponentBase；允许准备这个 detached component，并返回需要与 attach 一起执行/撤销的 owned command。Aurora.Editor 处理已有 SkeletalMesh + 新 Animation component 的 skeleton 初始化和 pose mode 联动。
- `EditorExtensionHost` 在已注册扩展中派发，单一 owner 处理同一编辑；多个扩展同时声称处理视为明确冲突，不用顺序覆盖。失败不能退回普通字段写入绕过领域校验。
- WorldEditorState 接收宿主拥有的 EditorExtensionHost 借用引用；宿主仍负责 CollectDependencies、attach/detach、dirty、selection refresh 和 CommandService。领域命令只表达值变化，不单独建立 undo history、不保存 WorldDocument。
- 完整操作作为同一事务：依赖系统启用 → attach → 领域联动；任一步失败逆序补偿，撤销恢复 pose/组件/依赖状态。准备失败仅丢弃 detached component，当前 world 不变。
- Bind/关闭文档先清空历史，再释放 actor/组件/扩展绑定。命令优先存 actor identity 与受宿主约束的解析器，不能捕获已销毁的窗口；沿用已有服务/数据生命周期，不新增 DLL code lease。

### 文件和 target 范围

| 文件 | 改动 |
| --- | --- |
| `engine/sandbox/core/include/editor/core/extension/EditorExtension.h` | 两个默认 hook 和无领域类型的契约 |
| `engine/sandbox/core/include/editor/core/extension/EditorExtensionHost.h`、`src/extension/EditorExtensionHost.cpp` | 有界派发、handled/conflict/error 结果；继续由原 host 持有实例 |
| `engine/sandbox/module/src/WorldEditorState.h/.cpp` | 注入 host；删除具体 Aurora 创建、SkeletalBindingCommand、SetComponentSkeleton 与 Animation 类型识别，保留通用事务 |
| `engine/sandbox/module/src/SandboxModule.cpp` | 构建/解绑时传入现有 extensionHost；菜单仍消费 EditorActorCreation/EditorActionRegistry |
| `engine/aurora/editor/include/aurora/editor/AuroraEditorExtension.h`、`src/AuroraEditorExtension.cpp` | 接管四类创建与两个领域 hook；复杂命令可拆同目录 `AuroraWorldEdit.cpp`，仍由 Aurora.Render.Editor 编译 |
| `engine/sandbox/test/WorldEditorStateTest.cpp`、`engine/sandbox/test/CMakeLists.txt` | 现有 SandboxProjectTest 做集成验证，EditorCoreTest 使用中性 fake extension 测派发/事务；不重复编译生产源 |

不新建 editor target，不给 EditorCore 增加 Aurora/Animation 实现依赖。不要因删除 WorldEditorState 的 include 就立即删 SandboxModule 的整个 Aurora 依赖：renderer host 等其它合法用途仍存在，应按实际引用裁剪。

## 3. Source、product 与 mesh 身份边界

本轮已解决的范围：同名 Engine/Project 源各有 `(mount, relativePath)` 身份；默认资源由 owner 精确选择 Engine 源再传 UUID；manifest 以锁内最新内容修改；运行材质解析不再追查 `.mat` 源路径；导航无需 Plane。

仍坚持现有链路：`.glb` 是导入源；AuroraMeshBuilder 产出 `AuroraMesh`/MeshAssetData + bulk + 材质依赖；运行时按 UUID 和 product catalog 加载 Sky 产品。组件持有 mesh/material UUID，Inspector 展示来源路径与 hint。主产品 UUID 可以继续等于源 UUID；generated children 使用稳定 key/独立 UUID。源和产品属于不同记录/职责，并不要求两者数值永不相同。

后续 API 应明确 source 查询、product 解析与显示名查询，检查多产品场景返回具体 product 身份。禁止用把 `.glb` 显示或改名为 `.mesh` 的方式假装建立了原生格式；不创建重复的 SkyMeshLoader，不在 runtime 重新运行 importer。运行包只带 products 时必须正常加载，缺产品或类型不匹配返回显式错误。旧缺材质槽产品使用明确 override 或重新 cook，不能恢复隐式源路径 fallback。

## 4. 分批实施与验收矩阵

建议先独立实施资产/metadata 边界，再实施编辑扩展策略。两批分别 review；目录改名、全局 renderer target 细拆、其它 feature 的历史异常迁移不混入。

| 验收范围 | 目标/方法 | 必须证明 |
| --- | --- | --- |
| 源码唯一 owner | `python3 python/tests/test_cmake_source_ownership.py` + 真实 CMake configure | 每个 production cpp 只由一个实现 target 编译，测试无借用 cpp |
| 真实库/模块 | Aurora.Assets.Static、AuroraCook.Static、Aurora.Cook、Aurora.Adaptor.Static、Aurora.Metadata、Aurora.Render、Aurora.Render.Editor、AssetTool | 静态库和真实动态模块都能链接，不能仅报告测试链接成功 |
| CPU cook / codec | Aurora.CookTest、SoftBody.BuilderTest、FrameworkTest | 原材质槽/依赖契约不变；新版/旧版合法产品读写；破损输入不覆盖输出；enum END/越界拒绝 |
| metadata-only World cook | AssetTool + Content.Builder.Module + Aurora.Metadata；补 Framework/集成用例 | 无 Aurora.Render 会话、无窗口/Device 时仍识别 Aurora persistent schema，输出 World 资产引用正确 |
| metadata ownership | Aurora.Renderer.ModuleTest、Aurora.Renderer.StaticModuleTest | definition 只建一次；Init 失败/重试/重启无重复反射；persistent schema 与runtime component声明一致 |
| Adaptor/runtime | Aurora.AdaptorTest、相关 renderer host 测试 | Skin/Material/Image GPU转换仍在适当 owner；独立材质依赖完整；无source包按UUID加载 |
| 通用编辑扩展 | EditorCoreTest、SandboxProjectTest | 无扩展时普通属性可编辑；handled/error/conflict不混淆；undo/redo/失败补偿完整；卸载绑定不会调用失效owner |
| Aurora编辑集成 | SandboxProjectTest、EditorShellTest、SandboxModule、SandboxEditor | Cube/Plane/灯光/相机/环境创建；换skeletal mesh补skin/skeleton；加Animation复制skeleton并切pose；撤销恢复所有字段 |
| 编辑器实机 | macOS 当前工程；Windows VS2022 后续平台验收 | 项目设置/新建对象、Inspector路径和hint保留；换资产/撤销/重开文档不闪退；只保留关键截图 |
| 平台构建边界 | macOS Xcode、Windows VS2022 各自真实构建；Linux可用CI | 静态/动态打包、符号导出、反射owner一致；不能把Mac成功写成三平台已测 |

已确认的具体范围：新增 Aurora.Assets.Static；修改 Aurora.Metadata/Aurora.Render 的注册 owner 与配置依赖；在现有目录拆分上述混合源文件；扩展 EditorExtension/EditorExtensionHost 并将 WorldEditorState 的 Aurora 策略归还 Aurora.Render.Editor。该确认不包含 commit 或 archive 授权。
