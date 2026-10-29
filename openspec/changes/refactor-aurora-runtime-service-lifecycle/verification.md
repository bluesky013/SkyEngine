# Verification — 2026-10-30

## Implementation review

- `AuroraModule.h/.cpp` contain no Skeleton source type, member, factory, interface access or lifecycle operations.
- The existing `AuroraRegistry.cpp` now registers a runtime assembly entry. It owns the source and delegates renderer Start/Tick without changing module names or load dependencies.
- Init acquires the existing defining-module lease, rejects a conflicting Skeleton registration owner, and rolls back its source if renderer initialization fails.
- Shutdown withdraws only its own source interface, drains RendererHost, then releases the assembly's source. Request/result code and asset leases retain their existing behavior.
- Repeated Init/Shutdown, destructor cleanup, provider-only use, dynamic factory loading and the existing tagged static variant preserve ownership.
- Source implementation and asset/metadata owners remain unchanged. No CMake, target or production compile owner changes were made.
- A separate read-only lifecycle review found no blocking ownership or teardown issue.

## Build

Built the real `Aurora.Adaptor.Static` and `Aurora.Render` targets, plus `Aurora.AdaptorTest`, `Aurora.Renderer.ModuleTest`, `Aurora.Renderer.StaticModuleTest` and `Animation.ModuleTest` in the existing VS 2022 Debug cache. The animation target also rebuilt its actual runtime/editor dependency modules.

The existing generator and `3RD_PATH=D:/Code/Engine/sky_3rd/Win32` were retained. Builds and native/module tests used normal process permissions because the restricted environment previously failed PDB-server and temporary product publication operations.

## Regression results

| Selection | Passed | Skipped | Failed |
| --- | ---: | ---: | ---: |
| Adaptor Host/source/lifecycle selection | 18 | 0 | 0 |
| Dynamic Renderer module suite | 8 | 2 | 0 |
| Tagged static Renderer module suite | 9 | 2 | 0 |
| Production Animation module suite | 2 | 0 | 0 |
| CMake source-ownership Python suite | 26 | 0 | 0 |

The four skips are the two shipped-product rendering cases instantiated in both module suites. Their existing condition requires `python/tests/run_shader_package_tests.py` to supply stripped products; the packaging workflow was not changed here.

New direct-adapter cases prove provider-only and host initialization need neither Skeleton service nor ModuleManager, preserve an external source during successful shutdown, and preserve it after host creation failure. The existing Launcher scene-pass fixture still retains a ModuleManager because its Feature code lease is unrelated to Skeleton registration.

New module cases prove provider-only source publication without ISystemNotify, fallback to the current loader, interface withdrawal on unload, request/result lease safety after manager destruction, and rollback when an external renderer provider prevents initialization. The static duplicate-owner case also checks source identity through repeated Init and duplicate/double Shutdown. Production Animation tests verify actual consumer load order and teardown.

## Static checks and evidence

- `clang-format --dry-run --Werror` passed on all five changed C++ files.
- `git diff --check` passed for the engine changes.
- Strict OpenSpec validation passed.
- No throw, catch or RTTI downcast was added.
- Existing staged changes to `docs/README.md` and `engine/framework/test/assets/assets.db` were preserved.

Local logs and XML reports are under `build/aurora-runtime-service-review/`. The concrete commit plan is maintained in `build/renderer-runtime-review/commit-plan.md`. No commit or archive was performed.

The user's process-resident DLL preference is recorded in design.md. Existing lease compatibility tests verify the preserved API; they do not introduce a plugin hot-reload requirement.
