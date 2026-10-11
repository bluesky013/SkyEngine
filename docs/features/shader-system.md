---
title: "Shader System"
description: "Aurora's slang-based shader pipeline, the pre-generated GlobalBlock header, and how to regenerate it."
module: "aurora"
updated: "2026-10-11"
---

## Overview

Aurora compiles shaders from a single [Slang](https://shader-slang.com/) source with `Aurora.ShaderCompiler`
(`engine/aurora/shader`, backed by the `slang` third-party library) to per-backend bytecode / source
(SPIR-V, HLSL, MSL). The compiler is also available as a runtime module (`Aurora.ShaderCompiler` shared lib)
that registers itself into `ShaderCompilerFactory`; apps that omit it fall back to cache-only shader resolution.

## Generated global block header

The global uniform block is declared once in Slang:

```
engine/aurora/shader/assets/shaders/slang/global/GlobalBlock.slang   ->  set 0 (GlobalRenderResources)
```

Its layout must match the C++ side, so the reflection is materialized into a header:

```
engine/aurora/pipeline/generated/GlobalBlock.gen.h
```

`engine/aurora/pipeline/src/GlobalRenderResources.cpp` includes it to obtain the `RgBlockDesc` used for
field offsets and total size. Keeping the two in sync is what makes the global block usable from both sides.

## Why the header is pre-generated and committed

Generating the header requires a **host tool** (`ShaderHeaderTool`), which links `Aurora.ShaderCompiler.Static`
(slang) and runs on the build machine. Cross builds (iOS / Android) cannot run host tools built for the target,
so the header is generated on a host machine and committed. When `SKY_MOBILE` is set, the engine skips the host
tools and consumes the committed header directly:

- `engine/aurora/shader/CMakeLists.txt`: builds `ShaderHeaderTool` / `ShaderCookTool` only when **not**
  `SKY_MOBILE`.
- `engine/aurora/pipeline/CMakeLists.txt`: includes `generated/` and no longer depends on a build-time
  codegen target.

## Regenerating `GlobalBlock.gen.h`

Run on a desktop machine (macOS / Windows / Linux) after building the tool:

```bash
# From the repository root
cmake -S . -B build
cmake --build build --target ShaderHeaderTool

output/bin/ShaderHeaderTool \
    engine/aurora/shader/assets/shaders/slang/global/GlobalBlock.slang \
    engine/aurora/pipeline/generated/GlobalBlock.gen.h
```

Then commit the updated `GlobalBlock.gen.h`. Regenerate it whenever `GlobalBlock.slang` changes — a mismatch
between the slang declaration and the committed header leads to wrong field offsets at runtime.

> The header starts with `// generated: do not edit`; do not hand-edit it.

## Related

- `engine/aurora/AGENTS.md` — RHI / pipeline conventions.
- `engine/aurora/shader/assets/shaders/slang/` — slang sources by resource tier (`global` = set 0,
  `pipeline` = set 1, `batch` = set 2).
- [Cross-Platform Determinism](../features/cross-platform-determinism.md) — shader/hash determinism notes.
