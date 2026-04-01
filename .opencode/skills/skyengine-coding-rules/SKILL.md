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

## Related skill

- Load `skyengine-clang-style` alongside this skill when the task depends on the repository's current `.clang-format` or `.clang-tidy` behavior.

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

## Rule 3 — Prefer forward declarations over `#include`

### Requirement

In **header files** (`.h` / `.inl`), prefer forward-declaring a type over including its full definition whenever the full definition is not required.

### When a forward declaration is sufficient

A full `#include` is **not needed** when the header only uses a type in these ways:

| Usage in header | Forward declare? |
|---|---|
| Pointer or reference (`T*`, `T&`, `const T&`) | Yes |
| Return type of a declared (not defined) function | Yes |
| Parameter type of a declared (not defined) function | Yes |
| `std::unique_ptr<T>`, `std::shared_ptr<T>` member (destructor not in header) | Yes |
| Template type argument that the template does not instantiate | Yes |

### When `#include` is required

| Usage in header | Must include? |
|---|---|
| Base class (`class Foo : public Bar`) | Yes |
| Member variable by value (`Bar m_bar;`) | Yes |
| `sizeof(T)` or accessing members of `T` | Yes |
| Inline / template function body that calls `T` methods | Yes |
| `static_cast` / `reinterpret_cast` to `T` (needs complete type) | Yes |

### How to apply

```cpp
// Good — header only uses pointer; forward declare
// Foo.h
namespace sky {
class Bar;          // forward declaration

class Foo {
public:
    void Process(Bar* bar);
private:
    Bar* m_bar = nullptr;
};
} // namespace sky

// Foo.cpp — include the full definition where it is actually needed
#include "Bar.h"

void Foo::Process(Bar* bar) {
    bar->DoWork();  // full definition required here, and we have the #include
}
```

```cpp
// Bad — unnecessary include in header
// Foo.h
#include "Bar.h"   // ← full definition not needed here

class Foo {
public:
    void Process(Bar* bar);
private:
    Bar* m_bar = nullptr;
};
```

### Rationale

- Reduces header coupling and transitive include chains, improving compile times.
- Makes dependency relationships explicit: the `.cpp` that actually uses the type includes the header.
- Minimizes recompilation cascades when an included header changes.

### Guidelines

1. **Headers**: Always ask "do I need the full definition here?" before writing `#include`. If not, forward declare.
2. **Source files (`.cpp`)**: Include whatever is needed; forward declarations in `.cpp` files are rarely useful and can be confusing.
3. **Do not forward declare** standard library types (e.g., `std::vector`, `std::string`) — always `#include` their headers.
4. **Nested / inner classes** cannot be forward declared from outside their enclosing class — `#include` is required.

### Detection

When adding a new `#include` to a header, verify that the included type is used in a way that requires its full definition. If only pointers, references, or declarations are involved, replace the `#include` with a forward declaration.

---

## Rule 4 — C/C++ source files must be ASCII-only

### Requirement

All new and modified C/C++ source files must contain **ASCII characters only**, including:

- code
- comments
- string literals
- character literals
- preprocessor directives

This rule applies to `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, and `.inl` files.

### Prohibition

Do **NOT** place non-ASCII characters in C/C++ source files, even when they appear only in comments or disabled code.

Examples of forbidden content include:

- Chinese, Japanese, or Korean characters in comments
- full-width punctuation such as `，` `。` `：` `；` `（` `）`
- smart quotes such as `“` `”` `‘` `’`
- em dashes / en dashes such as `—` `–`
- ellipsis `…`
- non-breaking spaces, full-width spaces, zero-width characters, or other invisible Unicode characters
- Unicode math symbols such as `×` `÷` `−`

### Rationale

- Source portability is more predictable when files stay within the basic source character set expected by C/C++ toolchains.
- Non-ASCII characters can be mis-decoded on different compilers, shells, terminals, editors, or Windows code pages.
- Visually similar Unicode characters can hide mistakes that are hard to spot during review.
- Invisible Unicode whitespace and control characters can introduce confusing parse, copy/paste, and diff problems.

### What to do instead

| If you want to write | Use this instead |
|---|---|
| `，` `。` `：` `；` | `,` `.` `:` `;` |
| `（` `）` `【` `】` | `(` `)` `[` `]` |
| `“` `”` `‘` `’` | `"` `'` or plain ASCII quotes |
| `—` `–` | `--` or `-` |
| `…` | `...` |
| `×` `÷` `−` | `*` `/` `-` |
| `　` or non-breaking space | plain ASCII space |
| localized text inside comments | English ASCII comments |
| non-ASCII user-facing text in source | move the text to assets/resources, or use explicit escaped byte/code-point sequences only when unavoidable |

### Prevention

1. Configure your editor to save C/C++ files as UTF-8 **without BOM**, but keep the file contents ASCII-only.
2. Disable smart punctuation / smart quotes for code editing.
3. When using an IME, switch back to ASCII input before editing code or comments.
4. Be careful with copy/paste from documents, chat tools, browsers, and issue trackers; they often introduce full-width punctuation or invisible characters.
5. Before submitting, scan changed C/C++ files for non-ASCII characters with your preferred search or validation tool.

### Detection

If you encounter existing non-ASCII characters in C/C++ source files during a task, replace them with ASCII-safe equivalents only when that file is already in scope for the current change. Otherwise, flag the issue in your response instead of silently expanding scope.

---

## Rule 5 — No trailing underscore on member variables

### Prohibition

Do **NOT** use a trailing underscore suffix (`_`) on class member variables in new or modified code.

### Rationale

- The majority of the SkyEngine codebase uses plain member names without a trailing underscore.
- Trailing underscores add visual noise and do not match the dominant repository style.

### What to do instead

Use descriptive, plain camelCase names for member variables.

```cpp
// Good
class Widget {
private:
    bool visible = false;
    std::string name;
};

// Bad
class Widget {
private:
    bool visible_ = false;
    std::string name_;
};
```

### Detection

If you encounter existing trailing-underscore members during a task, do **not** rename them unless they are already in scope.

---

<!-- Future rules go here as ## Rule N sections -->
