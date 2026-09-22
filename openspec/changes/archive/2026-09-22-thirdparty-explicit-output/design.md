## Context

`python/third_party.py` drives the third-party build: it reads `cmake/thirdparty.json` (the dependency list),
builds each package per platform, writes a cmake cache and per-platform build metadata into the output tree, and
`archive_output()` (line ~499) zips the whole output and writes `file` + `md5` into `cmake/thirdparty.json`'s
`archives` section. `ensure_default_output()` defaults `--output` to `<engine>/build_3rd`.

Two problems: the `archives` block is generated state written into a **tracked** config file, and it conflicts on
every branch that adds a dependency; and the implicit default output path hides where the build lands. The
`archives` block was just removed from `cmake/thirdparty.json`, so the tooling is now inconsistent with it.

## Goals / Non-Goals

**Goals**
- The tool never writes archive/zip information into tracked configuration by default.
- The third-party output directory is explicit.
- The CMake side consumes the platform path via configuration, not via an `archives` entry.

**Non-Goals**
- Changing how individual packages are configured or built.
- Removing the ability to produce a distributable archive at all (it becomes explicit/opt-in).

## Decisions

1. **Remove the default archive step.** Delete `archive_output()` and its call. `cmake/thirdparty.json` stays a
   dependency list (no `archives`, no `file`/`md5`). If packaging is still wanted, expose it as a separate,
   explicit opt-in command (e.g. `--archive <path>`) that does not mutate tracked files.
2. **Require an explicit output directory.** `--output <dir>` is required (or defaults to a path outside the
   repository). `ensure_default_output()` no longer injects `<engine>/build_3rd`.
3. **Generated state stays generated.** The cmake cache and per-platform build metadata remain under the output
   directory and are not tracked.
4. **CMake consumes configuration.** The build reads the third-party path from `3RD_PATH` /
   `SKY_THIRD_PARTY_*` (written into the cache), never from an `archives` entry in `thirdparty.json`.
5. **Keep the dependency list authoritative.** `cmake/thirdparty.json` remains the source of truth for which
   packages exist; only the `archives` section is dropped.

## Risks / Trade-offs

- **Existing workflows** that rely on the default `build_3rd` path or on the zip must pass `--output` (and, if
  needed, run packaging explicitly). Mitigation: document the new requirement; keep the metadata format
  unchanged so re-builds only rebuild changed packages.
- **CI** that fetched the archive must instead provide the third-party output (or run the build). Mitigation:
  out of scope for this change; note it as a follow-up.
- **Multiple platforms** still share one `thirdparty.json` (library list). Only the archive hashes are removed,
  so add/add conflicts on the library list remain normal review.

## Migration Plan

```
1. Remove archive_output() + its call; drop the archives write
2. Make --output explicit; remove the build_3rd default
3. Update the GUI / project manager to stop assuming build_3rd / the zip
4. Confirm the CMake consumers only use 3RD_PATH / SKY_THIRD_PARTY_*
5. Update docs/build scripts
```

## Open Questions

1. Should `--output` be strictly required, or default to a path outside the repo (e.g. `../SkyEngine-3rd`)?
2. Is a distributable archive still needed (as an explicit command), or can it be dropped entirely?
3. Do CI / other scripts depend on the archive today?
