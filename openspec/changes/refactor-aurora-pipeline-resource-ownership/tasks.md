## 1. Review and ownership

- [ ] 1.1. Inventory existing CMake deployment/package helpers, plugin resource metadata and host pipeline registration; choose the smallest integration seams.
- [ ] 1.2. Present and confirm the exact generic binding helper ownership/include migration and host assembly changes before implementation; keep existing Aurora targets.

## 2. Shared declarations and bindings

- [ ] 2.1. Add owner-selectable C++ namespaces to shared declarations and migrate built-in declarations without duplicating fields or binding numbers.
- [ ] 2.2. Make generic Pass binding validation usable without the built-in pipeline library; preserve transactional apply, explicit dynamic policy and frame lifetime.
- [ ] 2.3. Verify two plugin fixtures can use identical local Pass/type names, including optional View and independent Global/Batch layouts, without shader codegen for C++.

## 3. Owner resources and compilation

- [ ] 3.1. Reuse the CMake resource workflow to emit per-owner bundle descriptions from one source list, preserving source/package ownership and declared dependencies.
- [ ] 3.2. Extend existing shader filesystem resolution with qualified paths, deterministic dependency lookup and atomic collision/error handling.
- [ ] 3.3. Feed the same bundle graph to runtime and existing ShaderCookTool without loading pipeline runtime modules during cook.
- [ ] 3.4. Keep include resolution and source-closure/cache identity consistent; migrate built-in ShaderRef/companion/usage paths and remove obsolete registration assumptions.

## 4. Pipeline assembly and lifetime

- [ ] 4.1. Assemble built-in or user pipelines explicitly through existing host feature commands, preserving the single producer/Renderer/Device contract.
- [ ] 4.2. Retain immutable source/dependency snapshots for compilation and reuse module/resource leases through feature retirement, failure and discard.
- [ ] 4.3. Verify source retirement during compilation and feature retirement with queued/GPU work; no implicit built-in fallback or parallel plugin submission loop.

## 5. Acceptance and documentation

- [ ] 5.1. Validate two external-style plugin fixtures, duplicate-path rollback, missing/undeclared dependencies and runtime/cook equivalence across source/package/embedded representations.
- [ ] 5.2. Verify shared dependency edits invalidate both build inputs and shader cache results; inspect the external pipeline build graph for unwanted built-in/compiler dependencies.
- [ ] 5.3. Run affected Core/Pipeline/Shader and renderer GPU regressions; report SPIR-V/DXIL/MSL reflection separately from native Metal execution.
- [ ] 5.4. Update authoring/deployment/migration docs, remove superseded integration paths, and pass scoped formatting/static analysis/diff checks and strict OpenSpec validation.

## Draft status

2026-10-21: proposal/design/spec deltas prepared; all implementation and acceptance tasks remain open. This change is the follow-up to the completed built-in shared-header checkpoint, not an extension of its completed task list.
