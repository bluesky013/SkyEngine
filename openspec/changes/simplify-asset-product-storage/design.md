## Scope and decisions

The user explicitly permits clearing development cook output. There is one runtime product layout, no old-format reader, no ProductGroup API and no durable generation history. Source owner-to-child identity remains an authoring/cook concern in the existing source catalog. AssetManager and packaging address products by UUID through the existing bundle/index path.

## Product format

Each `<bundle>/<uuid[0:2]>/<uuid>.bin` contains a 40-byte little-endian prefix: magic u32 `0x44525053` (SPRD), format version u32 `1`, UUID 16 bytes, body length u64 and FNV-1a64 body checksum. The body retains the existing type/dependencies/codec/header/bulk layout. The reader validates version, identity, exact length and checksum before creating the asset or allocating decoded content. InspectProduct returns formatVersion/checksum and checked metadata/payload, without a group.

## Checked publication and failure boundary

SaveAsset returns ProductStatus. SaveProducts first encodes/validates every input and stages, flushes and verifies every temporary file. It then atomically replaces individual UUID files and updates the product index with checked results. No multi-file rollback is promised. A failure before replacement leaves destinations untouched; a failure after partial replacement or index publication invalidates/removes the affected batch rather than exposing a partial batch as successful. Cleanup failures are explicit failures. Builders emit SUCCESS only after all required products and index operations succeed; packaging propagates failed cook/audit results and never accepts mere file existence as build success.

The rebase retains the remote `AssetProductData`/`SaveProductData` offline path: a builder validates its codec payload without loading a runtime handler. This path and typed `SaveProducts` share the same flat-envelope encoding and checked staging/replacement/index publication; they do not introduce a second storage layout. Jolt's shared codec validates collision/material data before raw publication. `InspectProduct` still verifies the complete product without loading assets, mutating the asset cache or scheduling cook. `LoadAssetAsync` remains the existing executor-backed lookup/request boundary, with resolved publication and host executor drain preserving its lifetime contract.

Same-owner cook requests remain coalesced and stale completions cannot finish a newer request. Removed generated children receive their own failure. These are scheduling/source-catalog contracts, not durable runtime group metadata. Independent processes do not obtain a cross-file snapshot guarantee from multiple file replacements.

## Active data and renderer lifetime

Loaded assets expose an in-memory instance ID and revision. A successful rewrite invalidates current cache entries and a later load creates a new instance; published objects are not mutated. An existing instance keeps its already-open bulk stream or owned bytes, never reopens a UUID path to read another product's data. Render caches use instance identity/revision. Mesh/Skin/Skeleton candidates still validate slot layout, reference pose and exact loaded instances before switching; no group generation is used as a shortcut. Existing CPU borrowers and submitted GPU frame resources retire through the existing leases/pins.

## Rebase Jolt ABI and SDK ownership

Retaining the remote Jolt functionality requires SDK ABI settings that match each platform's consumers. C++ exceptions remain disabled; RTTI is platform/consumer matched, including enabled RTTI for macOS Clang, rather than globally forced off. A static Jolt image owns native SDK globals, so its initialization guard and leases follow that actual image. Separate statically linked images cannot use one process guard to skip initialization of another image's globals. Native SDK symbols and private backend types/templates must also remain local to that image: weak-symbol coalescing must not select another image's allocator or implementation through header templates or standard-library construction helpers. Public Physics interfaces and process-shared registries remain visible, metadata retains its unique owner, and no module, target or public SDK lifecycle API is introduced. Existing worlds continue to retain SDK/code leases until jobs and native objects have been released.

## Cleanup and compatibility

Delete old groups/.groups output and headerless products before recooking all required bundles. Do not add directory fallback, obsolete ownership sentinels, dual writes or automatic migration. This development reset does not remove stable source UUIDs or generated child identity. A future released-format migration must be proposed separately after the first formal version.

## Validation

Framework tests cover prefix fields and corruption/truncation, checked encoding/write/flush/readback/replacement failures, batch invalidation, index failure, source-free inspection/loading, generated child completion, fresh instance identity and retained bulk bytes. Producer/integration tests check explicit failure propagation. Existing package tests and native dependency audit are rerun against fresh flat products; supported native atomic-replace behavior is checked without claiming a multi-file transaction.
