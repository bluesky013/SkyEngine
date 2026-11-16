# Separate architecture follow-up (proposal only)

## Aurora asset include boundary

Current `engine/aurora/adaptor/CMakeLists.txt` compiles persistent formats once in `Aurora.Assets.Static`, but exports the entire adaptor include root and links `Aurora.Core.Static`. Cook therefore receives World/render headers and renderer implementation transitively. The source-owner rule is satisfied; include visibility remains broader than its purpose.

A separate change should retain Aurora under `engine/aurora`, first inventory asset header dependencies, then introduce a pure asset include root and move only the persistent formats/serialization/metadata-owner sources into it. Keep runtime registration consuming that unique owner. Preserve the existing STATIC implementation target unless dependency evidence requires a smaller data-only target; any target or module-entry changes require the repository's explicit design approval. Do not duplicate production `.cpp` files in tests or keep forwarding includes indefinitely.

Acceptance: cook links asset code without World/render adaptor include visibility; runtime/editor share exactly one reflection owner; source ownership, real cook/runtime/editor builds and module restart tests pass. This change intentionally performs none of those directory/target moves.

## Cache abstraction after payload sharing

`ResolveMaterialData`, `ResolveGeometry` and `ResolveImage` share lookup and limits, but differ in asynchronous program readiness, child dependencies, binding variants and converted geometry reuse. Their former byte-accounting repetition is now removed: each payload is charged when its owning cache publishes it. A generic `ResolveCached<T>` would hide different pending/error/commit boundaries without eliminating meaningful logic. Keep explicit resolution methods and centralized limits; reconsider only if another cache repeats the complete publication protocol.

CPU budget covers cache-retained converted payload bytes, not process RSS. Loader assets, compiler jobs, in-flight renderer packet copies, and external snapshots retained after Refresh are excluded. Refresh fully clears payload caches and artifact identity accounting. A future partial eviction policy must replace the full-clear ledger with lifecycle-aware accounting before changing budget thresholds.
