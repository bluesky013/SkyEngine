> Status — 2026-11-10: implemented，macOS 动态构建与原生回归已验证；独立全静态打包、全新构建目录及专项 IPC 验收仍未执行。未 commit/archive。
> 当前压缩能力已由 Framework 内置 LZ4 提供；通用字节 IO 调度另属 `add-common-async-io`，不在此重复建设。

## 1. Design-only preparation

- [x] 1.1 Inspect AssetTool, Game, Editor and legacy tool bootstrap paths; identify duplicated parsing and stale missing-DLL comments.
- [x] 1.2 Record the existing initialization order, ModuleInfo/buildOption reuse and build/deployment/static-activation boundaries.
- [x] 1.3 Create a separate proposal, design and capability deltas; record the user's instruction to defer implementation.

## 2. Shared manifest parsing

- [x] 2.1 Add a small Framework parser using existing ModuleInfo and build-option lookup, explicit diagnostics and bounded input validation.
- [x] 2.2 Validate complete selections and dependency conflicts/cycles before publishing registrations; preserve implicit metadata dependency nodes.
- [x] 2.3 Replace Game/Editor module-array parsing while preserving host path precedence, project modules, game fields and fallback behavior.
- [x] 2.4 Test malformed fields, duplicates, unknown/disabled options, dependency conflicts and transactional failure.

## 3. AssetTool configuration

- [x] 3.1 Inventory actual builder targets/dependencies/options; add complete cook and minimal audit manifests using the current schema.
- [x] 3.2 Select manifests before PreInit without source mounts; remove hardcoded module names and SKY_HAS_*_BUILDER macros.
- [x] 3.3 Keep audit isolated from project source configuration, builder activation and cook; correct stale missing-DLL comments.
- [ ] 3.4 Verify Game/Editor compatibility, worker capability parity and IPC startup/failure completion.

## 4. Build and deployment

- [x] 4.1 Bind AssetTool build-only target dependencies and configuration tracking from the same manifest selections after module targets exist.
- [x] 4.2 Deploy both manifests and the required runtime dependency closure without relying on a prior SandboxEditor build.
- [x] 4.3 Scope existing static factory generation/activation to the selected host manifests; distinguish linked factories from active modules.
- [x] 4.4 Verify disabled-feature combinations, missing enabled targets/modules and reconfiguration after manifest edits.

## 5. Acceptance

- [ ] 5.1 Build AssetTool alone from a fresh verification layout and run native cook plus source-free product auditing with explicit failure cases.
- [ ] 5.2 Verify static audit does not initialize or register builder/editor/runtime metadata; record actual platform coverage.
- [x] 5.3 Run affected Framework/editor/package regressions and record commands, results and remaining limitations.
- [x] 5.4 Review the implementation, update specs/tasks and run strict OpenSpec validation before requesting archive/commit confirmation.

Status: implementation authorized on 2026-11-10. Shared parsing, configuration/deployment wiring and native macOS dynamic acceptance are recorded in verification.md. Tasks 3.4, 5.1 and 5.2 retain their broader IPC/fresh-build/static-package acceptance scope; completed native cook, audit and editor smoke results are not treated as those unexecuted scenarios. Development static cook retains explicitly documented legacy dynamic modules; no Aurora entry point was migrated.
