---
name: skyengine-aurora-shader
description: Author and migrate SkyEngine Aurora shaders, shared declarations and module resource registrations.
---

# SkyEngine Aurora Shader

Read `docs/features/shader-system.md` and the active target-shader-artifacts change before editing.

- C++ `.h` declarations are authoritative. C++ does not depend on Slang or generated C++ mirrors.
- Export pure `.slangh` with `python/shader_headers.py` / `ShaderHeaders.Update`, then run `ShaderHeaders.Check`.
  Persist and version the outputs with their declarations. Never make tracked exports CMake clean byproducts.
- Aurora's only shader asset root is `engine/aurora/assets/shaders/`; includes use `aurora/...`.
  Each plugin has `plugins/<name>/assets/shaders/`; includes use `<name>/...`, without `plugins/`.
- Define exports, include roots, explicit resource dependencies and programs in one module resource description.
  Reuse plugin.json registration and existing MultiFileSystem/ShaderFileSystem; no separate shader plugin loader.
- Slang programs include the exported interfaces, never the C++ declaration `.h` or SharedShader.h.
- Bindings are Global set 0 / Pass set 1 / Material set 2 / Batch set 3. Use `[[vk::binding(binding, set)]]`.
  Generated declarations carry AuroraResource/AuroraField attributes and use flat ConstantBuffers.
  Each requested target supplies its own complete ABI; never recover Metal identity from SPIR-V or short names.
- View inputs belong to their Pass. Scene techniques match queue Tags; fullscreen/compute select explicit techniques.
  Batch can be absent when reflection has no active Batch resources. Material and Feature providers must not overwrite each other.
- ShaderRef remains a canonical relative path/entry identity. Stage is explicit in compile requests.
  Do not turn material shaders into a universal ScenePass shader or introduce implicit program fallbacks.
- Companion `.slang.json` files define entries, schemas and dependencies. Program requirements and explicit variants feed project cook.
- Use one distribution index per target, preserving every selected owner's paths. Shipping resolution receives product roots.
  Native source compilation is a development mode; runtime packaging must work without sources or Slang.
- Source registration is transactional. Validate inventories, case collisions, include ambiguity and dependency closure.
  Preserve immutable compile snapshots and normal module/GPU retirement contracts.
- Validate semantic CPU schema compatibility and target UniformUploadPlans, then affected native renderer tests.
  MSL generation on Windows is not native Metal validation.

- Material payload version 4 / instance version 2 persist canonical typed values, textures and samplers.
  Generate keys from complete Group 2 declarations (zero/one cbuffer) and Shader-domain options; aliases are legacy migration input.
  Recook cache format 10 / usage format 3. Published schemas retain V/P/S ownership, typed values and full allowed selections.
- Stage artifacts own exact bytes, native entry, full declarations and target reflection.
  Shader linking must derive layouts from those artifacts; dynamic uniform policy is explicit.
- DEVELOPMENT requires source + current producer. PRODUCTS uses published environment/request → ArtifactID
  without consulting an installed compiler. Package target platforms/options are explicit.
- Query producer identity for the actual target; DXIL additionally binds the pinned deployed DXC/DXIL library contents.
  Keep HLSL register-space annotations DXIL-only; reject actual MSL argument collisions and never renumber metadata.
- ResourceGroup and graph adapters delegate to one RHI ShaderParameterTransaction; separate materializers preserve native/graph lifetimes.
  ResourceGroup uses Name-first typed DescriptorEncoder updates with an injected neutral context; do not duplicate transaction validation in Core.
  End returns a new immutable snapshot, never submits GPU work; any failure prevents publication.
- Cache upload plans by shader generation and CPU schema; encode target offsets/strides/major, clear padding.
  Keep Material and Feature-owned Batch providers distinct; combined blocks require one assembler.
- Keep Global frame-wide and put camera inputs in each concrete Pass. Material CPU residency needs no dummy GPU upload.
- Default MSL profile metallib_2_3 still emits MSL source; native compilation requires macOS 11 / iOS 14.
  An iOS 13 cross-build is not native runtime validation. Do not imply shipping products are metallib binaries.
- Check tasks.md for device coverage and explicit Unsupported boundaries; never restore ordinal slot fallbacks,
  retired shader embedding or generated C++ mirrors.


变体补充（2026-10-25）：Core 统一归一化 Vertex / Pipeline / Shader 三域；Material、Batch、fullscreen、compute 是 S 的不同提供者。
业务 API 只传 bool/int32/uint32 Key/Value，JSON 关联 source symbol；compile/native lowering 与 native ID 由对应 target 的源码声明反射决定。
完整选择、stage code、executable 身份分离，native ID 按 stage 独立；产品必须显式发布完整选择。Group 2 零或一个 cbuffer，已知 inactive 值保留并验证类型。
Scene Prepare 在 graph 声明前完成附加程序准备；Scene Pipeline 的四个 SHADOWS/IBL 请求是显式策略，不自动遍历所有选项组合。
