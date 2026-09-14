## Context

RDG 三步重构与 pipeline template 层均已落地。当前状态：

- `RenderGraph` 提供 6 种 pass 原语，其中 `SceneRasterPassBuilder`（`ColorAttachment`/`DepthStencilAttachment`）与 `FullScreenPassBuilder`（`SetTechnique`/`SetTarget`/`SetDepthStencil`/`SetInputSRV`）已能声明 render target 与输入。
- `Execute.cpp` 已按 payload 的 attachment + load/store 生成 `GraphicsEncoder::BeginRendering(RenderingInfo)` → pass body → `EndRendering()`（SCENE_RASTER 与 FULLSCREEN 两分支）。
- pipeline 层只有 `OpaquePass` 一个示范（LDR `RGBA8_UNORM` color + `D32` depth + 一个 opaque queue），其 PSO 创建仍是 TODO 占位。
- `FullScreenPassBuilder::SetInputSRV(handle)` 只记录 `AddRead(handle, SRV)`（供 barrier/culling），实际采样绑定走 pass-tier ResourceGroup（set 1）。
- `aurora-queue-submit-present`（Queue/Submit/SwapChain Present）尚未完全落地，backbuffer/swapchain image 以 `Import` 方式进入 RDG 是既有约定。

本 change 从 pipeline 层固定 renderpass begin/end 声明契约，并补两个具体 pass 打通「场景 → HDR → 屏幕」最小链路。

## Goals / Non-Goals

**Goals:**

- 固化 renderpass begin/end 声明模型：pass 只声明「color/depth attachment + load/store/clear + render area」，begin/end 由 RDG executor 统一生成。
- `ScenePass`（HDR）：继承 `SceneRasterPassTemplate`，声明 `RGBA16_SFLOAT` HDR color + `D32` depth（CLEAR）+ 一个 `opaque` queue。
- `TextureToScreenPass`：继承 `PipelinePass`，声明 FullScreenPass —— 输入 HDR texture（SRV read），输出 imported backbuffer（color attachment, DONT_CARE/STORE），全屏三角形。
- 保留 `OpaquePass` 不变。

**Non-Goals:**

- 不做 tonemap / 曝光（`TextureToScreenPass` v1 直通拷贝，后续 change 处理）。
- 不做真正的 PSO / shader 管线接入（与 `OpaquePass` 一致，`OnSetup` 留 TODO 占位，等 shader 管线落地）。
- 不做 swapchain 获取（`aurora-queue-submit-present` 的职责）；本 change 只消费「imported backbuffer texture handle」。
- 不改 RHI/RDG 接口、不动旧 render/core legacy。

## Decisions

### 1. begin/end 归属：RDG executor，pipeline 只声明 attachment

renderpass 的 `BeginRendering`/`EndRendering` 不新增 RHI 接口，也不在 pass 内手写。pipeline 层唯一职责是**声明** render target 集合（color 槽位 + depth/stencil + load/store/clear + render area），executor 在编译后生成对应的 begin/end。

理由：动态渲染下 begin/end 只是 attachment 描述的编译产物；pass 手写 begin/end 会破坏 RDG 的 barrier/culling 分析（execute lambda 内手写 barrier 已被明令禁止，begin/end 同理）。

**备选**：给 `PipelinePass` 增加虚 `GetRenderTargets()` 返回一个配置结构，由框架统一喂给 builder。→ 未采用：`SceneRasterPassTemplate`/`FullScreenPassBuilder` 的声明式 API 已足够，额外抽象收益低、且与本 change「简单补充」的定位不符。留作后续多 render target pass（GBuffer）出现时再抽象。

### 2. `ScenePass`（HDR）结构

```cpp
class ScenePass : public SceneRasterPassTemplate {
    // SetExtent(w,h)（与 OpaquePass 一致）
    // BuildRDG: CreateTexture(HDR color, RGBA16_SFLOAT, RENDER_TARGET|TRANSFER_SRC)
    //           CreateTexture(depth, D32, DEPTH_STENCIL)
    //           AddSceneRasterPass:
    //             ColorAttachment(0, hdr, CLEAR, STORE)
    //             DepthStencilAttachment(depth, CLEAR, DONT_CARE, DONT_CARE, DONT_CARE)
    //             DeclareQueue("opaque", FRONT_TO_BACK) + Collect
    //           MarkOfInterest(hdr)
    // GetHDRColorHandle() / GetDepthHandle()
};
```

HDR color 选 `RGBA16_SFLOAT`：16-bit float 是桌面/移动通用的 HDR 中间缓冲格式（`RGBA32_SFLOAT` 带宽成本高，`RGBA16_UNORM` 无 HDR 范围）。clear 值用 `ClearValue(0,0,0,0)`（黑透明）。

`Collect` 沿用 `SceneRasterPassTemplate` 默认实现（cull-only，technique 分桶留待后续）。

### 3. `TextureToScreenPass` 结构

```cpp
class TextureToScreenPass : public PipelinePass {
    // SetInput(RDGTextureHandle hdr) / SetOutput(RDGTextureHandle backbuffer)
    // BuildRDG: graph.AddFullScreenPass(name, [&](FullScreenPassBuilder &b) {
    //     b.SetTarget(mOutput, LoadOp::DONT_CARE, StoreOp::STORE);  // 全屏覆盖，无需 clear/load
    //     b.SetInputSRV(mInput);                                     // SRV read → barrier/culling 依赖
    //     if (GetPassResourceGroup()) b.SetPassResourceGroup(...);  // set 1（采样输入 texture）
    //     b.SetTechnique(GetPSO());                                  // 全屏三角形 PSO
    // });
    // GetPSO() 由 OnSetup 创建（TODO 占位）
};
```

输入采样绑定走 **pass-tier RG（set 1）**：`SetInputSRV` 只建立 RDG 的 read 依赖（保证 HDR texture 从 RTV 转 SRV 的 barrier + culling），实际采样描述符由 pass RG 提供。

**关键点**：HDR texture 是 transient RDG 资源，其 backing image 由 transient pool 在 `Compile()` 时解析，每帧可能变化。因此 pass RG 中指向该 texture 的 SRV 描述符**必须每帧重写**（`Compile()` 之后、`Execute()` 之前）。v1 通过 `SetInputSRV` 只做声明，描述符写入随 shader 管线接入一并落地；此契约在设计上明确为「per-frame write」，后续由 frame 编排器在 `Compile` 后调用 pass 的 descriptor 更新。

### 4. backbuffer 以 Import 进入 RDG

`TextureToScreenPass::SetOutput` 接收一个 `RDGTextureHandle`，由上层（frame 编排器 / renderer change）通过 `graph.Import(name, swapchainImage, AccessFlagBit::NONE)` 产生。pass 本身不负责获取 swapchain image，避免与本 change 无关的 present 时序耦合。

### 5. PSO / shader 接入延后

`ScenePass::OnSetup` 与 `TextureToScreenPass::OnSetup` 的 PSO 创建均为 TODO 占位（与 `OpaquePass` 一致），因为 shader 管线（`aurora-shader-derived-pso` 链路 + 真实材质）尚未接入。本 change 聚焦 renderpass begin/end 声明与 RDG 结构，PSO 为 `nullptr` 时 executor 跳过 draw（现有行为）。

## Risks / Trade-offs

- **[transient input SRV 的 per-frame 写]** `TextureToScreenPass` 采样的 HDR texture 是 transient 的，SRV 描述符需每帧重写，否则采样到上一帧的 aliased image。→ 缓解：设计明确 per-frame write 契约；若后续证明 transient aliasing 收益低，可将 HDR color 标记为 `PERSISTENT`。
- **[PSO 占位导致 draw 被跳过]** `OnSetup` 未建 PSO 时 executor 不绘制，smoke test 只能断言 RDG 结构而非像素。→ 缓解：测试断言 CompiledGraph 的 attachment/pass 结构，不依赖实际 draw。
- **[HDR 格式平台兼容]** `RGBA16_SFLOAT` 需保证 `RENDER_TARGET` + `SAMPLED` 能力（移动/桌面均支持，但个别低端设备对 16f 混合有约束）。→ 缓解：HDR target 不参与混合（opaque 队列），仅作为全屏采样源，规避混合能力问题。
- **[范围蔓延]** 若把 tonemap / swapchain 也纳入本 change，会与 `aurora-queue-submit-present` 重叠。→ 缓解：Non-Goals 明确排除，`SetOutput` 只收 handle。

## Migration Plan

1. pipeline 层新增 `ScenePass`（HDR）与 `TextureToScreenPass`（声明式 BuildRDG + TODO OnSetup）。
2. `AuroraPipelineTest` 加两个 BuildRDG smoke test（断言 attachment/pass 结构）。
3. 全量构建 + 测试通过。
4. archive 本 change；后续 change 接 PSO/shader 与 swapchain 真实接入。
