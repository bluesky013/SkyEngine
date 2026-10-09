## Context

`aurora-material` (archived 2026-09-18) defined `Material` / `MaterialInstance` as header-only types in `engine/aurora/core/include/aurora/resource/Material.h`. The property model was:

- `MaterialPropertyType { VALUE, TEXTURE }` — a material-only enum duplicating the shader field vocabulary.
- `MaterialPropertyEntry { type, offset, size }`, where `size` was supplied by the caller to `AddValue(name, size, default)` and could not be validated.
- `MaterialInstance` stored sparse value overrides as `std::unordered_map<Name, std::vector<uint8_t>>` (one heap allocation + map node per overridden field) and textures as `std::unordered_map<Name, CounterPtr<Texture>>`.

The repository already owns a shader-side type vocabulary, `RgFieldType` in `aurora/shader/RgBlockDesc.h` (`FLOAT/FLOAT2/FLOAT3/FLOAT4/MAT4/INT/UINT/BOOL/TEXTURE2D/TEXTURE_CUBE/SAMPLER`), declared the "single source of truth for a resource block". Reference engines (Filament, Unreal, legacy `engine/render`) also confirm two points: the type is a flat scalar×component enum, and a material instance holds one contiguous value block rather than per-field containers.

Constraints: header-only, `namespace sky::aurora`, no RHI/backend or asset-serialization dependency. No other aurora module consumes these APIs yet (only `MaterialTest.cpp`).

## Goals / Non-Goals

**Goals:**

- One property type vocabulary shared with shader blocks; byte size derived from the type, never supplied.
- Validate `SetValue<T>`/`GetValue<T>` against the declared property.
- Store `MaterialInstance` value overrides without per-field heap allocations.

**Non-Goals:**

- No shader-reflection-derived layout (layout stays explicit `AddValue`/`AddTexture`).
- No GPU pack/upload of material properties; the value block is a packed CPU copy (std140 packaging is out of scope).
- No `MaterialInstance` parent chain or render-state overrides (Filament/Unreal have them; deferred).
- No material asset serialization of the type enum.

## Decisions

### D1: Reuse the shader field type as the material property type

`using MaterialPropertyType = RgFieldType;`. Value kinds reuse `FLOAT/FLOAT2/FLOAT3/FLOAT4/MAT4/INT/UINT/BOOL`; texture kinds reuse `TEXTURE2D/TEXTURE_CUBE/SAMPLER`. `IsTextureProperty(type)` discriminates.

- **Rationale**: `RgBlockDesc` is already the shader-side single source of truth; a second material-only enum is duplication (the original `{ VALUE, TEXTURE }` enum was).
- **Alternative**: a material-owned enum mirroring Filament's `UniformType` (`BOOL..BOOL4`, `INT..INT4`, `UINT..UINT4`, `MAT3/MAT4`) — rejected, duplicates an existing vocabulary and adds types the engine doesn't need yet.
- **Alternative**: legacy `MaterialValue` `std::variant` + `std::visit` — rejected, a different API shape than the requested type-based declaration.

### D2: Drop `MaterialPropertyEntry::size`

`MaterialPropertyEntry` becomes `{ type, offset }`. Size is `constexpr SizeOfPropertyType(type)` (packed CPU layout: `FLOAT=4, FLOAT2=8, FLOAT3=12, FLOAT4=16, MAT4=64, INT=4, UINT=4, BOOL=1`).

- **Rationale**: size is a pure function of the type; storing it invites drift. `offset` stays (layout order is not derivable).
- **Consequence**: `offset` keeps its dual role (value byte offset vs. texture index), discriminated by `IsTextureProperty(type)`.

### D3: Validate by size, no trait template

`SetValue<T>`/`GetValue<T>` look up the property by name and require `SizeOfPropertyType(entry.type) == sizeof(T)`; mismatched or unknown names are a no-op / return false.

- **Rationale**: type implies size, so the size check is the validation the declaration enables. It rejects the realistic mistakes (`Vector4` vs `MAT4`, wrong field) without a C++→enum mapping.
- **Alternative**: an explicit `MaterialPropertyTraits<T>`→enum specialization table — rejected as unnecessary machinery and inconsistent with how the reference engines expose typed setters.
- **Known limitation**: `FLOAT`/`INT`/`UINT` all occupy 4 bytes and cannot be told apart by size; for CPU storage the bit pattern is what is written, and strict type checking is deferred to a reflection-based path.
- **Note**: material storage is packed (`sizeof`-based), not std140; `RgBlockDesc::ComputeFieldOffsets` uses std140 (`vec3` = 16B) for GPU blocks, so material values are repacked when a block is built (out of scope).

### D4: `MaterialInstance` uses one contiguous value copy (option 1)

`MaterialInstance` keeps `CounterPtr<Material> material`, a `std::vector<uint8_t> storage` that is lazily copied from `material->storage` on the first `SetValue`, and a sparse `std::unordered_map<Name, CounterPtr<Texture>> textureOverrides`. `SetValue` writes in place; `GetValue` reads the copy or falls back to the material.

- **Rationale**: matches Filament (`UniformBuffer mUniforms` copy) and the legacy `engine/render` (`storage` copy on `SetMaterial`); removes the per-field heap allocation and map node entirely.
- **Alternative**: sparse `map<Name, vector<uint8_t>>` — rejected (per-field heap, the original problem).
- **Alternative**: contiguous `storage` + per-byte dirty bitmap — rejected as unnecessary; `IsOverridden` can be answered by comparison (D5).
- **Follow-up (non-goal now)**: an instance parent chain (`SetParent(MaterialInstance)`) for Unreal-style hierarchical variants.

### D5: `IsOverridden` compares against the material default

A value field is overridden when `storage` differs from `material->storage` at that field; a texture field is overridden when present in `textureOverrides`. No extra bookkeeping.

- **Rationale**: the instance already holds the material default in the copied block, so "differs from default" needs zero extra storage. Semantics: an explicitly-set value equal to the default reports not-overridden — acceptable and matches "has no effect".
- **Alternative**: an explicit overridden-name set — rejected as extra state for little benefit.

## Risks / Trade-offs

- **[Packed vs std140 size]** Material CPU storage uses `sizeof` (`Vector3` = 12B); GPU blocks use std140 (`vec3` = 16B). → Mitigation: material storage is a CPU-side copy; block packaging is out of scope and done by the block builder.
- **[FLOAT/INT/UINT same size]** Size-based validation cannot distinguish 4-byte numeric kinds. → Mitigation: documented; CPU writes are bit-pattern-equivalent; strict checking needs reflection.
- **[Full block copy per instance]** An instance that overrides one field copies the whole value block (lazily, on first write). → Mitigation: blocks are small; this matches Filament/legacy and avoids per-field allocation.
- **[`SetMaterial` discards overrides]** Rebinding drops override state. → Mitigation: binding is a setup-time call; clears stale offsets.
- **[Instance default propagation]** Because the block is copied, later changes to the material default do not reach an instance that already copied. → Accepted (same as Filament/legacy); use a new instance or re-`SetMaterial`.

## Migration Plan

1. `Material.h`: alias `MaterialPropertyType = RgFieldType`; `SizeOfPropertyType` / `IsTextureProperty`; `MaterialPropertyEntry { type, offset }`; include `aurora/shader/RgBlockDesc.h`.
2. `AddValue(name, MaterialPropertyType, default)`; size-validated `SetValue<T>` / `GetValue<T>`.
3. `MaterialInstance`: lazy contiguous value copy + sparse texture map; `IsOverridden` by comparison.
4. Update `MaterialTest.cpp`; build `Aurora.CoreTest`; run `MaterialTest.*`.
5. Rollback: revert the header + test (header-only, no ABI/migration).

## Open Questions

- Add `MaterialInstance` parent chaining and render-state overrides later?
- Move to reflection-derived layout (name/binding/size from shader) when material↔shader binding lands?
- Should `MAT3` / `INT2..4` be added to `RgFieldType` if materials need them?
