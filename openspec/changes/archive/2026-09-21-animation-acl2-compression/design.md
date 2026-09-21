## Context

`openspec/specs/animation/spec.md` already requires ACL v2 cook-time compression, bounded error, deterministic output, preserved duration, runtime-only decompression, a third-party-packaged dependency, and asset versioning/compression metadata. None of it was implemented; `python/third_party.py` had no `acl` package.

ACL v2 is header-only (no compiled library targets) and its root `CMakeLists.txt` forces unit tests on and installs only tool executables, so it cannot be consumed through the existing third-party flow without a small patch. ACL depends on RTM, shipped as a git submodule.

## Goals / Non-Goals

**Goals:**
- Package ACL v2.1.0 (MIT) as a header-only third-party dependency with no vendored copy.
- Implement cook-time compression and runtime decompression for animation clips.
- Keep the runtime path free of compressor code.
- Keep the feature optional (`SKY_ANIMATION_ACL`) with graceful degradation.

**Non-Goals:**
- Integrating compressed clips into `.clip` asset cooking/serialization and graph/plan ops (builder/asset layer is deferred with the aurora integration).
- ACL databases, streaming, or multi-clip packing.
- Changing the existing virtual/plan evaluation paths.

## Decisions

### D1: Header-only ACL package with a minimal patch
Add `acl` to `cmake/thirdparty.json` with `header_only: true`, `submodules: ["external/rtm"]`, and `ACL_HEADER_ONLY=true`. `cmake/patches/acl.patch` disables unit tests, guards the compressor/benchmark/regression subdirectories behind `ACL_HEADER_ONLY`, and adds install rules for `includes/` and `external/rtm/includes/`. `cmake/thirdparty/Findacl.cmake` exposes `3rdParty::acl` via `sky_3rd_header_only`, so both `acl/` and `rtm/` headers resolve from one include root.
- *Alternative considered*: build ACL as a static library. Rejected - ACL ships no library targets, only tool executables.

### D2: Resample sparse tracks before compression
ACL compresses uniformly sampled track lists, but our clips store sparse keyframes. `AnimationClipCompressor` derives the sample count from the last key time, samples the pose per frame through the existing `AnimationTrackData::SamplePose`, and builds a dense per-bone `rtm::qvvf` block with a fixed stride. Only then does ACL see a track list.
- *Alternative considered*: feed ACL varying bit rates directly. Rejected - our sparse representation is not a uniform sample stream and would need a new track abstraction.

### D3: Cook/runtime split in separate translation units
`AnimationClipCompressor` (compression headers) and `AnimationCompressedClip` (decompression headers only) live in separate `.cpp` files so a runtime-only build can exclude the compressor TU. `AnimationCompressedClip::Sample` uses `acl::decompression_context` + a custom `acl::track_writer` that writes directly into the engine's `Transform` layout.
- *Alternative considered*: one TU. Rejected - it would instantiate compressor code in the runtime module.

### D4: Feature gate with graceful degradation
`SKY_ANIMATION_ACL` (default ON) adds the `3rdParty::acl` link and the `SKY_ANIMATION_ACL=1` definition only when the package is found. Without it, `Compress` returns false and `Sample` is a no-op, so the `Animation` module still builds.
- *Alternative considered*: hard dependency. Rejected - keeps the engine buildable if ACL is unavailable.

### D5: Blob ownership and format
The compressed blob is a raw copy of ACL's `compressed_tracks` buffer held in a `std::vector<uint8_t>`, plus `numBones`/`numSamples`/`frameRate` metadata. It is pointer-free and ready to be stored with other runtime animation assets. Full asset-format versioning/metadata is deferred with the asset layer; the runtime exposes `SetBlob`/`GetBlob` for the eventual serializer.

## Risks / Trade-offs

- [ACL header-order dependencies] → Include `acl/core/sample_rounding_policy.h` before `track_writer.h`; ACL headers are not self-sufficient.
- [Error metric must be set explicitly] → `get_default_compression_settings()` leaves `error_metric` null; the compressor sets a `qvvf_matrix3x4f_transform_error_metric` and a per-track `precision` from the settings.
- [Blob alignment] → `make_compressed_tracks` may require alignment; a large `std::vector<uint8_t>` allocation is 16-byte aligned on x64 in practice. Revisit with an aligned allocator if a platform complains.
- [Resampling changes the curve] → Compression happens on resampled frames at the clip frame rate; this matches the existing runtime sampling rate, and the error threshold bounds the reconstruction error.
- [Patch fragility] → The patch is pinned to v2.1.0. A tag bump requires regenerating it.

## Migration Plan

1. Run `python python/third_party.py -p <platform> -t acl` to build the package.
2. Reconfigure CMake (the package must exist before `SKY_ANIMATION_ACL` can be enabled).
3. Build `Animation` / `AnimationTest`; run the ACL tests.
4. Deferred: asset cooking writes `AnimationCompressedClip` blobs with version metadata; plan ops gain a compressed clip variant.

Rollback: set `SKY_ANIMATION_ACL=OFF` (keeps the code, disables the dependency), or revert the change commit.

## Open Questions

- Should the runtime hold a persistent `decompression_context` instead of constructing one per `Sample` call?
- Should compression operate on the plan's `AnimationTrackData` or on a cooked sample stream produced by the asset importer?
- Does the eventual `.clip` asset need ACL's optional error metadata (for databases) or only the playable blob?
