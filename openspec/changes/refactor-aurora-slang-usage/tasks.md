# Slang usage remediation tasks

Status: implementation started. Win32 verified: Slang v2026.19, static Slang package, ShaderModule boundary, session.detach removed; ShaderTest 57/57, PipelineTest 44/44, CoreTest 90 passed/1 skipped, AdaptorTest 45/45, UIShaderTest 9/9; full build passes. Other platforms and Phase 2.1-2.9/3/4/5/6 pending. Depends on `refactor-aurora-target-shader-artifacts` (per-target ABI, UniformUploadPlan, artifact/cache) and `refactor-aurora-pipeline-resource-ownership`.

## 1. Slang dependency and teardown safety

- [x] 1.1 Upgrade Slang baseline from v2026.9.2 to **v2026.19** (contains #11614 merged commit `12f677986`); use v2026.12 as compatibility fallback. Updated `cmake/thirdparty.json`, `cmake/patches/slang.patch`, and added required `mimalloc`/`fast_float` submodules. Win32 built; other platforms pending.
- [ ] 1.2 Rebuild Slang packages: Win32, macOS, iOS static, Android, Linux; keep the platform linkage model (shared except iOS).
- [x] 1.3 Remove `session.detach()` from `ShaderCompilerSlang::Compile`; fix the dangling `sourcePath` reference if not already fixed. Verified no detach remains.
- [x] 1.4 Add regression: N sequential session create/destroy, multi-target/stage/variant/optimization, multiple rounds; assert no heap corruption. Existing `SlangSpikeTest.CompleteStageArtifacts...` covers this and passes on v2026.19.
- [ ] 1.5 Verify no behavior/ABI change in generated SPIRV/MSL/DXIL vs the previous build; record compile-time deltas.
- [x] 1.6 Treat `loadModuleFromSourceString` results as session-owned borrowed modules; do not wrap them in an owning `ComPtr`, which corrupted session teardown refcounts.

## 2. CompilerContext and shader compile thread pool

- [ ] 2.1 Add `SlangCompilerContext` owning `IGlobalSession`, `SessionPool`, `ModuleCache`, and shared `ShaderCache` integration; contexts live inside `Aurora.ShaderCompiler`.
- [ ] 2.2 Add a dedicated shader compile pool (module-hosted or host-owned) on top of existing `ShaderCompileTask` / `ShaderCompileFuture` / `sky::ThreadPool`; worker calls into ShaderModule, which uses a thread-local/per-worker `SlangCompilerContext`; no Slang object shared across workers and no render-pool use.
- [ ] 2.3 Session key: target/platform + environment/options + search paths/fileSystem + platform macros; shader-level macro variants and runtime-specialized values excluded.
- [ ] 2.4 Decouple `ShaderCompileTask`/`ShaderCompileFuture` from a fixed compiler pointer; obtain the worker-local compiler/context at execution. Refactor `ShaderCompilerSlang::Compile` to acquire/reuse a worker-local session, load module, codegen, and return; remove per-call session creation.
- [ ] 2.5 Replace the single `gCompilerMutex` serialization with per-worker contexts; make DXC/library initialization thread-safe (`Once`/explicit init); verify downstream codegen concurrency and add per-worker/mutex fallback if needed.
- [ ] 2.6 Make artifact/cache writes thread-safe and order-independent; shared blobs only, not Slang objects.
- [ ] 2.7 Tests: concurrent compile, deterministic results across worker counts/orders, session reuse across different macro variants, module identity isolation for different define variants, worker memory/worker cap.
- [ ] 2.8 Worker/context teardown: release module/session/global session in the documented order; no process-exit or `detach()` reliance.
- [ ] 2.9 Bound/clear session pool and module cache; verify long-running Editor memory does not grow without limit.
- [x] 2.10 Make `Aurora.ShaderCompiler` the only Slang owner: static-link Slang into the module on all platforms; shared module on desktop/Android/Linux/macOS, static module on iOS. Win32 verified; other platform packages pending.
- [ ] 2.11 Remove direct Slang dependencies from runtime modules: `engine/sandbox/render`, `engine/ui/render`, and any other non-test module must use `IShaderCompiler`/`ShaderCompilerFactory` or precompiled artifacts; remove `ShaderCompilerSlang.h` includes and `Aurora.ShaderCompiler.Static`/`3rdParty::slang` links. (Runtime includes/links removed; moving `ShaderCompilerSlang.h` private remains.)
- [x] 2.12 Update third-party packaging: provide static Slang libs for Win32/macOS/Android/Linux (iOS already static), including required lz4/miniz/cmark and CRT compatibility; stop deploying `slang.dll`/`slang-compiler.dll`; deploy Windows dxcompiler/dxil with the ShaderModule. Win32 verified; other platforms build pending.
- [x] 2.13 Define module load order/factory behavior when ShaderModule is absent; runtime compile callers get an explicit error or use precompiled artifacts. Editor module dependency and UIRender/EditorRender factory checks added.
- [x] 2.14 Allow tests/offline tools to link a private static compiler target; assert they are not production runtime module dependencies.
- [ ] 2.15 Verify ShaderModule packaging: Develop includes it by default; release omits it by default; add an explicit release opt-in for runtime compilation; Windows/macOS/Linux/Android shared + static Slang, iOS static module. Win32 static module verified; release opt-in pending.

## 3. Unified variant interface

- [ ] 3.1 Extend `ShaderVariantEntry` with `CompileTimeMacro | RuntimeSpecialized`; keep Macro as default for compatibility.
- [ ] 3.2 Keep `ShaderVariantSchema` bitmask macro-only; add a separate specialization descriptor for runtime variants (name/type/default/spec id or root constant range).
- [ ] 3.3 Unified setting API: callers set `Name→value` through `ShaderVariant` (compile-time) and `ShaderVariantValues` (runtime); no Vulkan/DXIL/Metal-specific spec/root-constant choice.
- [ ] 3.4 Macro variants: platform macros fixed at session creation; other macros canonicalized into a source prelude (`#define NAME VALUE`), included in sourceHash; give each prelude a unique module identity (module name/path hash) to avoid session module-cache collisions; session pool groups only by target/platform/environment; reject invalid/colliding macro names; document that session reuse does not imply module reuse across macro sets.
- [ ] 3.5 Runtime specialized variants: add `TargetEnvironment` capability for specialization constants/root constants; implement spec-constant path for SPIRV/MSL and root constants for DXIL/D3D12; other backends explicit Unsupported. Do not add a Vulkan push-constant path. Add capability detection, cache-key participation, and tests.
- [ ] 3.6 Add generated variant declaration/accessor (from schema or cook) so shader source does not branch on backend; document the generation tool placement.
- [ ] 3.7 Ensure artifact identity excludes runtime values but includes compile-time macro values and the composed source prelude; update `ShaderCacheKey`/`ShaderUsage`.
- [ ] 3.8 Add pipeline/material binding for runtime variant values: spec constants at PSO creation; D3D12 root constants through the pipeline/root signature path; no variant cbuffer.
- [ ] 3.9 Tests: macro variants share a session via source prelude, platform macro fixed, sourceHash changes with macros, spec path, D3D12 root constants, unsupported backend explicit error, no silent structural/runtime downgrade.

## 4. Cbuffer and SSBO layout policy

- [ ] 4.1 Remove `ReflectBlock` canonical alignment/block alignment overrides; keep native target layout.
- [ ] 4.2 Update `SharedPassHeadersTest` to validate `BuildUniformUploadPlan(cpu, native)` instead of member equality.
- [ ] 4.3 Add cbuffer tests for float2/scalar/matrix/array offsets differing per target.
- [ ] 4.4 Define the canonical SSBO layout: default layout + 16-byte fields/padding (verified reflection across SPIRV/MSL/DXIL); complex layouts use `ByteAddressBuffer`. `Std430DataLayout`/`CDataLayout` are unavailable on Metal/DXIL.
- [ ] 4.5 Forbid `float3`, require fixed-width ints, explicit padding/matrix major/array stride in SSBO declarations.
- [ ] 4.6 Define the SSBO CPU canonical schema source (not the cbuffer `SharedShaderTypes` by default); extend reflection to record storage buffer element layout (size/stride/alignment/fields/array stride/matrix stride).
- [ ] 4.7 Add `ValidateStorageLayout(cpu, native)` exact validation; fail compile/cook on mismatch.
- [ ] 4.8 Add CPU→GPU→CPU SSBO readback tests; include `ByteAddressBuffer` manual offsets.
- [ ] 4.9 Ensure cbuffer upload plan and SSBO exact validation are separate code paths; no global compiler flag silently changing layouts.
- [ ] 4.10 Route D3D12 runtime-specialized variants through root constants; validate root signature DWORD budget and pipeline binding; do not add a variant cbuffer.
- [ ] 4.11 Run the SSBO layout matrix across SPIRV/MSL/DXIL with 16-byte default layout and `ByteAddressBuffer`; record reflection + GPU readback evidence; confirm `Std430`/`CData` are rejected on Metal/DXIL.
- [ ] 4.12 Validate D3D12 root constants root signature/pipeline binding, `D3D12_MAX_ROOT_COST=64` cost budget (root constants 1 DWORD, descriptor table 1, root descriptor 2), and multi-draw/indirect behavior.

## 5. Precompiled modules and offline cook

- [ ] 5.1 Precompile base modules to `.slang-module` (`IModule::serialize`/`writeToFile`); store content-addressed by target/environment/platform macros + composed sourceHash, shared across entries/shaders (not per instance); pin Slang/module version compatibility (`get-supported-module-versions`).
- [ ] 5.2 Use `UseUpToDateBinaryModule`; cook codegen and write artifacts/index through existing cache writer.
- [ ] 5.3 Shipping mode: no source, no Slang/DXC; explicit failure when artifact missing/incompatible.
- [ ] 5.4 Development mode: compile on demand through the same compiler pool; no separate legacy path.
- [ ] 5.5 Update `ShaderUsage`/cook matrix to record macro sets, target/environment, runtime variant defaults/representation; verify reproducibility; bump cache/usage versions and recook products.
- [ ] 5.6 Re-enable/execute `ShaderDistribution.RelocatedProductsResolveWithoutSourcesOrCompiler` and equivalent platform tests.

## 6. Platform validation

- [ ] 6.1 Windows: DXIL/D3D12 + Vulkan/SPIRV compile, static-Slang ShaderModule linkage/CRT match, #11614 regression, thread pool, DXC/codegen concurrency, session teardown, cbuffer upload, SSBO readback, spec/root-constant variant paths.
- [ ] 6.2 macOS: MSL/Metal, spec constant support, thread pool, cbuffer upload, SSBO readback; no DXIL.
- [ ] 6.3 iOS: static Slang, MSL, precompiled module, device memory/compile time, thread pool.
- [ ] 6.4 Android: SPIRV/Vulkan, cook-time macro matrix, SSBO read-only/RW readback, shipping without compiler, thread pool.
- [ ] 6.5 Linux/Vulkan: offscreen compile/readback where host support exists; record unsupported host prerequisites.
- [ ] 6.6 Record compile-time/memory benchmarks per platform, per worker count, per variant category, and ShaderModule binary size.

## 7. Cleanup and documentation

- [ ] 7.1 Remove `session.detach()` and any session-pool retention used only as a workaround.
- [ ] 7.2 Remove the canonical alignment workaround; keep per-target native cbuffer reflection.
- [ ] 7.3 Keep the `sourcePath` lifetime fix, Vulkan range=0 explicit-failure test, and `RendererRuntimeTest` compile fix.
- [ ] 7.4 Update `docs/aurora/shader-design-and-migration.md` and `skyengine-aurora-shader` skill.
- [ ] 7.5 Update `refactor-aurora-target-shader-artifacts` relationship notes; do not rewrite its completed evidence.
- [ ] 7.6 Run OpenSpec strict validation, full affected builds/tests, and the platform matrix; record residual unsupported cases explicitly.
- [ ] 7.7 Commit/archive only after user confirmation per AGENTS.md.
