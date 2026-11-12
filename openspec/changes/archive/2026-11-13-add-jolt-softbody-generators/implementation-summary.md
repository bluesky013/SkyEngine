# SkyEngine 软体修改交接

> 2026-11-13：按用户明确指令归档当前交付。任务完成 80/83；5.4（形变法线视觉验收）、7.5（三端完整应用及持续性能验收）、8.5（优化海豹交互 Generate/Save/reopen）仍未完成，保留原始未勾选状态。归档与规范同步不代表这些验收已经通过。

2026-11-13 已整合上游 `d61075ff`，完成实际模块重建，25 个 C++ 测试程序合计 1353 项通过、3 项跳过、5 项既有禁用，零失败；Python 与规范校验通过。上游已包含 Particle/Curve，AssetTool 软体接入已迁移到模块清单。详见 [集成验证](verification.md#rebase-validation-outcome)。

以下为 2026-11-10 的实现与验证快照；Git 状态属于当时记录，归档后的集成验证见 verification.md 最后一节。

盘点于 2026-11-10。当前分支 `dev_refactor_rhi` 已 rebase 到 `origin/dev_refactor_rhi`，HEAD 与本地远端跟踪引用同为 `983c581a`，领先和落后提交数均为零。软体求解、自动绑定、资产烘焙、动画蒙皮和 Mesh 预览已经接通，改动仍在工作区，OpenSpec change 仍处于活动状态。

当前路线是独立 CPU XPBD，由 Jolt 提供刚体碰撞查询。控制点数量可配置，每顶点最多四个骨骼影响。Android 已有双兔五档碰撞、跳跃和性能结果，三端同屏十只持续 60 FPS 仍待验收。

## 引擎改动与代码归属

| 环节 | 当前实现 | Review 入口 |
| --- | --- | --- |
| Physics 接口 | 独立 target `Physics.SoftBody.Interface` 定义控制点、约束、固定步碰撞查询、调参、统计和调试快照。接口不暴露 Jolt 类型。 | [接口](../../../../engine/physics/softbody/include/physics/softbody) |
| CPU 求解与碰撞 | 独立 XPBD 处理边长、四面体保体积、根随动和接触约束。动态点可通过有限柔度跟随角色根，也支持固定点和根运动学点。World 在同一固定步中批量推进软体，软–软点面接触同时修正双方。一批求解失败时保留上一次完整结果。Jolt 提供刚体世界、静态和平移运动学障碍的球扫与穿透查询。 | [CPU 求解器](../../../../plugins/softbody/core/solver/src/CpuSoftBodySolver.cpp) · [World 批处理](../../../../plugins/softbody/adaptor/src/SoftBodyWorldCoordinator.cpp) · [Jolt 查询](../../../../plugins/jolt/adaptor/src/backend/JoltSoftBodyCollision.cpp) |
| Rig 与资产 | Authoring 保留指定控制点的位置与模式，按可配置预算生成四面体笼、形变骨和每顶点最多 4 个非负权重。也可输入已有笼复刻兔子。Builder 读取同一份配方，通过 Aurora.Cook 构造 Mesh、Skin、Skeleton，再连同 SoftBody 产品整组发布。产物身份稳定，构建失败保留原结果。 | [绑定算法](../../../../plugins/softbody/core/authoring/src/SoftBodyRigGenerator.cpp) · [Builder](../../../../plugins/softbody/builder/src/SoftBodyBuilder.cpp) · [Mesh 编译服务](../../../../engine/aurora/cook/mesh/src/MeshAuthoringCompiler.cpp) |
| Mesh 编辑器 | EditorCore 提供跨 DLL 的通用 generator 注册；SoftBody.Editor 提供实际生成器。Mesh 文档支持选点、移动、编辑模式与参数、一键生成、笼和权重预览，以及整组撤销、重做、保存与重新烘焙。版本检查会丢弃过期的异步结果。默认由 Aurora 显示完整材质模型，可叠加控制点和骨骼，按需切换权重视图。 | [注册接口](../../../../engine/sandbox/core/include/editor/core/asset/AssetGeneratorRegistry.h) · [Mesh 文档](../../../../engine/aurora/editor/src/MeshGeneratorDocument.cpp) · [软体生成器](../../../../plugins/softbody/editor/src/SoftBodyGenerator.cpp) |
| 共享预览与交互 | Sandbox 预览会话提供地面、灯光和阴影；Mesh 与 Character 复用相机输入。默认视角参考 UE Static Mesh 预览，支持 Alt+左键环绕、Alt+中键平移、Alt+右键拉近拉远、滚轮缩放和 F 框选。Particle 编辑器尚未实现，可后续接入同一机制。 | [预览场景](../../../../engine/sandbox/render/src/EditorPreviewSessions.cpp) · [相机输入](../../../../engine/sandbox/widgets/src/AssetPreviewCameraInput.cpp) |
| 动画与渲染 | 软体将插值后的控制点位移转换为骨骼姿态，Animation World 与基础动画合成后交给 Aurora。主视图和阴影沿用四权重蒙皮，包围盒随姿态更新。通用笼有四面体形变梯度法线路径；原兔复刻使用标准蒙皮法线。 | [软体姿态](../../../../plugins/softbody/adaptor/src/SoftBodyPose.cpp) · [动画宿主](../../../../engine/animation/adaptor/src/AnimationWorldHost.cpp) · [渲染桥](../../../../engine/aurora/adaptor/src/renderer/RenderSceneBridge.cpp) |

配套改动包括 Framework 分离持久化 schema 与运行时工厂，使 AssetTool 可以离线 cook World；将动画组件元数据实现显式归入 `Animation.GraphAssets.Static`，解决 `RegisterAnimationComponentData` 链接错误。Aurora 修正 UV 到 shader TEXCOORD 的映射及非方形纹理末级 mip 尺寸，并将描述符容量配置传到帧上下文。离线 Mesh 编译服务用互斥锁保护共享所有权，适配 Android NDK 的标准库。

Rebase 后动画接入代码已合入上游的 `engine/animation/{core,assets,adaptor,editor}` 布局。算法、资产、World 与编辑器职责分别留在对应 target，生产源码只有一个编译 owner。软体接口不暴露 Jolt 类型，Builder 通过接口消费 Aurora.Cook，Aurora 与 Animation 不依赖软体插件实现。

## 编辑器卡顿修复与测量

卡顿时的窗口线程栈进入了材质的 Slang 编译和 IR 链接。主编辑器场景和资产预览现已启用异步材质编译，共享进程内程序缓存。内置 UI 程序仍在编辑器初始化时编译，尚无跨进程磁盘缓存。

Mesh 文档只在注册项或输入版本变化时刷新 generator actions，空闲时每 250 ms 检查文件时间戳，生成和保存仍立即校验。相机运动时权重图降低分辨率，停止后恢复，完整三角形覆盖不变。预览跳过未变化的相机更新，UI 顶点和索引改为批量复制以减少逐项开销。

同一 15 点兔子浮动预览窗口、材质就绪后的 12 秒 DxgKrnl Present 测量如下。数据表示窗口提交间隔，不能直接当成屏幕帧率或输入延迟。

| 配置 | 平均提交间隔 | P95 |
| --- | ---: | ---: |
| 优化前 Debug | 55.90 ms | 65.88 ms |
| 优化后 Debug | 47.50 ms | 50.06 ms |
| 优化后 Develop | 3.17 ms | 4.53 ms |

Debug 启用了调试运行库和 GPU validation，Develop 使用优化构建，因此第三行的提升包含配置差异。Develop 另一次含滚轮缩放的 18 秒记录平均 2.71 ms、P95 3.53 ms。最终冷启动在打开 Mesh 后 10.75 秒的首次检查中已显示材质，这只是视觉采样上限。详见[验证记录](verification.md)。

## 兔子项目与实测

- 原兔参考 rig 有 15 个动态控制点、24 个四面体、50 条边和 16 根骨骼（含根）。每顶点最多 4 个影响。背碰演示有单独调整过后背接触范围的 rig，提供 10、15、20、25、30 点五档。
- 独立 Android 项目负责双兔背碰驱动、跳跃、灯光和 IBL、HUD、调参、切 rig、调试线绘制及中央隔板布置。引擎提供这些功能消费的接口，包括跟随柔度、边柔度、体积柔度、阻尼，以及点、距离约束、表面和碰撞半径快照。Volume 调节受压时的保体积程度，非零气体压力当前不支持。[演示工程](C:/Users/bluesky/.codex/visualizations/2026/11/01/01a19720-4b10-7872-85c9-e32d17581722/rabbit-collision-demo/RabbitSoftBody.skyproj)
- 同一计时版 Android Release 已在小米 17 Pro Max 和 Redmi Note 12 Turbo 验证。最终普通光照场景固定 1 对兔子、120 Hz、每步 4 次 XPBD 迭代，每台各记录 72,000 个正式步、156 次完整撞击，覆盖五档背碰与中央隔板对照。小米另有同场 30 次连续接触与分离、期间两次跳跃的记录。求解数据是双兔批次的耗时，整帧渲染另计。[两机性能报告](../../../../build/rabbit-benchmark/bench-results/marble-c07e2926-20261103/ordinary-report/device-comparison.md) · [五档背碰与跳跃视频](C:/Users/bluesky/.codex/visualizations/2026/11/01/01a19720-4b10-7872-85c9-e32d17581722/rabbit-collision-demo/reference/validation/rig-collision-video-20261103/rabbit-impact-and-jumps.mp4)

最终 15 点 Android 背碰测得最大平均躯干缩短 3.319 cm，UE 参考为 4.864 cm；峰值相对分离速度分别为 0.983 和 0.946 m/s。回弹速度已接近参考，形变幅度仍有差异，不能视为完全一致。跳跃过度压扁的主要原因已通过同轨迹参数消融定位到边长柔度；保体积并不保证保持高度。

## 验证状态

- Rebase 前最终预览回归通过 EditorShellTest 158 项、EditorRenderTest 48 项、EditorWidgetsTest 44 项、Animation.EditorTest 8 项，以及 ViewportControls/AssetGenerator 选定的 22 项。Aurora 的三个异步材质用例和 UI 渲染契约检查通过，Debug 与 Develop 编辑器已实际运行。
- 2026-11-10 提交准备已补做 rebase 后的实际模块、SandboxEditor/SandboxModule 和 AssetTool Debug 构建。20 次测试程序执行合计 1,115 项通过、3 项跳过、5 项既有禁用用例，没有失败。真实兔子和海豹资产夹具均已启用，软体相关用例没有跳过；未重打 Android 包。跳过项及具体范围见[验证记录](verification.md)。
- 本轮另修复异步 Shader 快照漏掉宏 include/import 虚拟依赖的问题，并修正 rebase 后失效的 Python 构建测试提取标记。CMake 源码归属检查 26 项、Python 构建配置检查 11 项和 OpenSpec strict 校验通过。
- 求解器、生成器、资产、cook、Jolt 碰撞和动画集成各阶段的通过、失败和修复记录保留在[验证记录](verification.md)，包括早期海豹低点数接触失败与后续兔子重复碰撞回归。

## 边界与后续验收

- 当前 Jolt 适配层复用原 SDK，试验性的原生软体路径及额外补丁已撤回。PhysX 后端尚未实现。动态刚体双向推挤、旋转运动学障碍、软–软连续碰撞检测、边–边碰撞和自碰均未覆盖。刚体世界回滚也不会恢复软体状态。
- 自动笼支持经过约束验证的中心径向壳，并非任意复杂或多连通网格的通用四面体化。优化笼后的完整人工 Generate/Save/reopen 复验和大形变法线视觉验收仍待完成。
- 同屏十只持续 60 FPS 仍待验收。已有 Android 真机数据；OHOS 只验证了求解器与描述校验源文件的 ARM64 对象编译，iOS 尚无本功能的编译或实机结果。[任务表](tasks.md) · [验证记录](verification.md)

## Git 与交付位置（2026-11-10 历史快照）

软体核心新文件（`engine/physics/softbody`、`plugins/softbody`）和本 OpenSpec change 均未跟踪，既有 Physics、Jolt、Animation、Aurora、Framework 等接入修改主要未暂存。当前暂存区只有 `docs/README.md` 与 `engine/framework/test/assets/assets.db`，两者各自还有未暂存变化。工作区另有 PCG、飞行、天气文档及其他 OpenSpec 整理，需要独立辨别归属。

当前共 240 个变更路径，含 98 个已跟踪修改和 142 个未跟踪文件；此统计包含上述其他工作，不等于软体功能文件数。Rebase 冲突已解决，原暂存内容与本地改动保留，恢复 stash `b5530c387f4654f0ea259c3b91450ea51916b6e6` 尚在。本轮未 commit 或 archive。

提交候选选取其中 182 个路径，包含实现、测试、相关设计文档及本 change 的 spec。共享文档索引仅选取软体导航和活跃 change 数量，其他 58 个路径保留原状。候选补丁、完整文件清单和提交说明保存在 `cmake-build-debug/commit-prep-20261110`，未替换用户原有暂存区，等待提交确认。

引擎仓库中的 `build/rabbit-benchmark` 是 Git 忽略的本地构建和测试目录。独立兔子工程另存有源码、资产、签名 APK 和验证材料，单独检出引擎分支不能还原当前手机演示。[兔子工程使用说明](C:/Users/bluesky/.codex/visualizations/2026/11/01/01a19720-4b10-7872-85c9-e32d17581722/rabbit-collision-demo/README.txt)

旧 `plugins/animation/plugin.json` 暂时作为既有演示项目的导出兼容元数据保留；动画实现和模块装载已采用上游 engine 子系统布局。后续交付需同步迁移演示项目声明，并为源码、资产和性能原始数据确定正式存放位置。同一 change 的实现与 spec 应一起 review 和提交，其他功能设计与历史归档另行核对归属。
