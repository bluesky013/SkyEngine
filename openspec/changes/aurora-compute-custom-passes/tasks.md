# Compute/Custom pass 实施待办

Status: phase-complete（实现与 Windows 验收完成，待用户确认阶段归档；Metal 验收未完成）。6.3/6.4/10.11 保持未勾选，由 `aurora-renderer-platform-parity` 4.20 / P02 继续跟踪 macOS 编译与实机验收。

## 1. IR 与数据模型

- [x] 1.1 在 `FrozenFrameGraph.h` 增加 `FrameAccessMode`、`FrameBufferBinding`、`FrameImageBinding`；Compute/Custom 复用这些声明，Custom 增加 `FrameCustomPassEncoder` 和 `FrameCustomPassContext`
- [x] 1.2 扩展 `CopyBufferState` 为 `SHADER_READ` / `UNORDERED_ACCESS`，同步更新导入/导出、transient 与边界校验中依赖 `> RASTER_READ` 的判断
- [x] 1.3 保持声明顺序与 Custom context 资源数组稳定；原生 descriptor 按 set/binding 对应，dynamic offsets 按 binding 升序，并在头文件中写明契约
- [x] 1.4 为新增字段提供默认值，确保现有 copy/raster/fullscreen 图的构建与冻结行为不变

## 2. 分析与校验

- [x] 2.1 移除 `AnalyzeFrameGraph` 对 `FrameComputePass`/`FrameCustomPass` 的无条件 Unsupported，改为按声明生成 reads/writes/defines
- [x] 2.2 校验 compute 声明的 set/binding/range/view 与 shader reflection：资源类型、access 方向、重复绑定、越界 view、缺失 shader
- [x] 2.3 校验 custom 的 encoder、声明和 `sideEffect`，拒绝空声明空副作用与非法 access
- [x] 2.4 把 compute/custom 的读取纳入内容证明与 lifetime，写入定义对应 buffer range 或 image mip/layer 内容；未定义读取在 Compile 前拒绝
- [x] 2.5 更新 `UnsupportedPassKindsFailBeforeAllocationEvenWithoutExports` 及同类契约测试，覆盖“合法声明可编译 / 非法声明失败”

## 3. 图构建与公共辅助

- [x] 3.1 在 `FrameGraphBuilder` 保持 `Compute`/`Custom` 入口，补充参数/返回值说明与常用声明构造辅助（不引入新 builder 体系）
- [x] 3.2 确认 Custom encoder 的生命周期由 shared_ptr 持有，冻结图/编译计划在回调期间保持有效
- [x] 3.3 检查 handle 归属（graph id）与 range/view 校验在 builder 和 Compile 两端的一致性

## 4. Vulkan 后端

- [x] 4.1 在 `VulkanGraphResources` 增加 buffer `SHADER_READ`/`UNORDERED_ACCESS` 与 image `SHADER_READ_ONLY`/`GENERAL` 的转换辅助，复用现有 whole-buffer barrier 与按子资源 image barrier
- [x] 4.2 `VulkanDynamicRenderingCompiler` 生成 compute plan：声明 barrier、bind pipeline/descriptors、dispatch；custom plan 调用 encoder
- [x] 4.3 `VulkanRenderPassCompiler` 在 compute/custom 前后结束并重建 native scope，禁止跨该 pass 合并 subpass
- [x] 4.4 将 compute/custom 作为 coordinator-only encode 节点接入任务图，Custom 不进入并行路径
- [x] 4.5 对超出范围（async compute、跨队列、原生 custom 契约）显式返回 Unsupported

## 5. DX12 后端

- [x] 5.1 在 `D3D12FrameCompiler` 增加声明访问到 D3D12 resource state 的映射与 transition
- [x] 5.2 编码 compute dispatch 与 custom 回调，保持 DIRECT list 分片、状态恢复与稳定提交顺序
- [x] 5.3 将 compute/custom 纳入 coordinator 串行路径，失败时释放本次编译对象且不消费 lease
- [x] 5.4 对未支持的能力或格式显式返回 Unsupported

## 6. Metal 后端

- [x] 6.1 在 `MetalDeviceFrameContext` 用 compute encoder 实现 compute plan，按声明在 encoder 边界/显式 memory barrier 完成同步
- [x] 6.2 实现 custom 回调路径，保证 command buffer 与声明资源在回调期间有效
- [ ] 6.3 Metal 能力不足时显式 Unsupported；macOS 上完成可编译与契约测试（Windows 不构建 Metal target）
- [ ] 6.4 在 `aurora-renderer-platform-parity` P02 环境执行 Metal 真实设备读回，并回填该 change 的验收记录

## 7. 测试与验收

- [x] 7.1 `FrameGraphTest`：合法 compute/custom 不再 Unsupported；非法声明、未定义内容、越界 range/view 被拒绝
- [x] 7.2 `VulkanFrameCompilerTest`：两种模式 compute 写 storage buffer → copy 读回，以及 compute 写 storage image → raster 采样读回均通过
- [x] 7.3 `VulkanFrameCompilerTest`：Custom 回调写入声明 buffer 后读回，验证串行协调与 barrier 位置
- [x] 7.4 `D3D12FrameCompilerTest`：同 7.2/7.3 的读回与同步用例
- [x] 7.5 对照测试：compute/custom 与 raster/copy 交错时，串行与任务完成顺序变化不影响读回结果
- [x] 7.6 关闭编辑器后配置并构建相关目标，运行 `FrameGraphTest`、`VulkanFrameCompilerTest`、`D3D12FrameCompilerTest`、`Aurora.Renderer.GpuSubmissionTest`

## 8. OpenSpec 归属收口

- [x] 8.1 更新 `aurora-renderer-platform-parity` 4.20：Compute/Custom 的 IR 与 Vulkan/DX12 实施指向本 change，Metal 实机验收保留在 4.20
- [x] 8.2 更新 `aurora-renderer-extensions` E2：Compute/Custom 条目指向本 change，不再作为候选扩展重复排期
- [x] 8.3 运行 `openspec validate aurora-compute-custom-passes`，确认 delta spec 与主 spec 一致


## 9. Review 修复与结构精简

- [x] 9.1 修复 DX12 同状态 UAV 访问缺少依赖；连续 buffer/image compute 写入通过 barrier 检查和 GPU 读回
- [x] 9.2 公共纯函数统一有效 buffer range 与 image view；内容、裁剪与越界校验包含 dynamicOffset；compute storage image 限制为单 mip
- [x] 9.3 公共 GPU bindings 为 compute 保活 shader/sampler；CPU 图释放后继续持有，完成后释放
- [x] 9.4 DX12 按 descriptor 类型选择 constant/UAV 状态；dynamic-only set 不分配 descriptor table，检查 root CBV
- [x] 9.5 Vulkan/Metal 共用按 binding 升序的 dynamic offsets；Metal 的 WriteBuffer arrayElement 固定为零，偏移经 BindResourceGroup 传递
- [x] 9.6 buffer 边界校验统一支持 SHADER_READ/UNORDERED_ACCESS；非法 access 显式失败
- [x] 9.7 DX12 抽出 compute 绑定构建，Vulkan 两 compiler 共用 compute/custom 编码及 custom 资源解析；保持现有目录/target
- [x] 9.8 补充纯分析、静态/动态 GPU uniform、UAV 连续访问、sampler 退休与 Custom 失败路径回归；Metal 增加动态偏移读回用例，待 macOS 运行
- [x] 9.9 重新构建并运行完整 Aurora.Renderer.GpuSubmissionTest，更新最终结果
- [x] 9.10 运行 openspec validate aurora-compute-custom-passes --strict


### 2026-10-18 Review 验收记录

- VS 2022 / Debug：`Aurora.Renderer.GpuSubmissionTest` 及依赖构建通过。MSVC 本机 `/Zi` 触发 C1902，本次构建临时设置 `_CL_=/Z7`，未修改工程配置；构建日志：`cmake-build-debug/compute-review-build.log`。
- 完整 `Aurora.Renderer.GpuSubmissionTest`：832 项，829 通过、3 跳过、0 失败（168.949 秒）。跳过项为 Vulkan-only surface 测试的 DX12 参数，不涉及本次 compute/custom 回归。
- 覆盖 Vulkan RenderPass/Dynamic Rendering 与 DX12 的 GPU uniform 读回、连续 UAV buffer/image 访问、CPU 图释放后的 sampler 保活，以及 Custom 失败停止编码/DeviceLost 优先级；分析测试覆盖动态有效范围、producer 裁剪、新边界状态、非法 access 与单 mip storage view。
- 完整测试结果：`cmake-build-debug/compute-review-full.xml` / `.log`；`openspec validate aurora-compute-custom-passes --strict` 与 `git diff --check` 通过。
- Metal 已补 `FrameContextComputeUsesDynamicOffsetsInBindingOrder`；Windows 未编译/执行 Metal，6.3/6.4 继续保留为未验收。


## 10. 修订 Review 完整性修复

- [x] 10.1 FrameBufferBindingKind/FrameImageBindingKind 与 access 分离，迁移 Custom 声明及后端映射
- [x] 10.2 同 pass 的整 buffer/重叠 image view 类型冲突校验，保留合法重复绑定
- [x] 10.3 四个 compiler 的 NoWork 保留空资源 sideEffect 和资源边界工作
- [x] 10.4 Vulkan storage 同步覆盖 Custom graphics/compute 前后依赖
- [x] 10.5 Metal shader/范围/usage/线程组、native buffer alignment reflection 和真实 image backing 校验
- [x] 10.6 Metal custom 失败统一收尾，补 discard/复用；反射查找检查 end
- [x] 10.7 补只读 storage image/buffer GPU 读回、内容/类型冲突、副作用及 callback 保活测试
- [x] 10.8 spec/design 明确非零 dispatch、WRITE 覆盖、Custom 对齐与 encoder 生命周期边界
- [x] 10.9 重建并完整运行 Aurora.Renderer.GpuSubmissionTest / Aurora.RHITest
- [x] 10.10 OpenSpec strict / git diff --check
- [ ] 10.11 macOS 编译与 Metal 实机回归（沿用 P02）

### 2026-10-19 修订 Review 验收记录

- VS 2022 / Debug：`Aurora.Renderer.GpuSubmissionTest`、`Aurora.RHITest` 及依赖构建通过。沿用本机临时 `_CL_=/Z7` 绕过 `/Zi` 的 C1902，未修改工程配置；日志：`cmake-build-debug/compute-review-final-build.log`。
- 完整 `Aurora.Renderer.GpuSubmissionTest`：845 项，842 通过、3 跳过、0 失败（170.373 秒）。跳过项仍为 Vulkan-only surface 测试的 DX12 参数。
- 完整 `Aurora.RHITest`：133 项全部通过，0 失败（2.741 秒）。结果：`cmake-build-debug/compute-review-final-full.xml` / `.log`、`cmake-build-debug/compute-review-final-rhi.xml` / `.log`。
- 新增回归覆盖 Custom 只读 storage image/buffer GPU 读回、导入资源 usage、同 pass 类型冲突及合法重复绑定、未定义内容、非零 dispatch、空资源副作用、callback 退休保活，以及 Vulkan storage 同步阶段与失败停止编码。
- 本轮修改的 C++ 文件通过 clang-format 检查；`openspec validate aurora-compute-custom-passes --strict` 与 `git diff --check` 通过。
- Metal 已补原生对齐、预检错误、真实 image usage、空资源副作用和失败后 discard/复用测试；Windows 未编译/执行 Metal，6.3/6.4/10.11 继续保留为未验收。

### 2026-10-19 契约边界复核

- Custom STORAGE MSAA 未构成可达缺陷：AnalyzeFrameGraph 的全局 MSAA usage 校验已排除 STORAGE。补充 `CustomStorageImagesRequireSingleSampleButAllowMultipleMips`，覆盖导入/transient、READ/WRITE/READ_WRITE、X2/X4/X8、附带 RENDER_TARGET usage 的拒绝，以及 X1 多 mip 的接纳；不重复增加运行时检查。
- Metal memory 条件只防御非法枚举，合法类型沿用 private/shared 映射；buffer reflection 整体缺失时保守返回 UNSUPPORTED，不能与成功反射中的 inactive binding 混同。已补注释和 design/spec，运行逻辑不变。
- 相关 Debug 目标及依赖构建通过；`FrameComputeAnalysis.*` 8/8 通过。日志：`cmake-build-debug/compute-contract-review-build.log`、`cmake-build-debug/compute-contract-review-tests.log` / `.xml`。OpenSpec strict、clang-format 与 diff 空白检查通过；本次未重跑全量 GPU/RHI，Metal 验收状态不变。
- 用户另一次全量运行反馈：GPU 841 通过、3 跳过、1 项 `FrameOutputs.QueryErrorAfterAcquire.../2` 偶发失败，单独重跑通过；RHI 133/133。本次未修改该失败路径，仍需独立跟踪，不能将单独重跑通过记作该次全量无失败。

### 2026-10-20 阶段归档准备

- 对 2026-10-19 的偶发失败进行了独立复现：DX12 目标用例单独重复 200/200 通过；FrameOutputs 随机顺序运行 10 轮（seed 1729–1738），990 通过、30 已知跳过、0 失败；CTest 默认顺序完整 GPU 测试 846 项，843 通过、3 已知跳过、0 失败（210.659 秒）。目标用例累计 211 次全部通过，不能据此判定原问题已修复；按用户要求暂停调查，等待再次复现及原始断言。日志与命令记录见 `cmake-build-debug/frameoutputs-repro-summary.txt` 及同名前缀 `.log`。
- `FrameGraphSPIRV.h` 的生成器为全部 12 个模块输出等价 GLSL 注释（含编译宏），`EncoderTest.cpp` 的两个手写模块补充等价 shader 注释。14 个模块的字节码指纹保持一致，Python 语法和 diff 空白检查通过；注释按用户授权不受两行限制，未为纯注释修改重复运行 GPU 测试。
- 本次阶段归档范围为声明式 Compute/Custom 的 IR、分析、后端编码、Windows 回归及契约文档；不将 Metal 编译或实机测试计为完成。待验收内容已明确转交 `aurora-renderer-platform-parity` 4.20 / P02，包括原生 buffer alignment reflection、动态偏移、线程组限制、storage view、只读 storage、失败 discard/复用及资源退休保活。
