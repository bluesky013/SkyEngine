## Why

The `aurora-material` property model stores a caller-supplied byte `size` and overloads a single `offset` field, so a write cannot be validated against a declared type. It also defines a material-only `MaterialPropertyType { VALUE, TEXTURE }` enum that duplicates the shader field vocabulary already owned by `RgFieldType`. Separately, `MaterialInstance` stored each overridden value in its own `std::vector<uint8_t>` (a heap allocation plus map node per field).

## What Changes

- Reuse the shader field type as the material property type: `using MaterialPropertyType = RgFieldType` (value kinds `FLOAT/FLOAT2/FLOAT3/FLOAT4/MAT4/INT/UINT/BOOL`, texture kinds `TEXTURE2D/TEXTURE_CUBE/SAMPLER`); drop the separate `{ VALUE, TEXTURE }` enum.
- `MaterialPropertyEntry` becomes `{ type, offset }`; byte size is derived via `constexpr SizeOfPropertyType(type)`.
- `AddValue(Name, MaterialPropertyType, const void *defaultValue)` — callers declare the type, not a byte size.
- `SetValue<T>` / `GetValue<T>` validate `sizeof(T)` against the derived size (no C++→enum trait template).
- `MaterialInstance` holds a single contiguous copy of the material's value storage (lazily copied on first write) plus a sparse texture-override map; `IsOverridden` compares a field against the material default.
- Update `MaterialTest` to the new API.
- **BREAKING**: `AddValue` signature, `MaterialPropertyType` definition, and `MaterialPropertyEntry` shape change.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities
- `aurora-material`: property types reuse the shader field vocabulary (`RgFieldType`), size is derived from the type, and `MaterialInstance` value overrides use one contiguous storage copy plus a sparse texture map.

## Impact

- Code: `engine/aurora/core/include/aurora/resource/Material.h` (now includes `aurora/shader/RgBlockDesc.h`), `engine/aurora/core/test/MaterialTest.cpp`.
- API: `Material::AddValue`, `MaterialPropertyType`, `MaterialPropertyEntry`; internal-only (no other aurora module consumes these yet).
- No RHI/backend or asset-serialization impact.
