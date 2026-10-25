## Status and scope — 2026-10-26

The original plan mixed compiler reuse with a second variant API. That variant proposal is withdrawn: typed V/P/S key/value selection, generated Material keys and target-specific lowering are already the confirmed contract. This document describes only the remaining compiler/storage work and its integration constraints. Historical Windows upgrade/linkage results remain in tasks.md; they do not prove worker-pool or other-platform completion.

## Compiler module ownership

Aurora.ShaderCompiler remains the production Slang owner, statically linking Slang inside its existing module boundary. Desktop/Android shared and iOS static deployment must follow the existing target configuration and platform evidence. Runtime consumers use IShaderCompiler/ShaderCompilerFactory; private compiler implementation linkage remains limited to tests/offline tools. Windows DXC/DXIL deployment stays with the compiler module.

The previous teardown problem was addressed by the recorded upgrade and removal of session.detach. Do not treat static linkage, a module boundary or leaked sessions as a substitute for lifecycle correctness. Preserve session-owned borrowed Slang module pointers and stable sourcePath storage. Platform packages and repeated teardown still require their own checks.

## Worker and session plan

Each worker owns its SlangCompilerContext, global session, bounded session pool and module cache. Slang objects are not shared concurrently across workers; serialized artifacts may be shared. Use the existing ShaderCompileTask/ShaderCompileFuture/ThreadPool infrastructure with a dedicated compiler pool, keeping renderer workers independent. Bound worker count and record per-worker memory costs.

Session identity includes target/platform/environment/options, search paths, immutable filesystem/config identity and platform macros. The planned source-prelude path moves supported shader-level compile values into a canonical prelude and gives each resulting module a distinct identity. Session reuse must not imply module reuse across different preludes. Retain logical source-symbol associations and target-specific lowering; do not perform arbitrary textual declaration rewrites.

Validate DXC/downstream concurrency independently. If required, use worker-local instances or a narrowly scoped backend mutex. Shared library initialization may use an explicit thread-safe lifecycle; this must never become a guard around reflection registration. Reflection remains uniquely owned by process definitions.

Accepted work retains source/module/compiler/cache ownership until completion or collected cancellation. Resolving a worker context must not discard the previously accepted compiler code lease. Shutdown stops new work, gathers tasks, then releases modules, sessions and global sessions in their valid ownership order. Pools must support bounded eviction or explicit cleanup without relying on process exit.

## Variant contract to preserve

Callers provide typed bool/int32/uint32 values in Vertex, Pipeline and Shader-local domains. Mesh supplies Vertex; Material/custom Batch/fullscreen/compute supply S. Unknown, duplicate, wrong-owner, wrong-type and out-of-range values fail; defaults normalize consistently. Full identities are not limited to a 128-bit packed helper.

Stage projection retains each stage's dependencies. Supported Vulkan specialization and Metal function constants retain independent stage/key/type/native-ID/value mappings. DXIL's supported compile path consumes the same selected values. The compiler, source declarations and backend support determine lowering; business inputs never select macro/native policy. There is no caller category or DX12 root-constant/uniform variant fallback.

Complete selection, stage-code identity and native executable identity remain distinct. Native choices may share code only when their validated ABI/resource/I-O/workgroup contract permits it. Full selected values remain in published selection metadata and executable identity even when omitted from reusable code identity.

## Uniform and storage layout

Cbuffer uses the selected target's native reflection and the existing UniformUploadPlan. CPU offsets, sizes, strides and padding need not match the GPU. Keep semantic validation, padding zeroing, atomic parameter publication and exact resource lifetime. Shared-header checks compare encoded target values, not raw CPU/member equality.

Generic SSBO canonical-schema validation is remaining work, separate from uniform conversion. The proposed constrained schema uses fixed-width values, explicit matrix/array representation and 16-byte fields/padding where required; unsupported shapes fail. Raw ByteAddressBuffer offsets come from a shared definition. Validate element size/stride/alignment/fields independently for each target and use CPU→GPU→CPU readback. Do not globally alter target layout flags or claim that every storage shape is implemented because the current FLOAT4 palette succeeds.

## Precompiled modules, cache and products

Precompiled .slang-module is a proposed compiler input shared across compatible entries, not a new per-material asset. Cache identity includes producer/module compatibility, target environment, source closure and code-affecting lowering. Module freshness must be checked; incompatible inputs rebuild in DEVELOPMENT and never trigger source fallback in PRODUCTS.

Usage records and cook requests preserve complete typed selections. Product distributions retain allowed selections, stage-specific mappings and immutable artifacts. No implicit Cartesian expansion, hash-only validation or default substitution is introduced. Existing single-writer/atomic file publication and owned async endpoints remain authoritative.

## Verification gates

- Windows: compiler teardown/concurrency, both Vulkan raster paths, DXIL selected-value execution, uniforms/storage and compiler-free products.
- macOS/iOS: native Metal/function constants, module deployment, memory, uniforms/storage and stripped products; cross compilation alone is insufficient.
- Android/Linux: actual Vulkan environment, storage/readback, products and compiler lifecycle; record unavailable prerequisites explicitly.
- Determinism: identical artifacts for equivalent inputs across worker counts/orders; distinct identities for changed inputs; bounded context/session/module memory.
- Regression: existing four resource groups, generated Material keys, unique metadata owners and callback/module retirement remain unchanged.

The 61 task rows retain their checked states. Revised variant rows are integration/regression work for the future pool, not a second implementation of the current selector. No commit or archive is authorized by this design update.
