## Context

Runtime already consumes UUID-addressed AuroraMesh products. GLB is an authoring source and a useful Inspector label. The defects are implicit source selection, incomplete source identity, stale manifest updates and hidden material policy, not the product file extension.

## Goals / Non-Goals

Goals: preserve Engine identities beside same-path Project files; make mutations safe against cook-worker updates; make defaults explicit at policy owners; publish complete dependency sets; retain readable paths and hover hints.

Non-goals: regenerate existing UUIDs, rename GLB files to imply a new mesh format, reintroduce procedural builtins, relocate directories, change shader path policy or alter backend-owned FrameGraph compilation. Aurora target isolation and larger Sandbox domain hooks have their own review boundary.

## Decisions

1. Source identity is `(mount, relative path)`. UUID lookup opens the owner filesystem. Legacy unqualified path lookup remains a deliberate ordered overlay; Engine bootstrap uses an exact mount query. Both sources are scanned and catalogued, preserving manifest UUIDs.
   Product path publication indexes only the selected overlay identity, independent of cook order. AllowCook consults the source overlay first. ProductsOnly is a published-product snapshot and never queries sources; adding a source mount alone does not rewrite an existing product index, so exporting a changed overlay requires cooking/publishing it.
2. Manifest mutation acquires the filesystem's per-manifest lock, invalidates cached state, rereads, validates ownership, then persists. Multi-directory operations acquire locks in deterministic order and hold them through rollback. Cache invalidation alone or locking a stale copy is insufficient.
3. Catalog unregister and source deletion have distinct APIs. Editor Delete uses the durable source operation, rejects read-only/generated assets and preserves identity on recoverable failures. Source disappearance and manifest removal are coordinated before in-memory publication.
4. Mesh material binding is a pure mapping operation over explicitly supplied material IDs. Builder policy resolves a fallback; the compiler result reports effective dependencies, which SoftBody consumes. Existing `configs/mesh_build_presets.json` uses `bundles.<target>.defaultMaterial` (UUID string): explicit recipe slots win, then the configured material, then exact Engine Unlit. Project and Engine config overlay remains owned by AssetBuilderManager; malformed UUIDs/configuration fail explicitly.
5. The scene resource consumer interface resolves materials independently, carrying required uploads/resources and ownership. Navigation owns geometry and requests only its material. SceneAssetSource does not resolve source paths as a hidden runtime fallback.
6. Aurora editor extension registration runs after the source catalog is mounted/scanned and supplies Engine ground material/mesh IDs to the existing preview provider. Renderer Init does not capture defaults before source bootstrap. Project-configured creation material UUIDs remain authoritative; the preview stage is a separate policy. Inspector path labels/hints are unchanged. DDC uses writable host data rather than the deployed resource directory.

## Risks / Trade-offs

- Cross-file moves are not crash-atomic filesystem transactions. Lock ordering and checked rollback cover recoverable API failures; verification distinguishes those from power-loss recovery.
- Invalid preset syntax/UUIDs and an unavailable Engine default source fail preparation explicitly. Supplied/configured nonzero UUIDs are compilation inputs; asset existence and material type are checked by dependency/resource loading, not by the pure binding step. Tests supply explicit identities rather than depend on a developer checkout.
- Adding exact queries must not silently change intentional project override behavior. Existing unqualified API remains available and covered.
- Previously cooked products with no material can require recook or explicit override; runtime must not conceal malformed products by importing sources.

## Migration Plan

Keep manifests and UUID serialization unchanged. Rebuild catalogs to register all mounts. Rebuild affected cook products where fallback/dependencies change, then run Framework, Aurora, Recast, SoftBody and editor tests plus a native editor smoke check. Do not commit or archive without user confirmation.
