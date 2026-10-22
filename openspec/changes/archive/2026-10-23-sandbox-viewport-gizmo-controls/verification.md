# 验证记录

日期：2026-10-22。平台：Windows / VS 2022 Debug，NVIDIA GeForce RTX 3080。

## 实现与使用

- 从 Outliner 选择一个 Actor 后显示 Gizmo；支持轴/平面平移、轴旋转、局部轴/均匀缩放。一次拖拽对应一次撤销；Esc、失焦、文档切换、保存、进入 Play 等会取消未提交预览。
- 视口获得焦点后，Q/W/E/R 切换选择/平移/旋转/缩放，X 切换 World/Local，G 切换吸附，F 聚焦。缩放始终使用 Local。
- 右键拖拽转向，按住右键配合 WASD/QE 飞行，Shift 加速；中键平移，滚轮推进，Alt+左键环绕。编辑相机不修改场景 Camera 或 World dirty/history。
- 工具栏的减速/加速按钮调整相机速度；中间设置按钮的 tooltip 显示当前速度，并打开通用反射表单。速度、Shift 倍率、转向灵敏度和三类吸附步长使用独立设置历史，当前只在会话内保存。
- 控制与绘制留在现有 EditorCore / EditorShell / SandboxModule / EditorRender targets 中；未新增模块、第三方 Gizmo 库或 Aurora pass。

## 自定义绘制接入

实现 `editor/core/viewport/TransformGizmo.h` 中的 `IGizmoVisualProvider::Build`，由宿主调用 `SandboxModule::SetGizmoVisualProvider(sharedProvider)`；控制器也提供 `SetVisualProvider` 供嵌入或测试使用。传入空指针恢复默认外观。

`GizmoDrawContext` 提供相机、视口大小、世界 Transform、工具、空间与主题样式；输出 `GizmoGeometry` 中的点使用视口局部逻辑坐标。每个 primitive 绑定稳定 `GizmoHandle`，可使用折线或凸填充多边形。凹形需拆成共享 handle 的凸块。绘制和命中读取同一份几何，替换 provider 先取消手势，拒绝非法/非有限输出并报告原因。外观层不依赖 GPU/UI 类型，也不负责修改 Actor。

## 构建与自动化

关闭验收编辑器后使用现有 build 目录重新配置，构建 Framework、EditorCoreTest、EditorShellTest、EditorWidgetsTest、EditorRenderTest、SandboxProjectTest、FrameworkTest、SandboxModule、SandboxEditor。最终核心错误报告修改后又重建 EditorCoreTest、EditorShellTest、SandboxModule、SandboxEditor，并复跑相关测试。

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Debug --parallel 4 --target Framework EditorCoreTest EditorShellTest EditorWidgetsTest EditorRenderTest SandboxProjectTest FrameworkTest SandboxModule SandboxEditor -- /nodeReuse:false
```

| 测试程序 | 通过 |
|---|---:|
| EditorCoreTest | 175 / 175 |
| EditorShellTest | 133 / 133 |
| EditorWidgetsTest | 34 / 34 |
| EditorRenderTest | 6 / 6 |
| SandboxProjectTest | 11 / 11 |
| FrameworkTest | 180 / 180 |
| 合计 | **539 / 539** |

新增 9 项 core 和 4 项 shell 测试，覆盖相机投影/射线、按时间积分与斜向速度、look/pan/orbit/focus、速度和 Shift、Play 切换、父级旋转/非均匀缩放、吸附、缩放符号、退化拒绝、预览取消/单步撤销/重做、失效对象、文档切换、保存重开、自定义几何绘制/命中、捕获和视口外释放、尺寸变化取消、1/1.5/2 倍主题缩放以及主/浮动 surface 的焦点与快捷键路由。

最终复跑曾在受限沙箱中出现临时目录 `canonical: Access is denied`，连带既有资产和偏好持久化用例失败；正常用户环境下两套完整测试均通过。保留失败与成功日志，未改动无关异常处理来规避环境限制。FrameworkTest 对仓库 fixture 的写入已恢复为原内容。

## 原生验收

使用独立 fixture `output/gizmo-controls-verification/GizmoControlsVerification.skyproj` 和复制的 `assets/main.world`，未修改用户项目场景。

| 环境 / 操作 | 观察结果 |
|---|---|
| DX12，选中 Cube 后拖拽 X 轴 | Inspector 与视口同步；一次 Ctrl+Z 恢复原平移 |
| DX12，E / R 切换并拖拽 | Z 轴旋转约 -90 度，均匀缩放到 (2,2,2)；图形和命中对齐 |
| DX12，工具栏加速与设置 | 速度从 5 变成 10，反射表单同步显示 10 |
| DX12 保存，Vulkan 重开 | JSON 和 Inspector 保留已提交旋转与 (2,2,2) 缩放 |
| Vulkan，滚轮一格 | 适量推进相机，Gizmo 保持固定屏幕尺寸，未增加 World 修改 |
| Vulkan，Alt+Space 系统菜单后 Esc，再拖 Gizmo | 正常平移，未因 Alt release 被系统吞掉而误入 orbit |
| Vulkan，Play / Stop | Play 隐藏 Gizmo、使用场景视图；Stop 恢复此前编辑相机与 Gizmo |
| Vulkan，拖出 Viewport tab | 原生浮动窗口显示场景与 Gizmo；可拖拽，Ctrl+Z 撤销，E 切换旋转 |
| 最终构建 Vulkan / DX12，各 `--frames 240` | 均 exit 0；设备成功创建，renderer host 正常停止，无 ERROR 日志 |

原生验收发现并修复：滚轮输入需按 120 单位/格归一化；鼠标按下需采样当前修饰键，避免 Alt 状态残留并支持先按 Shift 再按右键；反射表单中的转向灵敏度改用度/像素，避免默认值显示为 0.00。

原始构建、测试及最终冒烟日志保存在被 Git 忽略的 `output/gizmo-controls-verification/`。最终后端日志为 `smoke-vulkan-final.log`、`smoke-dx12-final.log`。

## 验证边界

- 原生观察覆盖 Windows Vulkan/DX12。Cocoa 鼠标修饰键采样已对称接入，但未在 macOS 编译或验证 Metal；现有非 Windows EditorRenderer 仍为 stub。
- RMB+按键持续飞行、MMB 和 Alt 组合拖动、不同物理显示器 DPI 切换由 core/shell 回归覆盖对应逻辑；当前桌面自动化不支持保持右/中键同时发送独立输入，未把这些项目计为完整原生组合输入验收。
- 自定义 provider 通过 core 与 shell 测试验证绘制/命中一致性，未额外加载第三方 provider DLL。
- 字母快捷键需输入法将原始字母键交给应用；本机中文 IME 会优先进入组词，原生快捷键验收使用该应用的英文输入模式。
- 首期为置顶 Gizmo overlay、单选 Transform 编辑；无场景深度遮挡、视口实体拾取或多选变换。

OpenSpec strict 校验和 diff 检查通过。代码和本 change 产物保留在工作区，未 commit 或 archive。

## 第二轮 review 与优化（2026-10-22）

本轮在原有 change 内关闭上轮三项 P2，并修正自定义填充轮廓的边界命中不一致：

| 问题 | 修正与证据 |
|---|---|
| RMB 导航中按下/释放 LMB 会提前丢失捕获，随后视口外释放 RMB 不能结束手势 | UIEventRouter 按 pointer + 起始 button 保持 capture，消费其它按钮边沿；SandboxModule 的原生 capture 跟随 surface router。新 Shell 回归通过真实 router 覆盖主/浮动 surface 上的左/右/中键手势、视口外释放及释放后移动；UI 回归确认其它 pointer 仍独立 |
| 0.1° 旋转被四元数 dot 容差当成无变化 | 使用符号对齐的四元数分量差；新 core 回归确认最小吸附值提交一条命令、Undo/Redo 精确恢复，以及零位移不增加命令 |
| 相机倾斜时 W+E / W+Q 的实际飞行速度偏离设置值 | 在世界空间合成视线/上下方向后归一化；新 core 回归覆盖正负 45° 和近极点、同向/反向组合及半幅输入 |
| 自定义 filled polygon 未设置 closed 时最后一条边缺少命中容差 | 填充轮廓始终闭合，closed 仅控制折线；扩展 provider 回归确认闭合边容差与开放折线区别 |

架构状态 **OK**：消费接口/控制/数学仍属于 EditorCore；Shell 绘制与输入、Module 组合、EditorRender 相机快照分工不变。新增 capture button 数据留在已有 UIEventRouter，Shell 仅暴露按 surface 查询，Module 调用已有 NativeWindow 接口；没有反向依赖、新 target、装载入口或目录移动。复查未发现尚未处理的 P1/P2；上述四项均已修正并验证。

本轮新增 4 个回归用例并扩展 1 个既有用例。相关目标（包括 UICoreTest 和 SandboxModule）重建成功，完整复跑结果如下；原先 FrameworkTest 的 180 项通过属于前一轮证据，本轮未重复计入。

| 测试程序 | 本轮通过 |
|---|---:|
| UICoreTest | 186 / 186 |
| EditorCoreTest | 177 / 177 |
| EditorShellTest | 134 / 134 |
| EditorWidgetsTest | 34 / 34 |
| EditorRenderTest | 6 / 6 |
| SandboxProjectTest | 11 / 11 |
| 合计 | **548 / 548** |

最新构建在 Windows Vulkan、DX12 上各运行 240 帧，均 exit 0、Renderer host 正常停止，无 ERROR 日志。日志为 `output/gizmo-controls-verification/review-build.log`、`review-<TestName>.log`、`review-smoke-vulkan.log` 与 `review-smoke-dx12.log`。

本轮原生运行是模块加载/渲染/退出冒烟；组合按键验证来自真实 UI router 的自动化用例，未扩充桌面工具不支持的原生按键组合验收。macOS/Metal、物理多 DPI 和第三方 provider DLL 的验证边界保持上节所述。

实体 Pick 独立跟踪在 `sandbox-viewport-picking`，proposal/design/specs/tasks 已齐备并通过严格校验，本轮未实现。两项 change 均未 commit 或 archive。

## 导航延迟修复（2026-10-22）

后续 RMB/Vulkan 卡顿排查发现帧积压，以及大角度旋转会触发 `Invalid UI vertex` 的独立问题；因此上节 review 结论仅代表当时已覆盖的范围。本轮按用户授权修复如下：

- `UIPaintContext::AddTriangle` 在 surface transform 后、快照编码前裁剪到有效 clip，保留 opacity、批次和索引语义。屏幕外有限坐标不再原样进入 GPU 快照；非法非有限输入仍进入原有显式校验。
- EditorRender 的私有 `EditorFramePacer` 限制 2 个已提交、未完成的帧，生成下一份 UI/相机快照前检查额度。精确 `FrameCompleted` 回执归还额度，CPU apply、重复或未知回执不提前放行。BUSY/REJECTED 保留封包，关闭沿用 RendererHost drain，无 sleep、丢帧或生命周期命令重排。
- Vulkan checked acquire 移除内部重复的完整 surface 查询，保留已知失效状态检查、native acquire 和 FrameScheduler 获取图像后的实时复核；每帧正常路径的完整查询由 3 次降为 2 次，不缓存能力或改动 fence/lease 退休。
- SandboxModule 去掉每次 host tick 的重复 action 刷新；Layout 及输入处理仍更新/校验状态，图标重绑按 action/scope/SetIcon 变化触发。

### 自动化与原生回归

| 测试程序 | 本轮通过 |
|---|---:|
| UICoreTest | 190 / 190 |
| EditorCoreTest | 177 / 177 |
| EditorShellTest | 136 / 136 |
| EditorWidgetsTest | 34 / 34 |
| EditorRenderTest | 8 / 8 |
| SandboxProjectTest | 11 / 11 |
| Aurora.Renderer.GpuSubmissionTest：FrameOutputs | 101 / 105，4 项 Vulkan 专属用例在 DX12 下跳过 |
| 合计 | **657 通过，4 项预期跳过，无失败** |

新增回归覆盖三种工具完整相机旋转的顶点边界、裁剪后覆盖面积、transform/opacity/批次索引、全离屏/空 clip、非有限值校验边界；帧节流的背压/拒绝、控制命令保留、乱序/重复/无 GPU 工作完成通知与 reset；图标后加载/替换；以及真实 Vulkan 每帧查询次数。输出调度回归同时覆盖 Vulkan Dynamic Rendering、RenderPass 和 DX12 的 resize、最小化、acquire 中变化、错误、关闭和退休。

专用工程加载并选中 Cube，内部持续调用相机 Look 以每秒 30 度回放；Vulkan 和 DX12 各运行约 25 秒，两圈以上旋转均 exit 0、renderer host 正常停止，stderr 为空，无 `ERROR`/`Invalid UI vertex`。这验证了真实绘制/快照/后端链路，未冒充 OS 右键鼠标组合输入验收。

### 性能对照

Windows Debug，RTX 3080，Vulkan validation 开启，输出 2552×1319，使用相同 fixture 与 ±0.3 rad、2 rad/s 相位的相机 yaw 回放。指标为快照生成到宿主收到 GPU 完成通知，包含 CPU 排队和观察间隔，不是鼠标到屏幕延迟；以下 stage 时间是 CPU wall time，不是 GPU shader 时间。

| 指标 | 修复前 | 修复后 |
|---|---:|---:|
| 快照到完成通知 P50 | 141.759 ms | 54.278 ms |
| 快照到完成通知 P95 | 155.732 ms | 59.367 ms |
| 快照到完成通知 P99 | 163.740 ms | 64.155 ms |
| 未完成快照数 P50/P95 | 5 / 5 | 2 / 2 |
| GUI 构建 P50 | 5.917 ms | 4.332 ms |
| Renderer BuildFrame P50 | 20.370 ms | 14.432 ms |
| Acquire 阶段 P50 | 6.474 ms | 3.381 ms |

快照延迟中位下降约 61.7%。修复前采样 5000 个 host tick（663 个完成样本）；修复后固定 25 秒（887 个完成样本），因为去掉重复 action 刷新后 host tick 大幅加快，不能把 tick 当作渲染帧数。高频 host tick/poll 指标在后测每 256 次采样一次，逐帧延迟和构帧计时均逐帧记录。启动/资源准备最大延迟仍包含在原始日志，不以它代表稳定交互，也不宣称降低了整个进程的 CPU 占用。

CPU review **OK**：频率控制归编辑器 host，UI 构建仍在主线程，未把生命周期/渲染工作移入控制器；大块剩余成本为现有全量 UI 快照和帧图编译。Desktop GPU review **OK**：无新增 pass/draw 策略或等待，获取后配置复核、错误和退休测试通过；Metal 原生验证仍未执行。

日志位于 `output/gizmo-controls-verification/latency-*.log`；性能基线为 `probe/oscillate.log`，后测为 `probe-fixed/oscillate-timed.log`，完整旋转为 `probe-fixed/full-turn-timed.log` 和 `probe-fixed/full-turn-dx12-timed.log`，旁边保留原始 CSV。临时探针的三个源文件均从修复后的逐字节备份恢复，保留正式修复。

移除探针后 `SandboxModule`/`SandboxEditor` 重新构建成功；最终普通版本在 Vulkan、DX12 上各以 `--frames 500000` 有界运行（此参数计 host tick），均 exit 0、host 正常停止，无 ERROR 或探针日志。记录为 `latency-final-build.log`、`latency-final-vulkan.log` 和 `latency-final-dx12.log`。OpenSpec strict 和 diff 检查通过；未 commit/archive，Pick 提案未修改。

## 延迟分解与优化编译对照（2026-10-22）

本轮为分析，不修改正式渲染策略或构建配置。使用同一验证工程、选中 Cube、2552×1319 输出、Vulkan、两帧在途上限和相机 yaw 回放，五次运行各 25 秒。主线程、renderer coordinator 与 Vulkan 的临时探针记录相同 steady-clock 时间轴，按 packet sequence 关联；剔除开始 3 秒，只分析具有完整首尾记录的帧。记录缓存于内存，退出时写 CSV。不同阶段的独立 P50 不相加；下面分解使用同一组完整帧的均值。

首次关联采样有 816 个稳定帧，延迟 P50 为 **53.432 ms**、P95 为 **57.903 ms**、均值 **53.769 ms**，复现此前 54.278 ms 的量级。各段互不重叠：

| 从快照开始到宿主完成通知的阶段 | 均值 ms |
|---|---:|
| 主线程构建并提交 packet | 7.004 |
| Accepted 到 coordinator 开始 Validate | 13.854 |
| Validate、CPU apply 前段到 PacketApplied 发出 | 1.029 |
| apply 后段、窗口查询、场景 prepare 到 frame ready | 4.496 |
| frame ready 到 BuildFrame 开始 | 6.267 |
| BuildFrame 完整调用 | 14.275 |
| 构帧结束到开始 submit | 0.046 |
| submit 调用 | 0.578 |
| submit 返回到 coordinator 观察到 GPU 完成 | 6.201 |
| 完成事件到主线程接收 | 0.020 |
| 合计（未四舍五入） | **53.769** |

`PacketApplied` 在 `Applied()` 中途、窗口查询之前发出，不能将其当成整个 apply 阶段结束。`Prepared()` 会调用 `Pump()`，其中包含呈现、回收与 BuildFrame；此前 20.686 ms 的 Prepared 不能再与 14.432 ms 的 BuildFrame 相加。第二次增加原生查询计时后确认，frame-ready 到 BuildFrame 的间隔主要是 **上一帧的 vkQueuePresentKHR**，不能统称为等待 frame slot。

### 同源码 Debug / Release 对照

随后四次采用同一套更细的探针，分别构建当前源码 Debug 与 Release，并运行 validation 开/关两组。均正常退出、stderr 为空。Release 保留 Sandbox 所启用的 `SKY_DEVELOP=1`；它是优化编译对照，并非已经新增或验证 Develop 配置。

| 配置 | 完整稳定帧 | 延迟 P50 ms | P95 ms | 均值 ms |
|---|---:|---:|---:|---:|
| Debug，validation 开 | 769 | 56.906 | 62.865 | 57.065 |
| Debug，validation 关 | 881 | 51.313 | 55.740 | 49.792 |
| Release，validation 开 | 1976 | 21.941 | 26.236 | 22.247 |
| Release，validation 关 | 2027 | 21.335 | 25.481 | 21.681 |

以下为 validation 开启时各 CPU 阶段的均值，属于嵌套阶段对照，不是可相加的延迟分解：

| CPU 阶段 | Debug ms | Release ms |
|---|---:|---:|
| GUI 构建（布局/绘制等） | 4.405 | 0.301 |
| UI 快照转换/编码 | 2.597 | 0.245 |
| renderer packet Validate | 1.025 | 0.116 |
| UI feature 构帧（再次解码、转换/上传、声明绘制） | 3.343 | 0.220 |
| FrameGraph Compile | 5.084 | 0.257 |
| Dispatch | 1.266 | 0.101 |
| BeginViewport 查询 | 3.893 | 3.347 |
| Acquire（含获取后复核） | 4.014 | 3.422 |
| 单次 vkQueuePresentKHR | 5.710 | 3.237 |
| BuildFrame 完整调用 | 15.022 | 4.104 |

优化编译显著降低 UI 和帧图的 CPU 开销，优化态主要剩余成本转为 WSI 查询、呈现及其串行排队。优化态关闭 validation 只把 P50 从 21.941 降至 21.335 ms；一次顺序 A/B 不能消除系统/驱动波动，但足以否定“只关 validation 就能解决”的判断。

### 原生大头与优化次序

1. **补齐优化编译的开发模式。** 当前 Debug 使用 `/Od /Ob0 /RTC1`、Debug CRT；Release 使用 `/O2 /Ob2`、Release CRT。Debug 中 UI 与容器/帧图工作被显著放大，不能仅用它作为日常交互性能基线。
2. **拆分 surface 实时状态查询和格式协商。** `QueryConfiguration` 每次调用两遍 `vkGetPhysicalDeviceSurfaceFormatsKHR`（数量、数据），每帧两次完整查询，即四次格式枚举。Debug 原生采样的两类调用分别平均 2.020/1.845 ms，格式枚举约占单次配置查询的 98.7%；capabilities 查询仅 0.012 ms。Release 单次完整查询仍为 3.380 ms，每帧约 6.8 ms。优先将格式/模式协商移至创建、重建及明确失效路径；必须保留获取后实时状态复核，并覆盖 resize、跨屏/HDR、surface lost 与格式变化，不能无条件永久缓存整个 SurfaceConfiguration。此项尚未实现，6.8 ms 是暴露的成本，不是承诺全部可消除的收益。
3. **处理 Present 对协调线程的占用及完成轮询时机。** 原生采样确认 frame-ready 到 BuildFrame 间隔中的绝大部分落在上一帧 Present：Debug 约 5.7 ms、Release 约 3.2 ms。日志 `presentMode=0` 对应 IMMEDIATE，不能归因于配置了 FIFO VSync。Vulkan 明确允许 [vkQueuePresentKHR 阻塞](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)。需要先审视呈现/构帧/轮询调度；若隔离到其它线程或队列，须保持原生 queue 同步、现有 slot/lease、present fence 与资源退休契约，不能直接丢给线程池。
4. **保留 UI dirty/快照缓存作为后续优化。** 绘制确实没有复用 dirty 子树结果；但优化态 GUI + snapshot + UI feature 的均值合计约 0.77 ms，优先级低于约 6.8 ms 的重复 surface 查询。缓存仍有助于 Debug 体验、空闲 CPU、复杂 Inspector 和多窗口扩展；应贯通绘制 revision、快照及 GPU buffer，而不是直接跳过 Paint。
5. **暂不优先重写帧图编译或优化 shader。** Compile 从约 5.08 ms 降到 0.26 ms；已有 device 级 PSO cache，不能将该计时误称为每帧重新编译 shader。后续先补 GPU timestamps，再确定 GPU 执行侧是否另有大头。

提交后到完成观察这一段仍包含 GPU 执行及 coordinator 轮询间隔。Debug 原生采样每帧都在提交后的第一次 poll 发现完成，首次 poll 平均发生在提交后约 6.38 ms；Release 约 3.61 ms，绝大多数也是第一次 poll 即完成。**这不证明 GPU 执行用了 6.38/3.61 ms，也不能宣称 GPU 不构成瓶颈。** 本轮未加入 GPU timestamp，亦非鼠标到屏幕延迟测量。主线程收到完成事件的额外延迟只有约 0.002–0.020 ms，事件投递本身不是大头。

### Develop 建议边界

建议单独补齐 Develop 开发构建：以现有 `RelWithDebInfo` 为基础提供 develop 入口，使用优化编译、PDB、Release CRT 和三方 Release 库，保留 `SKY_DEVELOP` 的开发设施，并独立控制引擎不变量断言、Tracy 与 Vulkan validation。不要用定义 `_DEBUG` 的方式恢复断言，以免混入 Debug CRT/STL ABI。

当前 `SKY_DEVELOP` 是 editor/tool 功能意图，Sandbox 在 Debug 和 Release 中都会启用它；它不是第三种优化配置。CMake cache 虽列出 RelWithDebInfo，但 `cmake/thirdparty_helpers.cmake`、部分 FindSlang/Findassimp/Findcpython/Findtracy 分支只匹配 Debug/Release，需要一起补齐库选择、CPython LTCG、运行库部署以及断言策略后，才能称为可用 Develop 模式。本轮按“先分析”的范围没有修改这些配置。

CPU review **Risk**：已确认重复 UI 工作、WSI 查询以及串行呈现导致延迟；优化基线和 WSI 路径优先。Desktop GPU review **Risk / 证据未闭合**：已有原生 API CPU 耗时，缺少 GPU 执行时间，不能用回执延迟代替 GPU profile。

原始记录位于 `output/gizmo-controls-verification/trace-breakdown/`，分别为 `debug`、`debug-native`、`debug-no-validation`、`release-native`、`release-no-validation` 的 host/runtime/vulkan CSV 与 summary JSON。`analyze_latency_trace.py` 记录关联、热身裁剪与均值/P50/P95算法；`manifest.json` 保存四份探针源文件的前后 SHA-256，源码按备份逐字节恢复。

清理后普通 Debug/Release 的 SandboxModule 与 SandboxEditor 均重建成功，各以 Vulkan `--frames 100000` 完成启动/渲染/退出冒烟，exit 0、stderr 为空、无 ERROR，renderer host 正常停止。Release 收尾首次遇到 CMake 生成标记时间戳访问错误，在正常用户权限下重新配置现有构建目录后成功，未修改构建配置规避。OpenSpec strict 和 diff 检查通过；本轮保留分析文档及被忽略的测量记录，没有额外生产代码修改，未 commit/archive。
