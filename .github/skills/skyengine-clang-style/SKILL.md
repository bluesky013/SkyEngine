---
name: skyengine-clang-style
description: Apply the repository's current clang-format and clang-tidy policy when writing, reviewing, or refactoring SkyEngine C++ code.
metadata:
  audience: contributors
  source: .clang-format, .clang-tidy
---

# SkyEngine Clang Style

Load this skill whenever a task depends on the repository's current `.clang-format` or `.clang-tidy` behavior.

## Goal

Translate the checked-in clang tooling configuration into explicit authoring and review rules, without inventing stricter policy than the repository currently enforces.

## Working rules

- Treat `.clang-format` and `.clang-tidy` at the repository root as the authoritative source of truth for C++ style and static-analysis policy.
- If an example in this document conflicts with the actual tool output, follow the tool configuration.
- Do **not** invent stricter rules that are not present in the current configuration.
- Use this skill together with `skyengine-coding-rules` when a task needs both general project conventions and current clang-derived policy.

---

## Rule 1 — Match the current `.clang-format` layout exactly

### Requirement

Write and edit C++ code so that it already matches the current repository `.clang-format` configuration before formatting is applied.

### Current formatting rules derived from `.clang-format`

- Use **4 spaces** per indent level.
- Keep lines at **150 columns or less** when practical.
- Indent namespace contents (`NamespaceIndentation: All`).
- Keep access specifiers aligned with the class indentation level rather than the member indentation level.
- Use **right-aligned pointers**: `Type *ptr`, `const Foo *value`, not `Type* ptr`.
- Do **not** compress function definitions into one line (`AllowShortFunctionsOnASingleLine: None`).
- Place a **function** opening brace on the **next line**.
- Keep **control-statement** braces on the **same line**: `if (...) {`, `for (...) {`, `while (...) {`.
- Keep `else` on the same line as the preceding closing brace: `} else {`.
- Put `catch` on the next line after the preceding closing brace.
- When parameter lists wrap, do **not** bin-pack them; prefer one parameter per line.
- Always break template declarations in the formatter-approved style instead of forcing compact one-line templates.
- Preserve existing include block grouping (`IncludeBlocks: Preserve`); do not merge unrelated include groups just for cleanup.
- Indent preprocessor directives before `#` within their scope (`IndentPPDirectives: BeforeHash`).

### Constructor initializer formatting

When constructor initializers wrap, format them in the current project style:

```cpp
Foo::Foo()
    : first(value)
    , second(other)
    , third(flag)
{
}
```

- Opening brace stays on its own line for the function body.
- Wrapped initializers use the current `BreakConstructorInitializers: BeforeComma` style.

### Alignment guidance

The formatter is configured to align consecutive declarations, assignments, macros, and operands. When editing an existing aligned block, preserve that style instead of introducing a mismatched layout.

---

## Rule 2 — Follow the current `.clang-tidy` policy, including what is intentionally *not* enforced

### Requirement

New and modified C++ code should be written to pass the repository's current `.clang-tidy` checks without relying on post-hoc cleanup.

### Enabled check families

The repository currently enables these families:

- `bugprone-*`
- `modernize-*`
- `performance-*`
- `portability-*`
- `readability-*`
- `mpi-*`

It also explicitly enables these focused checks:

- `google-default-arguments`
- `google-explicit-constructor`
- `google-runtime-int`
- `google-runtime-operator`
- `misc-misplaced-const`
- `misc-new-delete-overloads`
- `misc-no-recursion`
- `misc-non-copyable-objects`
- `misc-throw-by-value-catch-by-reference`
- `misc-unconventional-assign-operator`
- `misc-uniqueptr-reset-release`

### Practical coding rules derived from the enabled checks

- Mark converting constructors `explicit` unless an implicit conversion is truly intended.
- Avoid introducing new recursive code unless there is a strong, deliberate reason and the surrounding code already accepts it.
- Throw exceptions by value and catch them by reference.
- Avoid unusual assignment-operator behavior and unnecessary custom `new`/`delete` overloads.
- Do not write `ptr.reset(ptr.release())`-style ownership churn with `std::unique_ptr`.
- Prefer portable integer types where width matters instead of leaning on platform-sensitive integer spellings.
- In general, prefer modern, readable, performance-aware C++ constructs that satisfy `modernize`, `readability`, `performance`, `portability`, and `bugprone` checks.

### Important non-rules from the current configuration

The current `.clang-tidy` intentionally disables several checks. Do **not** invent project rules that contradict those decisions.

- `modernize-avoid-c-arrays` is disabled: raw arrays are **not** globally forbidden by tooling.
- `modernize-concat-nested-namespaces` is disabled: compact nested-namespace syntax is **not** required.
- `modernize-use-nodiscard` is disabled: `[[nodiscard]]` is **not** mandatory everywhere.
- `modernize-use-trailing-return-type` is disabled: trailing return types are **not** required.
- `modernize-use-default-member-init` is disabled: in-class default member initialization is **not** mandatory.
- `bugprone-easily-swappable-parameters` is disabled: parameter similarity is **not** specially policed by tooling.
- `bugprone-implicit-widening-of-multiplication-result` is disabled: no extra project rule should be invented beyond normal correctness review.
- `readability-implicit-bool-cast` is disabled: explicit `!= nullptr` / `!= 0` style is not mandated by tooling in every case.
- `readability-magic-numbers` is disabled: magic numbers are **not** globally banned.
- `readability-named-parameter` is disabled: comment-style named arguments at call sites are **not** required.
- `readability-uppercase-literal-suffix` is disabled: literal suffix casing is not a project-level rule.
- `readability-identifier-length` is disabled: there is no global minimum identifier length rule.

### Review behavior

When writing or reviewing code, treat `.clang-tidy` as the baseline policy:

1. Fix violations that are triggered by the currently enabled checks.
2. Do **not** request style churn for checks that are explicitly disabled.
3. If a pattern is controversial and the tool config is silent, follow nearby repository code instead of inventing a new global rule.
