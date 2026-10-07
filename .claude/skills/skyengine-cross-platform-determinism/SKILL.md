---
name: skyengine-cross-platform-determinism
description: Enforce SkyEngine cross-platform determinism constraints (fixed-width integers, char signedness, endianness, FP contraction/fast-math, ordering, pointer hashing) when writing or reviewing code used by lockstep, rollback, deterministic physics, or reproducible hashing/assets.
compatibility: claude
metadata:
  audience: contributors
  source: project conventions
---

# SkyEngine Cross-Platform Determinism

Use this skill whenever you write or review code that must produce **bit-identical** results across platforms
and architectures: lockstep networking, rollback re-simulation, deterministic physics, and reproducible
hashing / asset generation.

## Goal

Stop the classes of platform-dependent behavior that silently break determinism, and know how to verify them.

## Checklist

Apply these to any code whose output feeds deterministic state, hashes, snapshots, or wire formats.

- **Integers**: use fixed-width `<cstdint>` types wherever bit width or overflow matters. Never `long` /
  `unsigned long` when an exact width is implied (`core/type/Type.h` already rejects registering them).
- **Bytes/text**: treat bytes as `uint8_t`; never hash/accumulate a plain `char` (signed on x86, unsigned on ARM).
- **Endianness**: encode integers with an explicit byte order (little-endian is the engine convention); do not
  `memcpy` native-width integers to/from byte streams.
- **Floating point**: no fast-math; disable FP contraction for deterministic builds with
  `-DSKY_DETERMINISTIC_FP=ON` (`-ffp-contract=off` / `/fp:precise`); keep `SKY_MATH_SIMD` OFF; fix reduction order.
- **Ordering**: emit snapshot/replication output in a total, platform-independent order (sort it), never in
  `unordered_map`/`unordered_set` iteration order; never hash or key on pointer values/addresses.
- **Layout/UB/libraries**: do not serialize raw struct memory; avoid UB; pin toolchain and third-party versions
  for cook output; fix RNG seeds; keep wall-clock/addresses/scheduling out of deterministic data.

## Known examples

- `engine/core/src/crypto/md5/MD5.h`: `unsigned long` used as a 32-bit word (64-bit on LP64) → `uint32_t UINT4`.
- `engine/core/include/core/hash/Fnv1a.h`: `char` signedness in byte hashing → `static_cast<uint8_t>(str[i])`.

Avoid false positives: the `unsigned long pos` in `core/util/Memory.h` is required by the MSVC
`_BitScanForward*` intrinsics (Windows-only); `Murmur3Hash32`'s native-order `memcpy` is safe on the
all-little-endian targets.

## Build option

```bash
cmake -S . -B build -DSKY_DETERMINISTIC_FP=ON
```

## References

- `docs/features/cross-platform-determinism.md`
- `openspec/specs/physics-determinism/spec.md`, `openspec/specs/network-determinism/spec.md`
- `openspec/changes/deterministic-cross-platform-hardening/`
