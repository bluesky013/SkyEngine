# 验证与性能记录

日期：2026-10-23。基线提交：`230ee6b6`（已获用户确认的 58 文件 Gizmo/相机控制实现）。本优化独立 change，尚未提交或 archive。

## 实现与 Review

- **CPU status: OK。** 热路径 owner 仍是 `engine/aurora/rhi/vulkan/src/VulkanSwapChain.cpp`。QueryCapabilities 只读取实时窗口尺寸/caps 并解析创建约束；稳定 checked QuerySurface 不分配格式/模式 vector、不排序、不枚举。创建/重建和 native 失效复核仍完整协商。异常驱动持续 SUBOPTIMAL 时保守回到完整复核，收益不适用于该退化状态。
- **Desktop GPU status: OK，平台验收有限。** 没有改 shader/pass、提交时序、semaphore/fence 或资源生命周期。TryAcquireFrame 成功的 SUBOPTIMAL 图像仍有 serial 和同步 owner，获取后发现变化由既有 scheduler abandon；TryPresentFrame 保留已排队呈现的独立 fence。RetireFrame、TryResize、TryClose 未改变。Windows Vulkan 两种 compiler 与 DX12 回归；Metal/Android 未在本次实机验证。
- **Architecture status: OK。** 变更限于现有 VulkanSwapChain 私有实现、状态和既有 FrameOutputs 测试；无新公共接口、target、依赖、跨 DLL singleton、线程或目录迁移。mutable dirty 状态继续归同一 renderer coordinator。旧 direct 协议保留完整查询，未扩展遗留 native 参数调用链。
- Review 检查了 dirty 清除条件：失败/INCOMPLETE 不清除；选项仍不同持续 OutOfDate；匹配已应用值或成功创建才清除。重复查询不推进 surfaceRevision。原生错误继续显式返回，无新增 throw、try/catch、RTTI 或用 assert 代替失败返回。
- Native 测试通过实际 acquire/present 后改返回码注入 SUBOPTIMAL，保留原生信号；OUT_OF_DATE 在实际 acquire 前注入，避免伪造“未获取”却遗留图像责任。模式变化测试按真实创建模式判断，不假定每台设备都支持 IMMEDIATE。

## 自动验证

在现有 `cmake-build-debug` 构建 Debug/Release 的 Vulkan、SandboxModule、SandboxEditor 和 Aurora.Renderer.GpuSubmissionTest。FrameOutputs 覆盖 stable call budget、caps 错误、extent/transform、minimize/resize、格式/色彩空间/模式失效、SUBOPTIMAL acquire/present、INCOMPLETE/error retry、OUT_OF_DATE 重建、direct 兼容、多窗口及独立 GPU/presentation 退休。

最终 Debug/Release 各运行 120 项：**111 passed、9 skipped、0 failures**。9 项跳过均为 Vulkan 专属测试的 DX12 参数。两种普通编辑器以 `--rhi vulkan --frames 100000` 完成有界启动、模块加载、交换链创建和退出，exit 0。日志/XML 位于 `output/surface-query-verification/`；OpenSpec strict validation 与 git diff --check 通过。

## 配对性能测量

Windows / RTX 3080 / Vulkan，交换链 2552×1319，validation 开启，双包在途。使用既有 `GizmoControlsVerification.skyproj`、main.world、选中 Cube 与相同正弦旋转相机。每轮约 25 秒，舍弃前 3 秒，仅统计完整关联 sequence。

同一临时探针二进制分别运行完整查询对照与 capabilities 快路径。对照在 QuerySurface 内强制调用完整 QueryConfiguration；其稳定场景的 native 调用次数与基线一致。由此避免只拿前一天的绝对耗时作比较。每种配置一组配对采样，未把单机结果推广成所有设备的保证。

| 配置/路径 | 完整样本 | 单次 surface 查询均值 | 快照→完成通知 P50 | P95 |
|---|---:|---:|---:|---:|
| Debug 完整查询 | 955 | 3.576 ms | 45.602 ms | 50.662 ms |
| Debug capabilities | 1,396 | 0.0189 ms | 31.012 ms | 34.573 ms |
| Release 完整查询 | 2,719 | 3.097 ms | 15.807 ms | 18.741 ms |
| Release capabilities | 13,187 | 0.00631 ms | 3.153 ms | 4.666 ms |

快路径稳态的格式/模式调用均为 **0**；Debug 记录 2,798 次 capabilities，Release 记录 26,381 次。计数的首尾跨越采样界限，所以不以总数推断每帧次数；精确 prepare/post-acquire 各一次由 native 回归断言验证。每帧移除约 7.1 ms（Debug）/6.2 ms（Release）的查询开销。

这里的延迟定义是**host 创建相机/UI 快照 → host 收到 GPU 完成通知**，不是 GPU timestamp 耗时，也不是鼠标输入到屏幕发光延迟。两包在途时，查询减负也缩短了排队；不能把总延迟差全部算作单帧查询成本。此前约 54.3 ms 的绝对值存在环境波动，本次以同二进制 A/B 为准。

## 剩余成本

| 快路径阶段均值 | Debug | Release |
|---|---:|---:|
| GUI 生成 | 5.762 ms | 0.232 ms |
| UI snapshot | 3.472 ms | 0.231 ms |
| packet validate | 1.020 ms | 0.109 ms |
| UI/feature frame 建图 | 3.349 ms | 0.179 ms |
| 图 Compile | 4.895 ms | 0.215 ms |
| Dispatch | 1.263 ms | 0.078 ms |
| native Present | 0.964 ms | 0.769 ms |

这些是阶段采样，存在线程重叠/嵌套，不能相加作为总帧时间。Debug 的无优化 CPU 工作仍是明显大头，支持将 Develop 作为日常模式；具体迁移见 [develop-assessment.md](develop-assessment.md)。Release 下 GUI/snapshot/feature 合计约 0.64 ms，后续 dirty 缓存收益应按实际复杂场景测量，不能再据旧 surface 热点判断优先级。

## 证据与边界

- 四轮运行均 exit 0、stderr 为空，stdout 未出现 ERROR、VUID 或 Validation Error；数据与分析结果在 `output/surface-query-verification/trace/{debug,release}-{full,caps}.*`。
- 临时探针通过 manifest SHA256 校验后恢复四个文件；再重建普通二进制。最终源码不含 SKY_LATENCY、计时探针或自动旋转逻辑。
- HDR/跨屏用格式/色彩空间失效注入验证软件路径；没有真实切换 HDR 显示器的实测。有效的当前 SDR 选项不会因仅新增 HDR 能力而自动改策略。
- 本次没有实现 Develop、UI dirty 缓存或 Present 线程迁移。
