---
name: skyengine-coding-rules
description: Enforce SkyEngine C++ coding rules when writing or reviewing engine code. Covers prohibited patterns, preferred idioms, and type safety conventions.
compatibility: opencode
metadata:
  audience: contributors
  source: project conventions
---

# SkyEngine Coding Rules

Load this skill whenever writing, reviewing, or refactoring C++ code in this repository.

## Goal

Enforce project-wide coding conventions so that AI-generated code matches the standards expected by human reviewers.

## Working rules

- Apply these rules to ALL new and modified C++ code (`.h`, `.cpp`, `.inl`).
- When refactoring existing code that violates a rule, fix the violation only if it is within the scope of the current task. Do not silently expand scope.
- If a rule conflicts with an explicit user instruction, follow the user and note the deviation.

---

## Rule 1 — No `dynamic_cast`

### Prohibition

Do **NOT** use `dynamic_cast` in any new or modified code.

### Rationale

- `dynamic_cast` requires RTTI, which increases binary size and degrades performance.
- SkyEngine builds may disable RTTI (`-fno-rtti` / `/GR-`), making `dynamic_cast` undefined behavior or a linker error.
- The engine already provides its own type identification and casting mechanisms where safe down-casts are needed.

### What to do instead

| Scenario | Preferred approach |
|---|---|
| Down-cast when the concrete type is **known** at the call site | `static_cast<Derived*>(base)` |
| Down-cast when the type must be **checked** at runtime | Use the engine's own RTTI / type-id system (e.g., type enums, `GetTypeId()`, or template-based safe casts provided by the framework) |
| Branching on multiple subtypes | Prefer the visitor pattern or virtual dispatch |

### Detection

If you encounter existing `dynamic_cast` during a task, flag it in your response but do **not** remove it unless the current task explicitly covers that code path.

---

## Rule 2 — Explicit `const` on `auto` variables

### Requirement

When using `auto` to receive a return value that should be **const**, always write `const` explicitly. Do **NOT** rely on the return type to propagate const implicitly through `auto`.

### Forms

```cpp
// Good — const intent is visible at the declaration site
const auto  value = GetConfig();        // const by-value
const auto& ref   = GetConfig();        // const lvalue reference
const auto* ptr   = GetConfigPtr();     // pointer-to-const

// Bad — reader must look up GetConfig() to know whether value is const
auto value = GetConfig();               // const-ness hidden
auto& ref  = GetConfig();              // const-ness hidden behind return type
```

### Rationale

- `auto` strips top-level const from value types. A function returning `const T` becomes a mutable `T` when received by plain `auto`. Explicit `const auto` preserves the developer's intent.
- For references (`auto&`), the underlying const-ness is preserved by the language, but writing `const auto&` makes the **intent** immediately obvious without requiring the reader to inspect the callee signature.
- Consistent `const auto` / `const auto&` usage improves readability and prevents accidental mutation.

### Rule summary

| Declaration | When to use |
|---|---|
| `const auto x = ...` | Value should not be modified after initialization |
| `const auto& x = ...` | Binding to an existing object without copying; no mutation intended |
| `const auto* x = ...` | Pointer to data that should not be modified through this pointer |
| `auto x = ...` | Value **will** be modified later (mutable on purpose) |
| `auto& x = ...` | Reference **will** be used to mutate the referred object |

### Detection

When reviewing or writing code, if a variable received via `auto` is never modified after initialization, prefer `const auto` (or `const auto&` / `const auto*`).

---

<!-- Future rules go here as ## Rule N sections -->
