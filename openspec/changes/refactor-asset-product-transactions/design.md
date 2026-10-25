## Boundary

Core supplies checked filesystem operations and atomic replacement. Framework owns product groups, immutable snapshots, generated catalog records, cook routing and completion. Neither layer depends on an animation schema, renderer, editor or plugin.

`add-skeletal-animation-workflow` consumes these APIs for its Mesh/Skin/Skeleton groups and Clip/Graph products. Its end-to-end requirements remain there; completing this infrastructure does not complete character, packaging or PIE acceptance.

## Publication and reads

A transaction records its owner, bundle, expected generation and complete member set. Encoding, exclusive writes, flush and readback precede publication of a complete manifest. A failed or superseded transaction leaves the previously published generation available. Readers pin the selected generation and its bulk storage; reload creates a new snapshot rather than changing a published object.

Generated children retain source-local keys, UUIDs and types in the existing source catalog. A child resolves to its real owner for cooking, while dependency and load results retain the child identity. Coalesced requests still complete individually: owner success does not imply that a removed child exists.

Archive and file operations propagate explicit failure. No exception, assertion or silent success substitutes for a recoverable publication failure. Process interruption and stale completion are tested at the publication boundary.

## Commit dependencies

The source changes use existing Core/Framework targets. In the isolated snapshot, the `RetainModuleCode` registration lines and their storage members are left to the process-owned-reflection batch; no new guard or alternate reflection entry is introduced. The final working tree retains those leases.

The `remove-unused-legacy-rendergraph` batch is ordered first for review but supplies no asset API dependency. Framework tests copy source fixtures into their build directory so validation cannot dirty the source database.
