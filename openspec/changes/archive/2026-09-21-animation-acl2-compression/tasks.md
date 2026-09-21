# Tasks

## 1. ACL third-party package

- [x] 1.1 Add `acl` (ACL v2.1.0) to `cmake/thirdparty.json` as a header-only package with the `external/rtm` submodule and `ACL_HEADER_ONLY=true`
- [x] 1.2 Add `cmake/patches/acl.patch` disabling unit tests, guarding tool subdirectories, and installing `includes/` + `external/rtm/includes/`
- [x] 1.3 Add `cmake/thirdparty/Findacl.cmake` exposing `3rdParty::acl` via `sky_3rd_header_only`
- [x] 1.4 Build the package with `python python/third_party.py -p Win32 -t acl` and confirm `build_3rd/Win32/acl/include/{acl,rtm}` exists

## 2. Build wiring

- [x] 2.1 Add the `SKY_ANIMATION_ACL` option (default ON) in `cmake/options.cmake`
- [x] 2.2 Find the `acl` target and define `SKY_ANIMATION_ACL=1` in `cmake/thirdparty.cmake` when enabled
- [x] 2.3 Link `3rdParty::acl` into `Animation` and `AnimationTest` when enabled

## 3. Cook-time compression

- [x] 3.1 Add `AnimationClipCompressor` with a `Settings` error threshold/level and `Compress(tracks, numBones, frameRate, ...)`
- [x] 3.2 Resample sparse tracks into a dense per-bone `rtm::qvvf` block via `AnimationTrackData::SamplePose`
- [x] 3.3 Build an `acl::track_array_qvvf` with per-track `output_index`/`precision`, compress with `acl::compress_track_list`, and copy the result into a pointer-free blob + metadata
- [x] 3.4 Report ACL failures with a diagnostic instead of a silent false

## 4. Runtime decompression

- [x] 4.1 Add `AnimationCompressedClip` owning the blob + metadata with `SetBlob`/`GetBlob`/`Clear`
- [x] 4.2 Implement `Sample(time, pose)` using `acl::make_compressed_tracks`, `acl::decompression_context::seek` and a custom `acl::track_writer` writing the engine `Transform` layout; clamp time into `[0, duration]`
- [x] 4.3 Keep the compressor in a separate translation unit from the runtime decompressor

## 5. Feature gating

- [x] 5.1 Provide `#if SKY_ANIMATION_ACL` stubs so the module builds without ACL (compression fails, sampling is a no-op)

## 6. Tests

- [x] 6.1 Compress -> sample round trip per authored frame within the error tolerance
- [x] 6.2 Deterministic compression (identical blobs for identical input)
- [x] 6.3 Blob transfer round trip

## 7. Verification

- [x] 7.1 Build `AnimationTest` (Debug) with ACL enabled
- [x] 7.2 Run the full animation test suite and confirm all tests pass

## Future increments (out of scope)

- Integrate compressed clips into `.clip` asset cooking/serialization with version + compression metadata
- Add a compressed-clip variant to the evaluation plan / `AnimationPlan` clip table
- Consider a persistent `decompression_context` per instance instead of per-call construction
- ACL databases / multi-clip packing and streaming
