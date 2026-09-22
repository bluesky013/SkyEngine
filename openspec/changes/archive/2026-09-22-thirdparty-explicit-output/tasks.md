## 1. Remove the default archive step

- [x] 1.1 Delete `archive_output()` in `python/third_party.py` and remove its call site
- [x] 1.2 Stop writing the `archives` section (and any `file`/`md5`) into `cmake/thirdparty.json`
- [ ] 1.3 (Optional) Add an explicit opt-in packaging command (`--archive <path>`) that does not mutate tracked files

## 2. Explicit output directory

- [x] 2.1 Make `--output <dir>` required (remove `ensure_default_output()`'s `<engine>/build_3rd` default)
- [x] 2.2 Keep the generated cmake cache + per-platform metadata under the output directory (untracked)

## 3. Consumers

- [x] 3.1 Update `python/widgets/third_party_build_widget.py` to stop defaulting to `build_3rd` (output/intermediate now explicit)
- [x] 3.2 Confirm the CMake side reads `3RD_PATH` / `SKY_THIRD_PARTY_*` only and never the `archives` section
- [ ] 3.3 Update docs / build scripts that describe the third-party workflow

## 4. Validation

- [ ] 4.1 Run the tool with an explicit output directory and confirm `cmake/thirdparty.json` is unchanged
- [ ] 4.2 Configure + build the engine against that output
