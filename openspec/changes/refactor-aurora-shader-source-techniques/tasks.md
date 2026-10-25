# Implementation tasks

Status: implemented with Windows automatic validation on 2026-10-25; macOS native acceptance remains open. These tasks replace the withdrawn source-technique migration. Check a task only after its implementation and relevant verification are complete.

## 1. Contract and compiler gate

- [x] 1.1 Reconcile proposal/design/spec deltas with the confirmed JSON V/P/S and material-key design; retain rollback history and current format baseline.
- [x] 1.2 Prove key-to-source-symbol reflection, per-stage native ID/type/default extraction, unused declarations and equivalent DXIL compile values on the pinned compiler.
- [x] 1.3 Cover conflicting symbols, unsupported scalar/default/shape and non-native fallback diagnostics without arbitrary source rewriting.

## 2. Common selection and target metadata

- [x] 2.1 Implement Core-owned typed V/P/S selection, duplicate/domain/range/default normalization and stable full identity; migrate Pipeline assembler to the common contract.
- [x] 2.2 Separate per-stage target lowering/native mappings from logical schema; preserve explicit stage programs and JSON symbol associations.
- [x] 2.3 Separate SelectionKey, StageCodeKey and executable identity; preserve native code sharing, full-key validation and stage-specific projection.
- [x] 2.4 Update versioned artifact/schema/cache/usage codecs and source-free metadata with bounded transactional validation.

## 3. Native execution

- [x] 3.1 Carry stage/key/type/native-ID/value through technique preparation and device creation; eliminate program-global ID merging.
- [x] 3.2 Apply independent Vulkan stage maps and Metal function values; keep DXIL selected values in code and no unintended uniform parameters.
- [x] 3.3 Validate native ABI/shape contracts and conservative required resources; test same ID with different stage values/types and executable isolation.

## 4. Material schema and bindings

- [x] 4.1 Generate complete logical Material keys from stable declarations plus S options; validate cross-technique types and zero/one Group 2 cbuffer.
- [x] 4.2 Migrate values/aliases with explicit diagnostics; preserve inactive values, reject unknown/type-invalid keys and require active values/resources.
- [x] 4.3 Reuse target uniform transactions/packing and resource lifetimes; keep Material and Pass/Batch ownership distinct.
- [x] 4.4 Update material/instance serialization and generic reflected editor behavior; retain value-only MaterialInstance overrides.

## 5. Production preparation and cook

- [x] 5.1 Route Scene, Batch, Fullscreen and Compute through the same normalized selection with explicit V/P/S input ownership.
- [x] 5.2 Prepare immutable complete versions outside graph recording, key reusable results correctly and preserve pending/error/revision/GPU retirement behavior.
- [x] 5.3 Collect explicit cook/usage requests without automatic Cartesian expansion; publish and validate complete allowed selections including native values.
- [x] 5.4 Update built-ins, module requirements and product fixtures while preserving companions, explicit entries and shared exports.

## 6. Verification and documentation

- [x] 6.1 Run affected Core/Pipeline/Adaptor/Cook/Shader/RHI tests and shared declaration checks; update obsolete inactive/identity assertions.
- [x] 6.2 Verify native Vulkan/DX12 graphics and compute readback, stage isolation and code/executable reuse behavior on Windows.
- [x] 6.3 Verify relocated compiler-free Windows products with nondefault scene/fullscreen/compute selections and missing-selection rejection.
- [ ] 6.4 Run native Metal execution and stripped-product tests on macOS; keep unchecked until that platform is available and executed.
- [x] 6.5 Synchronize actual implemented docs/domain rules and record commands, results and remaining platform gates; validate OpenSpec and diff hygiene. No automatic commit/archive.

Evidence and manual UI acceptance limits: see implementation.md. No commit/archive was performed.


## 2026-10-26 规范接续复核

已逐项修正相关主 spec 与 active delta 的现行契约，并将已存在的 ADDED/RENAMED/REMOVED 操作重设为可再次合并的 delta。只读合并检查与 strict 校验分开记录；未执行 archive。历史平台结果不覆盖后续四组/typed selection/metadata owner 修改。完整范围、证据和剩余项见 `docs/aurora/consistency-review-2026-10-26.md`。
