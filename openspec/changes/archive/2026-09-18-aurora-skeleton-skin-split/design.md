## Context

Aurora has no skinning pipeline yet, but the type vocabulary is already load-bearing. Animation is an **independent module**: `engine/animation` builds the static library `Animation` and depends only on `Core`. The old render stack respects this: `engine/render/core` keeps a self-contained `Skin` (bone matrix palette) and only `engine/render/adaptor` links `Animation`. Aurora must follow the same rule instead of hosting an `aurora/animation/Skeleton`.

Current problems: `resource::Skeleton` mixes the rig hierarchy with `inverseBind`, the scene component `Skin` occupies the name needed by the mesh-side binding, and `Mesh::GetSkeleton()/HasSkin()` mixes two concepts.

## Goals / Non-Goals

**Goals:**
- Aurora defines **no** `Skeleton` and does not depend on the `Animation` module.
- `Skin` is a self-contained mesh-side skinning binding.
- `SkinnedMesh` is the scene instance component; `Mesh` exposes only the skin API.
- `animation::Skeleton` → `Skin::boneMatrices` mapping lives in an external bridge layer.

**Non-Goals:**
- Implementing the skinning pipeline, animation evaluation, or GPU palette upload.
- Creating an `aurora/animation` module/target (explicitly rejected).
- Changing RHI/backends.

## Decisions

### D1. Aurora defines no `Skeleton`

No `aurora/animation/` directory, no `aurora::Skeleton`/`Bone`. The animation rig lives in `engine/animation` (`sky::Skeleton`, target `Animation`), independent of aurora. This avoids a duplicate `Skeleton` and keeps `aurora/core` free of an `Animation` dependency.

### D2. `Skin` is self-contained

`aurora/resource/Skin.h`:

- `Skin : RefObject` — `inverseBindMatrices` (bind pose), `boneMatrices` (evaluated palette, old `render::Skin`), `boneMapping` (vertex-bone-slot → bone index).

It does not reference any animation type. The joint hierarchy is not needed on the aurora side; the bridge produces `boneMatrices`.

### D3. Scene component `Skin` → `SkinnedMesh`

`aurora/scene/SceneTypes.h`: `SkinnedMesh { CounterPtr<Skin> skin; }`, tagged `sky.aurora.SkinnedMesh`. Renaming frees `Skin` for the mesh-side type and avoids a future collision with a render-side palette.

### D4. `Mesh` exposes only the skin API

`Mesh` gets `SetSkin/GetSkin/HasSkin`. The previous `HasSkin()` meaning "has a skeleton" becomes "has a `Skin`". No `SetSkeleton/GetSkeleton/HasSkeleton`.

### D5. Bridge layer maps animation → Skin

`animation::Skeleton` (+ `AnimationPose`) → `Skin::boneMatrices` is done by a bridge/adaptor layer that links `Animation` (mirroring old `render/adaptor`). `aurora/core` stays animation-agnostic. Render-side per-frame palette types, if needed, are `SkinningPalette`/`SkinMatrices`, never `Skin`. Recorded in `engine/aurora/AGENTS.md`.

## Risks / Trade-offs

- **Source-breaking rename** of `Skin`/`HasSkin` under `sky::aurora`. Only Aurora tests use them today; grep confirms no other consumers.
- **`Skin` mixes asset (`inverseBind`) and runtime (`boneMatrices`)** — acceptable for now; if awkward, split the runtime palette into `SkinningPalette`.
- The bridge layer does not exist yet, so no code maps animation into `Skin` until it lands.

## Migration Plan

Single refactor, no runtime migration. Update tests (`MeshResourceTest`, `SceneCollectTest`) in the same change.

## Open Questions

- Should `Mesh` hold one `Skin` or per-submesh skins (old `SkeletalMesh` had an array)? (Current: single `Skin`; revisit with technique/material sectioning.)
- Where exactly will the animation→Skin bridge live (aurora adaptor target vs framework)? (Current: a future adaptor that links `Animation`.)
