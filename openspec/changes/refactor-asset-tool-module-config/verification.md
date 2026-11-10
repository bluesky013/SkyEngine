# Verification — 2026-11-10

Implemented shared Framework parsing, host manifests, lazy static factories, explicit required/optional dependency semantics, Particle builder development gating, and local PUBLIC/PRIVATE link scopes.

- `python3 python/tests/test_cmake_source_ownership.py`: 26 tests, 25 passed, 1 Windows-only test skipped on macOS.
- `python3 python/tests/test_module_configuration.py`: temporary real-CMake configure fixtures cover shipping/test/development Particle targets, link usage propagation, shared C++/CMake manifest cases, missing enabled targets, and hybrid factory collection/config deployment. 5 test methods passed, including 17 shared manifest cases.
- `git diff --check`: passed at the implementation checkpoint.
- `openspec validate refactor-asset-tool-module-config --strict --no-interactive`: passed (exit 0); optional telemetry flush failed because network DNS is unavailable.
- Framework/native/editor/tool builds and runtime tests are coordinated by the root agent; results are not inferred from the configuration fixtures.

Static availability regression exercises dormant factories, dependency activation, restart, null-factory failure/retry, and preserves process-once metadata. The game template still links available factories; activation now follows the selected module graph. AssetTool collects only the real static subset of its selected closures and retains existing SHARED cook modules as dynamic dependencies. This is hybrid development tooling, not a claim of fully static cook validation.

Final review added explicit generated-registration failure propagation and immediate Application cleanup on registration/topology failure. The Particle/static inventory and AssetTool hybrid configuration regressions passed again (2 methods). `ApplicationShutdownTest.CyclicModulesReleasePreInitServicesAndDrainCallbacksBeforeReturning` checks that failed initialization revokes PreInit services and drains pending callbacks before returning; its native rerun is coordinated separately below.

Only the SandboxModule to RecastNavigation.Editor edge became optional. JoltPhysicsModule and JoltPhysicsRenderModule gained their existing SKY_BUILD_JOLT gate in the editor manifest; no new backend or builder was enabled.

## Native integration results

- Existing macOS Xcode Debug build passed for Framework, SandboxModule/SandboxEditor and AssetTool; implementation modules and their tests were built as real targets.
- FrameworkTest: 288 passed, 22 opt-in Cocoa tests skipped. Aurora.CoreTest: 128 passed with system graphics access. EditorShellTest: 149 passed. Additional asset/editor/UI regressions are recorded in `add-curve-asset/verification.md`.
- AssetTool package integration: 21 expected-success/expected-failure cases passed, including real Jolt World cook and source-free product auditing. `/tmp/sky-review-assettool/asset-tool-batch-integration.json` records commands/results.
- Isolated Curve and manifest startup matrix: 7 cases passed; missing/bad/unknown-option/unavailable-module configurations fail rather than using a fallback. Source-free audit activates no modules. `/tmp/sky-curve-native-review-boo3_1wv/results.json` records the evidence.
- Bounded SandboxEditor startup opened the existing Curve sample and exited normally after 60 frames.

These results use the current dynamic macOS build. No fresh full static native package or Windows build was performed; configuration fixtures and ModuleManager native tests do not substitute for those platform runs. Third-party preflight reported 25 stale fingerprints with all outputs present; existing outputs were preserved.

Final startup-failure patch: incremental FrameworkTest/SandboxEditor/AssetTool build passed; the full FrameworkTest rerun passed 288 cases with 22 opt-in Cocoa skips. The cycle-failure case confirms PreInit service cleanup and callback draining before failure returns. The final full configuration suite passed 5/5 after generated static registration began propagating failure.
