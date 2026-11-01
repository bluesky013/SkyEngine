> Status — 2026-11-01: active；剩余 owner 为 worker-local 编译池/session/module 复用、预编译模块、SSBO 精确布局及相应验证。
> typed V/P/S 与 cbuffer 目标 ABI 沿用已交付契约；不恢复宏/RuntimeSpecialized 调用方分类或 DX12 root-constant 方案，不把未来编译池集成标为完成。

## Why

This change retains the remaining Slang compiler lifecycle and throughput work. The original teardown failure and static compiler-module boundary have Windows evidence in tasks.md; worker-local contexts, bounded session/module reuse, precompiled modules and other platforms remain incomplete. The old proposal's caller-selected macro/runtime categories and DX12 root-constant variants conflict with the confirmed typed V/P/S design and are withdrawn.

## What Changes

- Preserve the implemented Slang upgrade, borrowed-module ownership and removal of session.detach; complete the remaining platform verification.
- Add bounded worker-local compiler contexts and a dedicated shader compile pool using existing task/future/thread-pool infrastructure. Do not share Slang objects concurrently across workers.
- Reuse sessions by target/platform/environment and isolate module identities for lowered source preludes. Preserve immutable source snapshots, owned compiler/cache endpoints and one execution per accepted future.
- Consume the existing typed V/P/S selector and complete Material schema. Shader declarations and target support decide macro/native lowering; DXIL uses its supported compile path with the selected values. No caller-facing category, root-constant variant or uniform fallback is introduced.
- Investigate precompiled .slang-module inputs and bounded module caches while preserving complete selection, stage code and executable identities.
- Keep per-target cbuffer ABI and UniformUploadPlan; finish independently scoped canonical storage-layout validation and native readback. Do not mark future generic SSBO support complete because the palette FLOAT4 path works.
- Preserve static Slang inside Aurora.ShaderCompiler, optional compiler-free PRODUCTS consumption and explicit release runtime-compiler opt-in work.

## Capabilities

### Modified Capabilities

- `aurora-shaders`: worker ownership, session/module reuse, teardown, precompiled module/cook integration; preserve the current variant and artifact contracts.
  distinguish target uniform encoding from proposed exact storage-layout validation.

## Impact

Compiler module internals, task ownership, third-party platform packaging, source/module identity and associated tests. No new renderer ownership, resource-group model or business variant API. Changes that affect artifact identity require explicit versioning and recook. Existing approved architecture still governs new target or loader changes.

## Related changes

refactor-aurora-target-shader-artifacts owns target ABI, parameter transactions and artifacts; refactor-aurora-shader-source-techniques owns typed V/P/S, complete Material keys and target lowering; refactor-process-owned-reflection owns unique process metadata definitions, and remove-module-code-leases now owns process-resident native code. This follow-up must preserve these contracts and the actual compiler/Slang-object ownership. Current tasks and native platform gaps remain open; this correction is not an archive or implementation claim.
