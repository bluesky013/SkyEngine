## Context

AppleClang rejects two Aurora renderer constructs introduced by upstream `d956f619`:

- `FrozenCpuFrame.h:108` and `:198`: `row.Read<Ts>(...)` / `row.Read<Reads>(...)` where the `FrozenReadView`/`FrozenWriteView` class templates make the call look dependent.
- `CpuSceneStore.cpp:266-271`: `scene.registry.Add<T>(...)` inside a `std::visit` generic lambda; `scene` is deduced with `auto &`.

MSVC accepts these without the `template` keyword; Clang and GCC require it when the object expression is treated as dependent. The errors are purely compile-time and do not indicate a renderer logic problem.

## Goals / Non-Goals

**Goals:**

- Restore a clean full build on macOS/AppleClang.
- Keep the fix minimal and behavior-preserving.
- Cover the whole Aurora renderer, not only the first reported lines.

**Non-Goals:**

- No renderer behavior, API, or performance change.
- No MSVC-specific workaround or compiler-flag relaxation.
- No broad reformatting of the affected files.

## Decisions

### 1. Use explicit non-dependent local types where sufficient

For the row iteration, declare `const FrozenEntityRow &row` instead of `const auto &row`. For the scene command path, declare `CpuSceneState &scene` (and the created scene pointer with its concrete type) instead of `auto`. This removes the dependent classification at the source and avoids sprinkling `template` through the call sites.

### 2. Add `template` where the call remains dependent

If a call still sits in a genuinely dependent context after the type cleanup, write `row.template Read<Ts>(...)` or `scene.registry.template Add<T>(...)`. The standard disambiguator is preferred over casts or helper wrappers.

### 3. Build iteratively and fix every Clang error

The first compile stops after a subset of errors. Build `Aurora.Core.Static`, then the full project, and address each remaining dependent-template diagnostic until `ninja` completes.

## Risks / Trade-offs

- **[More Clang-only issues appear after the first fix]** → Iterate with full target builds and record each fix.
- **[Explicit types diverge from local `auto` style]** → Limit changes to the lines that need non-dependence; keep the rest of the file untouched.
- **[Windows behavior could change]** → The edits are syntax-only; run the full macOS suite and keep the changes behavior-preserving.

## Migration Plan

1. Fix the two known headers/sources.
2. Rebuild `Aurora.Core.Static`, then the full project.
3. Run the full test suite and validate the change.
4. Archive the change and commit with the portability fix.

Rollback is a straight revert; no persisted data or public interface is involved.

## Open Questions

None.
