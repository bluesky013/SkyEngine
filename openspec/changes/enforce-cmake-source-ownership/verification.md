# Verification — 2026-10-27

## Repository audit

Reviewed source declarations across engine, plugins, tools and CMake helpers, including inactive source-list branches. The final configured Windows graph contains 148 targets with 883 distinct translation units. The generated `cmake-build-debug/source-ownership.txt` records the actual owners.

| Area | Finding before this change | Resolution |
| --- | --- | --- |
| Jolt | 13 adaptor sources, mesh conversion, asset builder and codec compiled again in tests; codec also shared by two modules | Implementation static targets; one codec owner |
| Animation editor | Five implementation sources compiled again in EditorTest | Link Animation.Editor.Static; retain private includes |
| Sandbox | Three module implementation sources compiled again in SandboxProjectTest | Link SandboxModule.Static; factory stays in SandboxModule |
| Enet | Backend and module entry compiled again in EnetNetworkTest | Link backend implementation; remove unused module entry from test |
| Framework | Host test compiled AndroidInput.cpp directly while Android production owned it separately by platform glob | One Framework.AndroidInput target consumed by both configurations |
| Remaining shared sources | Test mains, loopback mock, DLL probes and static factory contract | Test-only fixture policy; exact AuroraRegistry compile-variant declaration |

The configured graph has no undeclared production source duplication. Aurora.Renderer.StaticModuleTest retains its intentionally different factory tag; the check validates source membership and distinct flags. No implementation directories moved and no new core layer was added.

## Passed checks

- Existing `cmake-build-debug` configured with Visual Studio 17 2022 and the existing `D:/Code/Engine/sky_3rd/Win32` dependency root.
- Built JoltPhysicsTest (and its three real module dependencies), Animation.EditorTest, Animation.ModuleTest, EnetNetwork, EnetNetworkTest, SandboxProjectTest, SandboxModule, FrameworkTest, Aurora.Renderer.ModuleTest and Aurora.Renderer.StaticModuleTest.
- Existing CTest regressions: **9/9 suites passed** — FrameworkTest, the two Aurora module suites, SandboxProjectTest, JoltPhysicsTest, JoltPhysicsModuleLifecycle, EnetNetworkTest, Animation.ModuleTest and Animation.EditorTest.
- `CMakeSourceOwnership`: **26 Python tests passed**, invoking actual positive/negative CMake configurations. Includes disabled production owners, deferred source addition, path normalization/case, explicit source language, interface sources, invalid/cyclic variants, generated code, old CMake GENERATED-property visibility and existing game-template registration.
- Existing static shipping registration regression passed for both shipping and development selections: `python -m unittest python.tests.test_native_package_artifacts.NativePackageArtifactTest.test_static_shipping_registration_omits_the_compiler_factory -v`.
- `openspec validate enforce-cmake-source-ownership --strict` and scoped `git diff --check` passed.

Logs remain under `build_3rd/source-ownership-*.log`; the independent guard job is `.github/workflows/build-contracts.yml`. The CI workflow is added but has not been executed remotely.

## Limits

Native Android/iOS/macOS SDK builds were not run. The audit inspected their CMake source lists; each activated configuration now runs the same guard. The guard checks target source ownership, not semantic dependency layering or C++ textual inclusion. The physical asset/streaming implementation has separate evidence in add-physics-collider-authoring/verification.md and benchmarks.md; it is included in the same checkpoint. No OpenSpec archive was performed for this change.
