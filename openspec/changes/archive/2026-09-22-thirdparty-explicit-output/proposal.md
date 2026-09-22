## Why

`python/third_party.py` builds the third-party libraries and then, by default, zips the entire platform output
and records the resulting file name + MD5 into `cmake/thirdparty.json` under `archives` (`archive_output()`).
That couples the build to a prebuilt zip, makes the JSON a merge-conflict magnet on every branch that adds a
dependency (e.g. `acl` vs `cpython` vs `openssl`), and silently writes generated state into a tracked config
file. The `archives` block has just been removed from `cmake/thirdparty.json` while rebasing; the tooling must
match.

## What Changes

- Remove the default archive/zip behaviour: delete `archive_output()` and its call, and stop writing the
  `archives` section (or any `file`/`md5`) into `cmake/thirdparty.json`.
- Require an explicit third-party output directory: the tool SHALL take `--output <dir>` explicitly instead of
  silently defaulting to `<engine>/build_3rd`. (Or, if a default is kept, it SHALL point outside the tracked
  tree and SHALL NOT be recorded in a tracked file.)
- Keep only the per-platform build metadata in the output directory (`.thirdparty_cache`/metadata) — generated,
  not tracked.
- Update the CMake consumers and docs so the third-party path is provided via configuration
  (`3RD_PATH`/`SKY_THIRD_PARTY_*`), independent of any `archives` entry.
- Optional: expose packaging as an explicit, separate, opt-in command (not run by default).

## Capabilities

### New Capabilities
- `thirdparty-build`: the third-party build tooling contract — it SHALL build a platform into an explicitly
  specified output directory and SHALL NOT write archive/zip information into tracked configuration by default.

### Modified Capabilities
<!-- None. -->

## Impact

- `python/third_party.py` (remove `archive_output()` and its call; make `--output` explicit).
- `python/third_party_gui.py` / `python/project_manager.py` (stop assuming the default `build_3rd` path / zip).
- `cmake/thirdparty.json` (no `archives`; remains the dependency list only).
- `cmake/**` consumers that read the archive or the default path.
- Docs / build scripts that describe the third-party workflow.
