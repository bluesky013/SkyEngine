---
name: skyengine-cross-platform-determinism
description: Enforce SkyEngine cross-platform determinism constraints (fixed-width integers, char signedness, endianness, FP contraction/fast-math, ordering, pointer hashing) when writing or reviewing code used by lockstep, rollback, deterministic physics, or reproducible hashing/assets.
compatibility: opencode
metadata:
  audience: contributors
  source: project conventions
---

# SkyEngine Cross-Platform Determinism

Load this skill when writing or reviewing code that must produce **bit-identical** results across platforms
and architectures: lockstep networking, rollback re-simulation, deterministic physics, and reproducible
hashing / asset generation.

## Goal

Prevent the classes of platform-dependent behavior that silently break determinism, and know how to verify them.

## Working rules

Apply these to any code whose output feeds deterministic state, hashes, snapshots, or wire formats.

### Integers

- Use fixed-width types (`<cstdint>`: `uint8_t`/`int8_t` … `uint64_t`) whenever bit width or overflow matters
  (hashing, serialization, bit ops, rotation, protocol fields).
- Never use `long` / `unsigned long` where an exact width is implied. `core/type/Type.h` already
  `static_assert`s against registering them.

### Bytes / text

- Treat bytes as `uint8_t`; do not hash or accumulate a plain `char` (signed on x86, unsigned on ARM).
- Do not rely on `char` being signed or unsigned.

### Endianness

- Serialize integers with an explicit byte order (little-endian is the engine convention); do not `memcpy`
  a native-width integer to/from a byte stream. Targets are little-endian; big-endian is unsupported.

### Floating point

- Do not enable fast-math. FP contraction (`a*b+c` → `fma`) must be disabled for deterministic builds via
  `-DSKY_DETERMINISTIC_FP=ON` (`-ffp-contract=off` / `/fp:precise`).
- Keep `SKY_MATH_SIMD` OFF for deterministic builds; fix FP reduction order (no nondeterministic parallelism).

### Ordering

- Emit snapshot/replication output in a total, platform-independent order (sort it); never in
  `unordered_map`/`unordered_set` iteration order.
- Do not hash or key on pointer values/addresses in deterministic paths.

### Layout / UB / libraries

- Do not serialize raw struct memory; write fields explicitly (padding/alignment are ABI-dependent).
- Avoid UB (signed overflow, strict aliasing, OOB); pin the toolchain and third-party versions for cook output;
  keep RNG seeds fixed; never let wall-clock time, addresses, or scheduling into deterministic data.

## Known examples (real bugs fixed in this repo)

| Location | Class | Fix |
|---|---|---|
| `engine/core/src/crypto/md5/MD5.h` | `unsigned long` as a 32-bit word (64-bit on LP64) | `typedef uint32_t UINT4;` |
| `engine/core/include/core/hash/Fnv1a.h` | `char` signedness in byte hashing | `res ^= static_cast<uint8_t>(str[i]);` |

Non-issues to avoid false positives: `unsigned long pos` in `core/util/Memory.h` is the required parameter
type of the MSVC `_BitScanForward*` intrinsics (Windows-only); `Murmur3Hash32`'s `memcpy` reads native order
but all supported targets are little-endian.

## Build option

```bash
cmake -S . -B build -DSKY_DETERMINISTIC_FP=ON
```

Default `OFF` changes nothing; enable it for deterministic/replay/parity builds.

## Verification

- Prefer tests that pin reference vectors (e.g. RFC 1321 MD5 vectors in `CryptoTest`).
- Cross-platform trace tests for the `Exact` physics backend are reserved — see `physics-determinism`.

## References

- `docs/features/cross-platform-determinism.md`
- `openspec/specs/physics-determinism/spec.md`, `openspec/specs/network-determinism/spec.md`
- `openspec/changes/deterministic-cross-platform-hardening/`
