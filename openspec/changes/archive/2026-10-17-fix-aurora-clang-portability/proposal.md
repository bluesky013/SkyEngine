## Why

After rebasing onto `d956f619`, the Aurora renderer fails to compile with AppleClang. The upstream commit was verified on the Windows/MSVC host, which accepts member-template calls without a `template` disambiguator in dependent contexts. Clang enforces the standard two-phase lookup and rejects:

- `row.Read<Ts>(...)` in `FrozenCpuFrame.h`
- `scene.registry.Add<T>(...)` in `CpuSceneStore.cpp`

The branch can be pushed but cannot be built or tested on macOS until these are fixed.

## What Changes

- Resolve the dependent member-template calls in the Aurora renderer so the sources compile under Clang/GCC.
- Prefer explicit non-dependent local types where they remove the ambiguity cleanly; add the `template` disambiguator where a dependent call remains.
- Build the full project on macOS and run the complete test suite, not just the affected files.
- Record any further Clang-only issues found after the first two errors are fixed.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-renderer`: adds the requirement that renderer sources build with Clang/GCC, not only MSVC.

## Impact

- `engine/aurora/core/src/renderer/scene/FrozenCpuFrame.h`
- `engine/aurora/core/src/renderer/scene/CpuSceneStore.cpp`
- Possibly further Aurora renderer sources if the full build reveals more dependent-template calls.
- No runtime behavior change; this is a compile-portability fix.
