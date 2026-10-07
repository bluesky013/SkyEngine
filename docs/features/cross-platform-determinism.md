---
title: "Cross-Platform Determinism"
description: "Constraints and rules for producing bit-identical results across platforms (lockstep, rollback, deterministic physics, reproducible assets)."
module: "core"
updated: "2026-10-07"
---

## Overview

SkyEngine ships features that require **bit-identical results across platforms and architectures**:
lockstep networking, rollback re-simulation, deterministic physics, and reproducible asset/hash generation.
"Bit-identical" means: given the same initial state and the same ordered inputs, every supported platform
produces the same output bits — independent of CPU arch (x86/ARM), compiler, OS, and frame rate.

This document lists the constraints that keep deterministic code deterministic. They are also tracked as
OpenSpec specs (`physics-determinism`, `network-determinism`) and as the change
`deterministic-cross-platform-hardening`.

## Why platform differences happen

Determinism is broken by anything whose behavior is **not fully specified by the language + this document**:

- native integer widths (`long` / `unsigned long`)
- `char` signedness
- endianness
- floating-point evaluation (FMA contraction, fast-math, SIMD reductions)
- container iteration order and parallel reduction order
- pointer/address-dependent behavior
- unspecified struct layout / bitfield layout
- undefined behavior and library/toolchain differences

Two real bugs already fixed in this repo illustrate the classes:

| Bug | Class | Fix |
|---|---|---|
| `MD5.h` used `typedef unsigned long int UINT4` | native width (`unsigned long` is 64-bit on LP64) | `uint32_t UINT4` |
| `Fnv1a.h` did `res ^= str[i]` on `char` | `char` signedness (signed on x86, unsigned on ARM) | `static_cast<uint8_t>(str[i])` |

## Rules

### Integers

- Use fixed-width types (`<cstdint>`: `uint8_t`/`int8_t` … `uint64_t`) whenever **bit width or overflow**
  participates in logic (hashing, serialization, bit ops, rotation, protocol fields).
- Never use `long` / `unsigned long` where an exact width is implied. (`core/type/Type.h` already
  `static_assert`s against registering them.)
- Note: the `unsigned long pos` in `core/util/Memory.h` is the required parameter type of the MSVC
  `_BitScanForward*` intrinsics (Windows-only path) and is not width-sensitive logic.

### Bytes and text

- Treat bytes as `uint8_t`; never hash/accumulate a plain `char` (`char` is signed on x86 and unsigned on ARM).
- Do not rely on `char` being signed or unsigned.

### Endianness

- Serialized/wire formats must encode integers with an explicit byte order (little-endian is the engine
  convention). Do not `memcpy` a native-width integer to/from a byte stream.
- Current targets (x86/ARM macOS/Linux/Windows) are little-endian; big-endian is not supported. Code that
  reads native-order words (e.g. `Murmur3Hash32`'s `memcpy`) is safe only under that assumption.

### Floating point

- **Do not enable fast-math** (`-ffast-math`, `/fp:fast`). Not enabled in this repo.
- **FP contraction** (`a*b+c` → `fma`) varies by compiler/arch and changes results. Enable the
  `SKY_DETERMINISTIC_FP` build option for deterministic builds:
  - `-DSKY_DETERMINISTIC_FP=ON` → `-ffp-contract=off` (GNU/Clang) or `/fp:precise` (MSVC), and defines
    `SKY_DETERMINISTIC_FP=1`.
- Keep `SKY_MATH_SIMD` **OFF** for deterministic builds (SIMD reductions may reorder).
- Fix the order of floating-point accumulation/reduction; do not parallelize reductions nondeterministically.

### Ordering

- Snapshot/replication output must be in a **total, platform-independent order**. `ActorReplicationSource`
  does this correctly: it collects components into buckets and `std::sort`s them, and encodes members in
  their declared (vector) order.
- Do not emit output in `std::unordered_map`/`unordered_set` iteration order.
- Do not hash or key on pointer values/addresses in deterministic paths.

### Layout, UB, and libraries

- Do not serialize raw struct memory; write fields explicitly (padding/alignment are ABI-dependent).
- Avoid UB (signed overflow, strict aliasing, out-of-bounds) — it can differ per compiler/opt level.
- Pin the toolchain and third-party versions for deterministic/cook outputs; keep RNG seeds fixed.
- Never let wall-clock time, addresses, or thread scheduling into deterministic data.

## Build option

```bash
cmake -S . -B build -DSKY_DETERMINISTIC_FP=ON
```

Default (`OFF`) behaves exactly as before (no change to existing numbers). Enable it for deterministic /
replay / cross-platform-parity builds.

## Verification

- Unit tests pin reference vectors (e.g. the RFC 1321 MD5 vectors in `CryptoTest`) and catch width/sign bugs.
- The **cross-platform trace test** for the deterministic physics backend is still reserved — see the
  `physics-determinism` spec (Reserved determinism validation). When an `Exact` backend lands, add a test
  that compares simulation/event traces across platforms.

## Related

- Specs: `openspec/specs/physics-determinism/spec.md`, `openspec/specs/network-determinism/spec.md`
- Change: `openspec/changes/deterministic-cross-platform-hardening/`
- Skill: `skyengine-cross-platform-determinism`
