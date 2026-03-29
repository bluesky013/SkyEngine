---
name: skyengine-cpp-file-creation
description: Author new SkyEngine C/C++ source and header files with ASCII-only content, repository-aligned file structure, and correct logger usage.
compatibility: opencode
metadata:
  audience: contributors
  source: project conventions
---

# SkyEngine C/C++ File Creation

Load this skill whenever creating a new C/C++ source or header file in this repository.

## Goal

Create new `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, and `.inl` files that match SkyEngine conventions and avoid common MSVC and API mistakes.

## Working rules

- Apply this skill to every newly added C/C++ file.
- Pair this skill with `skyengine-coding-rules` when you also edit existing C++ code.
- Prefer small, repository-aligned files over speculative abstractions.
- If an existing nearby file uses a stronger local convention, follow the local convention unless it conflicts with this skill.

---

## Rule 1 - Keep file contents ASCII-only

### Requirement

All content in new C/C++ files must be ASCII-only, even if the editor saves the file as UTF-8 without BOM.

This includes:

- comments
- string literals
- character literals
- identifiers
- disabled code

### Why

This prevents MSVC warning `C4819` on Windows code page 936 builds and avoids invisible Unicode mistakes.

### Must do

- Write comments in English.
- Use plain ASCII punctuation only.
- Replace smart quotes, full-width punctuation, em dashes, and ellipsis with ASCII equivalents.
- If non-ASCII user-facing text is unavoidable, move it to assets/resources or use explicit escaped forms only when truly necessary.

### Must not do

- Do not paste Chinese comments into `.h` or `.cpp` files.
- Do not use full-width punctuation such as full-width comma, period, colon, semicolon, or parentheses.
- Do not rely on editor auto-conversion or local code pages.

---

## Rule 2 - Use the project file preamble style

### Header/source preamble

New files should start with the repository-style comment block:

```cpp
//
// Created by <author> on YYYY/MM/DD.
//
```

If the author name is unknown or not provided, use a neutral variant:

```cpp
//
// Created on YYYY/MM/DD.
//
```

### Header files

Use `#pragma once` in headers immediately after the preamble.

```cpp
//
// Created on 2026/03/28.
//

#pragma once
```

---

## Rule 3 - Use correct SkyEngine logger patterns

### Reality check

`sky::Logger` does **not** provide `Logger::Tag(...)` in `engine/core/include/core/logger/Logger.h`.

Available logging entry points are:

- `Logger::Print(...)`
- `Logger::PrintW(...)`
- macros `LOG_E`, `LOG_W`, `LOG_I`
- macros `LOGW_E`, `LOGW_W`, `LOGW_I`

### Correct tag pattern

When a `.cpp` file needs logging, define a file-local ASCII tag as a string pointer:

```cpp
static const char *TAG = "TerrainRenderGPU";
```

Then log through the macros:

```cpp
LOG_W(TAG, "TerrainRenderGPU is a placeholder.");
```

### Wide logging

Only use `LOGW_*` when the format string and tag are intentionally wide-character values.

### Must not do

- Do not write `sky::Logger::Tag("...")`.
- Do not invent helper APIs that are not present in `Logger.h`.
- Do not pass non-ASCII tag text.

### Minimal example

```cpp
//
// Created on 2026/03/28.
//

#include <plugin/Foo.h>
#include <core/logger/Logger.h>

static const char *TAG = "Foo";

namespace sky {

    void Foo::Init()
    {
        LOG_I(TAG, "Foo init");
    }

} // namespace sky
```

---

## Rule 4 - Match include and declaration conventions

### Headers

- Use `#pragma once`.
- Prefer forward declarations over includes when the full type is not required in the header.
- Keep headers focused on declarations.

### Sources

- Include the matching header first when practical.
- Include `<core/logger/Logger.h>` only if the file actually logs.
- Keep file-local helpers and tags in unnamed namespace or `static` storage as appropriate.

---

## Rule 5 - Pre-submit checklist for new C/C++ files

Before finishing a newly added C/C++ file, verify all of the following:

1. The file contains ASCII characters only.
2. Comments are English and ASCII-only.
3. Header files use `#pragma once`.
4. Logging uses `LOG_E`, `LOG_W`, `LOG_I` or wide variants.
5. Logging tags are simple string literals such as `static const char *TAG = "MyFile";`.
6. No `Logger::Tag(...)` usage exists.
7. Includes and forward declarations follow nearby project patterns.

---

## Quick templates

### New header template

```cpp
//
// Created on YYYY/MM/DD.
//

#pragma once

namespace sky {

    class Example {
    public:
        void Init();
    };

} // namespace sky
```

### New source template

```cpp
//
// Created on YYYY/MM/DD.
//

#include <module/Example.h>
#include <core/logger/Logger.h>

static const char *TAG = "Example";

namespace sky {

    void Example::Init()
    {
        LOG_I(TAG, "Example init");
    }

} // namespace sky
```
