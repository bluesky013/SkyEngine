## Boundary

Core supplies checked filesystem operations and atomic replacement. Framework owns checked product loading/saving, active instance lifetimes, generated catalog records, cook routing and completion. Neither layer depends on an animation schema, renderer, editor or plugin.

`add-skeletal-animation-workflow` consumes these APIs for its Mesh/Skin/Skeleton groups and Clip/Graph products. Its end-to-end requirements remain there; completing this infrastructure does not complete character, packaging or PIE acceptance.

## Publication and reads

The original group-generation design is superseded by `simplify-asset-product-storage`. Current publication uses checked flat UUID products with per-file atomic replacement and explicit batch failure, without an old-complete-group rollback promise. Old development output is cleared and recooked; no compatibility reader remains.

Generated children retain source-local keys, UUIDs and types in the existing source catalog. A child resolves to its real owner for cooking, while dependency and load results retain the child identity. Coalesced requests still complete individually: owner success does not imply that a removed child exists.

Loaded objects retain active CPU/bulk/GPU data while referenced, but this does not require durable product generations or a runtime group. Checked archive/file operations report recoverable failures explicitly.

## Commit dependencies

The source changes use existing Core/Framework targets. In the isolated snapshot, the `RetainModuleCode` registration lines and their storage members are left to the process-owned-reflection batch; no new guard or alternate reflection entry is introduced. The final working tree retains those leases.

The `remove-unused-legacy-rendergraph` batch is ordered first for review but supplies no asset API dependency. Framework tests copy source fixtures into their build directory so validation cannot dirty the source database.
