---
name: skyengine-aurora-reference
description: Quick index of Aurora module ownership, the production FrameGraph renderer, shared shader declarations, techniques, resource packaging and test targets.
compatibility: opencode
metadata:
  source: aurora implementation (2026-10-22)
  audience: contributors
---

# Aurora 架构速查

Aurora 属于 foundation，接口与实现均位于 `engine/aurora`。命名空间为 `sky::aurora`。
先区分生产 FrameGraph 路径与仍有测试消费者的旧 RenderGraph 模板；历史 change 不是当前 API 清单。

## 模块与入口

| 目录 | 职责 |
|---|---|
| `rhi/interface` | RHI 接口、FrameGraph 声明/冻结/分析及旧 RenderGraph API |
| `rhi/{vulkan,dx12,metal,common}` | 原生资源、后端 frame compiler/encoder 与共享提交生命周期 |
| `core/include/aurora/renderer` | Renderer 公共协议；仅接口 target 为 `Aurora.Renderer.Interface` |
| `core/src/renderer` | `Aurora.Core.Static` 中的 prepare、frame、resource、graph 实现 |
| `core/include/aurora/shader` | 无 Slang 依赖的声明、technique、uniform、source inventory、resolver/cache |
| `shader` | `Aurora.ShaderCompiler.Static`；Slang 输出 SPIR-V / DXIL / MSL |
| `pipeline` | `Aurora.Pipeline.Static`；SceneRenderFeature、具体 Pass 共享声明与旧模板 |
| `adaptor` | Framework/World/资产对接、RendererHost；单向依赖 Pipeline |
| `runtime` | 既有 Aurora.Render 模块入口 |
| `assets/shaders` | 统一的 Aurora shader 源、companion 与持久化 `.slangh` |

Core 不依赖 Pipeline 或 shader compiler。Shader compiler 依赖 Core/RHI；RHI 不反向依赖 compiler。
不创建第二套 renderer core/adaptor/runtime，也不创建 Aurora 业务 plugin。

## 生产渲染路径

宿主通过 RendererHost 驱动唯一 Renderer producer。World/资产对接产生 owned packet；CPU prepare
冻结场景/视图数据，coordinator 解析精确资源版本并用 FrameComposer 组合 feature 的 FrameGraph。
后端编译、录制、提交与完成退休沿既有 DeviceFrameContext/FrameScheduler 协议进行。
`RegisterScenePass` 显式注册场景 feature；没有注册时不存在隐式 scene raster fallback。

Scene 的 technique 由 queue Tag 匹配；无匹配省略 draw，重复/歧义显式失败。材质定义各 stage 的
source/entry 和 uniform mapping，Feature 与 Material 明确分配 Batch binding 所有权。
FullScreen/Compute 直接使用 technique，可省略 Batch；活跃反射输入缺失仍必须报错。
`PreparedRaster` 处理几何、资源版本和 owned 数据，不决定通用材质布局。

`PipelinePass`、`SceneRasterPassTemplate`、`ScenePass`、`OpaquePass` 类仍供旧 RenderGraph 模板测试使用。
默认 Collect 不产生 DrawItems；不能将它当作生产场景收集器。共享 Pass 声明头也被生产 feature 消费。

## 共享声明和资源绑定

| 层级 | set | 内容 |
|---|---|---|
| Global | 0 | 跨 View 的帧数据；内置为 time/delta |
| Pass | 1 | 具体 Pass 声明的输入，相机/View 可选 |
| Batch | 2 | Scene 的 Material/Feature 输入；具体布局由消费者声明 |

权威 `.h` 的字段/资源清单生成 C++ 类型、typed slots 和描述元数据。
`python/shader_headers.py` 导出持久化 `.slangh`；实际 shader reflection 验证字段和资源布局。
`RgBlockDesc` 用于布局元数据/校验，不生成 HLSL 或独立 ResourceGroupLayout。
`aurora/renderer/PassResourceBindings.h` 校验实际 shader 使用与显式 dynamic policy；失败保持原绑定。
Metal 的 native buffer/texture/sampler 映射取自对应后端反射，不从声明顺序或 set 编号猜测。

## 模块资源与打包

- Aurora：`engine/aurora/assets/shaders/aurora/...`；插件：`plugins/<owner>/assets/shaders/<owner>/...`。
- `resources.json` 声明 roots、依赖、include roots、exports 和 programs；统一 inventory 校验重名和大小写冲突。
- C++ 包含 `.h`，shader 包含 `.slangh`。`ShaderHeaders.Update/Check` 更新/检查持久化导出；C++ 编译不依赖 Slang 或导出执行。
- `ShaderResources.Stage` 按 canonical 相对路径部署；项目 cook 聚合选中 owner 的产品，每个 target 一份 index。
- `ShaderHeaderTool` 仅保留独立 variant schema 生成；`ShaderCookTool` 生成 shader cache。
- 内置材质程序：`aurora/materials/{Unlit,Lit,Shadow}.slang`；ToneMap：`aurora/pipeline/ToneMap.slang`。
- 材质 payload v2 显式记录 `program.vertex/fragment`；shader cache index v4。旧数据需迁移/重新 cook。

详见 `docs/features/shader-system.md` 与 `docs/aurora/shader-design-and-migration.md`。

## 验证入口与限制

| target | 内容 |
|---|---|
| `Aurora.RHITest` | RHI / 旧 RenderGraph / 后端资源与提交 |
| `Aurora.CoreTest` | 场景、technique/uniform、资源 inventory、cache/product lookup |
| `Aurora.ShaderTest` | Slang 编译/反射、后端 shader、source bundle |
| `Aurora.PipelineTest` | 共享声明布局、Pass 资源绑定、旧模板 |
| `Aurora.AdaptorTest` | 材质/World/资产对接 |
| `Aurora.Renderer.GpuSubmissionTest` | FrameGraph 与 Windows Vulkan/DX12 原生渲染 |

当前证据与待验收项在 `openspec/changes/refactor-aurora-pipeline-resource-ownership/tasks.md`。
Windows MSL 编译/反射不代表原生 Metal 验收。独立 Core-only 产品消费者不链接 Slang；
原生测试 exe 为其它用例仍链接 compiler，移除 Slang DLL 的 shipping host 验收尚待执行。
自动材质纹理/sampler 传输、变形、完整 PBR/通用 PSO 系统仍不是当前能力。
