# Renderer 模块归属纠正验收

2026-10-17。用户已确认本结构纠正；未 commit/archive。B10 功能不在此 diff 中追加。

Architecture status：**OK（本次纠正范围）**。Renderer 实现并入 Aurora.Core.Static，Framework 注册并入 Aurora.Adaptor.Static，运行时复用 Aurora.Render；删除嵌套 renderer 目录和两个实现 target、独立 provider 模块，保留仅接口 target。边界证据为 core/CMakeLists.txt、runtime/CMakeLists.txt、AuroraModule.cpp；CPU prepare 源码无 RHI/Framework/GPU graph/resource include，内部路径只通过 PRIVATE/测试 INCS 暴露。后续生产线程与宿主切换仍由 B12–B14 验收。

迁移 68 个文件。61 个移动后的 C++ 文件除 include/格式/注释外 token 一致；6 个原公开头文本一致，221 个 RHI 文件 SHA-256 一致。实际代码变化集中在 AuroraModule 注册/撤销 provider、初始化失败检查、幂等和析构清理，以及使用实际 Aurora.Render 入口的模块测试。原测试 target 名称保留，私有 include 改用引号；已清除旧 provider DLL 的输出残留。

CPU status：**OK（结构变更）**。Hot-path owner 仍是 prepare workers/coordinator；证据是移动代码 token 对照和 CPU 回归。没有新增逐帧开销，模块服务注册只在初始化/退出发生；不作性能提升声明。Required correction：后续继续守住 CPU prepare 的源代码依赖边界。

Desktop GPU status：**OK（功能未变）**。Cost driver、native 编译、提交顺序和资源退休均保持原状；证据是 RHI 文件哈希及三模式完整回归。Required correction：B10 的 depth resolve/cache 等能力仍按各自退出条件实施；此前 B10h 未归因崩溃记录不因本批通过而删除。

## 验证

- 配置、相关 core/adaptor/runtime、全部 Renderer 测试、SandboxModule/SandboxEditor 构建通过。
- 接口、CPU prepare、资源、动态/tagged static 模块与原 core/adaptor：13/13 组 CTest 通过。模块验证实际 Init/卸载/重复 owner；未 Start 窗口，不计 UI/presentation 验收。
- GPU 592/592，零跳过；RHI 133/133。Vulkan 同步验证与 DX12 InfoQueue 未报告验证错误。
- 三个 change 的 OpenSpec strict、原主 change 稳定任务/requirement 编号和链接、git diff --check 通过。
- CPU/GPU 算法本批不变；没有新增帧内调度、GPU 命令或等待。不宣称生产 profiling、Metal、完整静态打包或生产 IRenderer 验收完成。

## 独立 review 资料

- `output/renderer-layout-moves.json`：旧/新路径清单。
- `output/renderer-layout-before/`：批前原文件，含原工作区未提交功能修改。
- `output/renderer-layout-review.diff`：路径移动及内容变化。
- `output/renderer-layout-nonmove.diff`：排除纯移动，只展示内容变化。
- 日志：`output/renderer-layout-{configure,build,cpu,gpu,rhi}.log`。

aurora-renderer 保持 active，B10 按固定清单继续；aurora-scene-bridge 保持 active，其真实宿主验收仍依赖 B12/B14。本 change 实现验收完成，归档与提交等待用户明确授权。
