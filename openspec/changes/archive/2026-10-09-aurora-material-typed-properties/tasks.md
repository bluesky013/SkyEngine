## 1. Typed property model (`Material.h`)

- [x] 1.1 Alias `MaterialPropertyType = RgFieldType` (include `aurora/shader/RgBlockDesc.h`); remove the material-only `{ VALUE, TEXTURE }` enum
- [x] 1.2 Add `constexpr SizeOfPropertyType(type)` (packed) and `IsTextureProperty(type)`
- [x] 1.3 Reduce `MaterialPropertyEntry` to `{ type, offset }` (drop stored `size`)
- [x] 1.4 `AddValue(Name, MaterialPropertyType, const void *)` / `AddTexture(Name, type = TEXTURE2D)`; derive `size` from the type
- [x] 1.5 `SetValue<T>` / `GetValue<T>` validate `sizeof(T)` against the derived size (no trait template)

## 2. `MaterialInstance` override storage (option 1: contiguous copy)

- [x] 2.1 Replace `valueOverrides` map with a single lazily-copied `storage` block; keep a sparse texture-override map
- [x] 2.2 `SetValue` writes in place; `GetValue` reads the copy or the material default; `SetMaterial` clears overrides
- [x] 2.3 `IsOverridden` = value field differs from material default, or texture present in the override map

## 3. Tests and verification

- [x] 3.1 Update `MaterialTest.cpp` `AddValue` calls to the typed API
- [x] 3.2 Build `Aurora.CoreTest`
- [x] 3.3 Run `MaterialTest.*` (5/5 pass)
