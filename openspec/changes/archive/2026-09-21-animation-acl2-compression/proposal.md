## Why

The `animation` main spec already requires cook-time curve compression with ACL v2 and a runtime decompression path, but nothing was implemented and the ACL library was not available. The plan and virtual evaluators currently ship uncompressed keyframes only, so clip memory/bandwidth is unoptimised. This change adds the ACL third-party package and implements the cook/runtime split.

## What Changes

- Add `acl` (ACL v2.1.0, MIT) as a header-only third-party package built through `python/third_party.py`, with a minimal CMake patch that disables the unit tests/tools and installs the `acl` + bundled `rtm` headers. No vendored copy under `engine/`.
- Add `AnimationClipCompressor` (cook time): resamples sparse clip tracks into a dense per-bone QVV block at the clip frame rate, sets a per-track precision threshold, compresses with ACL, and produces a pointer-free blob.
- Add `AnimationCompressedClip` (runtime): owns the blob, decompresses via `acl::decompression_context`, and writes into the engine's `Transform` layout through a custom `acl::track_writer`. Decompression only.
- Wire a `SKY_ANIMATION_ACL` option, the `3rdParty::acl` target, and the `SKY_ANIMATION_ACL` compile definition. When disabled, the compression entry points fail gracefully and `Sample` is a no-op.
- Tests: compress -> sample round trip within error tolerance, deterministic output, and blob round trip.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `animation`: pins the concrete ACL2 integration (third-party packaging, resampling before compression, cook/runtime split, feature-gated availability). The behavioural requirements (bounded error, deterministic compression, duration preserved, runtime-only decompression, versioning) already exist in `openspec/specs/animation/spec.md`.

## Impact

- `python/third_party.py` / `cmake/thirdparty.json` / `cmake/patches/acl.patch` / `cmake/thirdparty/Findacl.cmake`: new `acl` package (header-only, `ACL_HEADER_ONLY=true`, unit tests off).
- `cmake/options.cmake`, `cmake/thirdparty.cmake`, `engine/animation/CMakeLists.txt`: `SKY_ANIMATION_ACL` option (default ON), `3rdParty::acl` link, `SKY_ANIMATION_ACL=1` definition.
- `engine/animation`: new `include/animation/acl/AnimationCompressedClip.h`, `src/acl/AnimationCompressedClip.cpp` (runtime), `src/acl/AnimationClipCompressor.cpp` (cook), `test/AnimationAclTest.cpp`.
- No change to the existing virtual/plan evaluation paths; compressed clips are an additional representation.
- Deferred: integrating the compressed representation into `.clip` asset cooking/serialization and into plan ops (the builder/asset layer is still scheduled with the aurora integration).
