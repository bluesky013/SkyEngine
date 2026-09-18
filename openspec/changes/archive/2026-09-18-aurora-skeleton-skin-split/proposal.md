## Why

Aurora's skinning types are named inconsistently and conflict with the animation layer:

- `aurora/resource/Mesh.h` defines `Bone` + `Skeleton` (hierarchy **and** `inverseBind`), i.e. it mixes the animation rig with mesh-side skinning data.
- The scene ECS component is also called `Skin` (`SceneTypes.h`), which is the natural name for the mesh/render-side skinning data.
- Animation is an **independent module** (`engine/animation`, target `Animation`, depends only on `Core`). Aurora must not host a `Skeleton` type: that duplicates the animation rig and couples render core to animation.
- `Mesh` mixes vocabulary: `GetSkeleton()` alongside `HasSkin()`.

Review decision: follow the old engine's layering — `engine/render/core` keeps a **self-contained** `Skin` and only `engine/render/adaptor` links `Animation`. Aurora does the same: no `Skeleton`, self-contained `Skin`, animation mapped in by a bridge layer.

## What Changes

- New `aurora/resource/Skin.h`: `Skin` (`inverseBindMatrices` + `boneMatrices` + `boneMapping`), self-contained mesh-side skinning binding.
- `resource/Mesh.h`: drop the local `Bone`/`Skeleton`; the mesh exposes only `SetSkin/GetSkin/HasSkin`.
- `scene/SceneTypes.h`: rename the scene component `Skin` → `SkinnedMesh` (holds `CounterPtr<Skin>`); update `SKY_TYPE_TAG` to `sky.aurora.SkinnedMesh`.
- **No `aurora/animation`**: animation (`engine/animation`) is independent; the mapping `animation::Skeleton` (+ pose) → `Skin::boneMatrices` belongs in a bridge/adaptor layer that links `Animation`.
- Document the contract in `engine/aurora/AGENTS.md`: aurora defines no `Skeleton`; render-side palette types are `SkinningPalette`/`SkinMatrices`, never `Skin`.

## Capabilities

### New Capabilities
- `aurora-skinning`: the mesh-side skinning type layout and naming contract (self-contained `Skin`, scene `SkinnedMesh`, animation independence).

### Modified Capabilities
- (none)

## Impact

- Affected code: `aurora/resource/Skin.h` (new), `aurora/resource/Mesh.h`, `aurora/scene/SceneTypes.h`, `aurora/AGENTS.md`, and tests `MeshResourceTest.cpp` / `SceneCollectTest.cpp`.
- No behavior change: this is a type/naming refactor. No skinning pipeline exists yet, so callers are limited to tests.
- No effect on RHI/backends. `aurora/core` does **not** depend on the `Animation` module.
