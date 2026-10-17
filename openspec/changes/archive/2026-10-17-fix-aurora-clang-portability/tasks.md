## 1. Fix dependent template calls

- [x] 1.1 In `FrozenCpuFrame.h`, make the read-view and write-view row locals explicitly `FrozenEntityRow` (or add the required `template` disambiguator) so `Read<Ts>` parses under Clang.
- [x] 1.2 In `CpuSceneStore.cpp`, make the scene locals non-dependent (or add `template`) so `Pool<T>`, `Add<T>`, and `Get<T>` parse under Clang.
- [x] 1.3 Build `Aurora.Core.Static` and fix any remaining dependent-template diagnostics in the renderer.
- [x] 1.4 Build the full project and repeat until the build is clean.
- [x] 1.5 Remove the obsolete `RenderDeviceExclusive.h` include from `EditorFrameSubmission.h` and include the concrete `DeviceFrameContext`/`Fence` headers it now needs.
- [x] 1.6 Update `EditorFrameSubmissionTest`'s frame-context mock to the new pure-virtual `DeviceFrameContext` API and restore fence reset on `BeginFrame`.

## 2. Verify

- [x] 2.1 Run the full `ctest` suite on macOS and record results.
- [x] 2.2 Confirm the affected files have no behavior changes beyond type spelling / `template` disambiguation.
- [x] 2.3 Run `openspec validate fix-aurora-clang-portability --strict --no-interactive` and `git diff --check`.
- [x] 2.4 Record verification results in `tasks.md`.

## Verification record

- Replaced `auto` locals with explicit non-dependent types in `FrozenCpuFrame.h` and `CpuSceneStore.cpp`; Clang then parsed `Read<T>` and `Add<T>/Get<T>/Pool<T>` without `template` noise.
- Removed the stale `aurora/rdg/RenderDeviceExclusive.h` include from `EditorFrameSubmission.h` (the header was deleted upstream) and added the concrete `DeviceFrameContext`/`Fence` includes it needs.
- Reworked `EditorFrameSubmissionTest`'s `TestFrameContext` to implement the new pure-virtual context API and reset the fence in `BeginFrame`.
- Full `cmake --build build --parallel` succeeds on macOS/AppleClang.
- Serial `ctest`: 47/49 passed. `Aurora.Renderer.ModuleTest` and `Aurora.Renderer.StaticModuleTest` fail in the upstream Metal frame path with `Renderer frame operation failed (frame=0, native=0)`; they are not touched by this change and belong to the pending Aurora platform-parity work.
- `EditorRenderTest` 6/6 and `EnetNetworkTest` 2/2 pass; `FrameworkTest` passes serially (the earlier parallel-run failure did not reproduce).
- `openspec validate fix-aurora-clang-portability --strict --no-interactive` and `git diff --check` passed.
