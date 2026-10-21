---
name: skyengine-aurora-shader
description: Author and migrate SkyEngine Aurora shaders, shared declarations and module resource registrations.
---

# SkyEngine Aurora Shader

Read `docs/features/shader-system.md` and the active resource-ownership change before editing.

- C++ `.h` declarations are authoritative. C++ does not depend on Slang or generated C++ mirrors.
- Export pure `.slangh` with `python/shader_headers.py` / `ShaderHeaders.Update`, then run `ShaderHeaders.Check`.
  Persist and version the outputs with their declarations. Never make tracked exports CMake clean byproducts.
- Aurora's only shader asset root is `engine/aurora/assets/shaders/`; includes use `aurora/...`.
  Each plugin has `plugins/<name>/assets/shaders/`; includes use `<name>/...`, without `plugins/`.
- Define exports, include roots, explicit resource dependencies and programs in one module resource description.
  Reuse plugin.json registration and existing MultiFileSystem/ShaderFileSystem; no separate shader plugin loader.
- Slang programs include the exported interfaces, never the C++ declaration `.h` or SharedShader.h.
- Bindings are Global set 0 / Pass set 1 / Batch set 2. Use `[[vk::binding(binding, set)]]`.
  Generated uniform declarations use flat ConstantBuffers; mat4 is column-major.
- View inputs belong to their Pass. Scene techniques match queue Tags; fullscreen/compute select explicit techniques.
  Batch can be absent when reflection has no active Batch resources. Material and Feature providers must not overwrite each other.
- ShaderRef remains a canonical relative path/entry identity. Stage is explicit in compile requests.
  Do not turn material shaders into a universal ScenePass shader or introduce implicit program fallbacks.
- Companion `.slang.json` files define entries, schemas and dependencies. Program requirements and explicit variants feed project cook.
- Use one distribution index per target, preserving every selected owner's paths. Shipping resolution receives product roots.
  Native source compilation is a development mode; runtime packaging must work without sources or Slang.
- Source registration is transactional. Validate inventories, case collisions, include ambiguity and dependency closure.
  Preserve immutable compile snapshots and normal module/GPU retirement contracts.
- Validate actual SPIR-V/DXIL/MSL reflection against C++ layouts, then affected native renderer tests.
  MSL generation on Windows is not native Metal validation.

- Material payload version 2 uses `program.vertex/fragment` with explicit path and entry, variant values,
  and `uniforms` member-to-property mappings. Reject legacy single-shader inference; recook cache version 4.
- Pack material values using reflected field offsets. Keep material-owned and Feature-owned Batch bindings distinct;
  combined blocks require one assembler. PreparedRaster does not select Tags or define uniform layouts.
- Keep Global frame-wide and put camera inputs in each concrete Pass. Material CPU residency does not need a dummy GPU upload.
- Check tasks.md for native platform limits and unsupported material texture/array/deformation transport.
  Do not restore retired shader embedding and generated C++ mirror flows.
