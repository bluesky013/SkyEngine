## Context

The editor needs icons, but there is no pipeline: SVG would be re-rasterized every launch and
bitmaps are resolution/DPI-bound. Expensive-to-produce, reproducible artifacts generally have no
home in the engine, so each feature would invent its own caching. This change introduces a generic
derived-data cache in `engine/framework` and uses it for the first consumer: SVG UI icons.

Constraints from `AGENTS.md`: the DDC is generic engine logic (`engine/framework`, no editor/plugin
dependency); the icon builder is implementation and lives in the sandbox module; a third-party may
only be referenced by the module that needs it, so NanoSVG is confined to the sandbox module.

## Goals / Non-Goals

**Goals:**

- A generic, content-addressed DDC: register builders by id; fetch derived bytes from source bytes.
- Correct invalidation: changing source, builder version, settings, or platform misses the cache.
- An editor UI-icon builder that turns SVG into cached RGBA8 at a requested pixel size.
- NanoSVG isolated to the sandbox module (no engine-wide dependency).

**Non-Goals:**

- Wiring icons into editor chrome (toolbar/buttons) or any concrete icon artwork.
- Runtime texture atlas packing and block compression of the derived output.
- A general cook/build integration or cross-process DDC; only the in-process first-launch bake.
- Cache eviction/GC policy.

## Decisions

- **Cache key = hash(source, builderId, builderVersion, settings, platform).** Chosen so that any
  input that affects output invalidates the entry, mirroring UE-style DDCs. Uses FNV-1a (64-bit)
  from `Core`-free standard code to avoid adding a crypto dependency; collision risk is acceptable
  for a local, input-complete cache. Alternative considered: SHA-256 — rejected as an unnecessary
  heavyweight dependency for this use.
- **On-disk layout `<root>/<key>.bin`.** Flat, one file per entry, key already namespaced by all
  inputs. Alternative: sharded two-level directories — deferred until cache size warrants it.
- **`IDerivedDataBuilder { GetId, GetVersion, Build }`.** Builders declare their own version so an
  algorithm change invalidates cached results without touching callers. Builders depend only on
  their own module + third-party (satisfies the plugin builder rule).
- **DDC lives in `engine/framework`, not `engine/ui`.** It is not UI-specific; icons happen to be
  the first consumer and future texture/asset derivations will reuse it.
- **NanoSVG for rasterization.** Single-header (`nanosvg.h` + `nanosvgrast.h`), permissive license,
  no build system, tiny footprint. Alternatives (lunasvg/thorvg, resvg) are heavier or need build
  integration. The implementation macros are defined in the single consumer translation unit to
  avoid duplicate symbols.
- **Settings string `WxH` for icons.** The builder parses the requested pixel size from the DDC
  settings string; absent/invalid settings fall back to the SVG's intrinsic size. This keeps the
  DDC generic (settings stays an opaque string).
- **Registration at module init.** `SandboxModule::Init` calls `InstallUiIconBuilder()`, registering
  the builder with the process-wide `DerivedDataCache` singleton before any fetch.
- **DDC root under the sandbox resources dir.** `SandboxModule::Init` sets the root to
  `<sandbox resources>/cache`, so baked icons live with the editor's own resources. It could move to
  a user-profile cache later without affecting callers.
- **Demo sample: resource SVG preferred, procedural fallback.** The reflection demo panel resolves
  `resources/icons/save.svg`, bakes it through the DDC and draws the resulting texture; if the source
  cannot be read it draws a directly generated glyph, so the sample always renders and both the
  "generated" and "used directly" paths are exercised.
- **NanoSVG pinned by commit.** NanoSVG has no release tags, so `cmake/thirdparty.json` pins a full
  40-char commit.

## Risks / Trade-offs

- [FNV-1a collision could serve wrong bytes] -> the key already includes every input; for a local
  cache this is acceptable, and a stronger hash can replace it behind the same interface later.
- [Unbounded cache growth] -> out of scope here; a GC/LRU pass over `<root>` is a follow-up.
- [DDC `Fetch` not thread-safe] -> currently called from module init/single-threaded paths; make it
  concurrency-safe when parallel first-launch bake is introduced.
- [NanoSVG fidelity] -> supports the SVG subset needed for UI icons; complex SVGs may render
  imperfectly. Revisit if artwork demands it.
- [Uncompressed RGBA8 output] -> fine for the current cached-icon path; atlas + compression is a
  separate follow-up (matches the runtime UI image plan).

## Migration Plan

Additive. No existing behavior changes. Rollback is deleting the new files/module hook; cached
`.bin` files are inert without the builder.
