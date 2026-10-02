## 1. Manifest core (framework asset)

- [x] 1.1 Add a source-manifest module in `engine/framework/asset` that parses and writes `assets.jsonl` (JSON Lines: one JSON object per line, `{"file","id"[,"cook"]}`, sorted by `file`, UTF-8, LF, trailing newline).
- [x] 1.2 Implement atomic write (temp file + rename) through `FileSystem`, and a parsed-manifest in-memory cache keyed by logical directory path.
- [x] 1.3 Treat a malformed manifest as empty with a warning log (`LOG_W`), and invalidate the cache on write.
- [x] 1.4 Add unit tests for parse/write round-trip, sorting determinism, duplicate filename handling, and malformed input.
- [x] 1.5 Introduce the mount-based logical namespace: compose source roots in `MultiFileSystem` (workspace writable first, engine read-only next), remove `SourceAssetBundle`/`sourceBundle`, and route `FindAsset`/manifest I/O through the owning mount.

## 2. Identity assignment and resolution

- [x] 2.1 Change `AssetDataBase::RegisterAsset` to resolve the UUID from the directory manifest instead of `Uuid::CreateWithSeed(CalculateHash(path))` (`engine/framework/src/asset/AssetDataBase.cpp:122`).
- [x] 2.2 On a manifest miss, seed the entry from a recoverable legacy UUID (`assets.db` row, or `CalculateUuidByPath` during migration), else generate `Uuid::Create()` and persist the entry.
- [x] 2.3 Route `FindAsset(path)` (logical path, owning mount) through manifest lookup with `assets.db` fallback only during migration.
- [x] 2.4 Stop exposing `CalculateUuidByPath` as an identity API (keep it only for migration; update `AssetDataBase.h`).
- [x] 2.5 Add tests: repeated registration is stable; manifest entry wins over path; manifest-miss during migration seeds the legacy UUID.

## 3. Mount namespace and runtime index

- [x] 3.1 Ensure manifest read/write works across the mount namespace (workspace writable, engine read-only) per 1.5.
- [x] 3.2 Restrict manifests to directories that contain registered assets; never create them for build-output or non-asset directories.
- [x] 3.3 Emit a generated product index per product bundle (`<bundle>/product.index`, JSON Lines `{"path","id"}`) keyed by logical path → id at build end.
- [x] 3.4 Load each bundle's `product.index` in `AssetManager::AddAssetProductBundle` into AssetManager's own product path→uuid map (not `AssetDataBase`); a logical path is unique, so no runtime bundle precedence is needed.
- [x] 3.5 Remove `assets.db` from the runtime bundle and confirm runtime never reads `assets.db` or `assets.jsonl`.
- [x] 3.6 Add tests: `LoadAssetFromPath` resolves through the product index in a runtime bundle with no `assets.db`; a path whose product lives in a non-`common` bundle resolves after merging bundle indexes.
- [x] 3.7 Implement two-level path resolution: `AssetManager` product map first, then (editor only) source catalog `ResolvePath` for not-yet-cooked assets; the runtime resolves paths only through the product map.

## 4. Migration pass

- [ ] 4.1 Implement a one-time migration that scans all mounts (filtered by builder-known extensions, plus `.world`) and seeds each manifest with the legacy `(bundle, path)` UUID, reproduced via a mount→legacy `SourceAssetBundle` role mapping; use `assets.db`, when present, only as a cross-check. (deferred: source-identity migration follow-up)
- [ ] 4.2 Provide an idempotent migration entry point via the `AssetTool` background worker (D5/D7); create manifests only in asset directories (may land in per-tree batches). (deferred: source-identity migration follow-up)
- [ ] 4.3 Document rollback (delete manifests → path derivation resumes) and verify existing references resolve after migration using a migrated world/material fixture. (deferred: source-identity migration follow-up)

## 5. Source-asset mutation (framework)

- [x] 5.1 Add a move/rename primitive to `IFileSystem` (`engine/core/include/core/file/FileSystem.h`) and implement it in `NativeFileSystem`.
- [x] 5.2 Add `AssetDataBase` mutation APIs: `MoveAsset`, `RemoveAsset` (existing, wire it), `DuplicateAsset`; update manifests and `pathMap`/`idMap` with UUID preserved on move and regenerated on duplicate.
- [x] 5.3 Add tests: move across directories keeps the UUID and updates both manifests; delete removes file + entry + identity; duplicate gets a new UUID.

## 6. World/document identity

- [ ] 6.1 Register world documents (`.world`) as source assets via the resolver (`RegisterAsset(path, build=false)`) so they get manifest identity; the editor document wiring lands with the sandbox editor refactor. (deferred: sandbox editor refactor)

## 7. assets.db scope

- [x] 7.1 Make `assets.db` a dev/build-side cache: rebuild it from source scan + manifests + builder `QueryType` when absent, and stop requiring it for source identity.
- [x] 7.2 Ensure `assets.db` is written/read only in editor/build contexts and excluded from the runtime bundle; update `.gitignore` policy accordingly.

## 8. Unified product loading

- [x] 8.1 Make the editor load asset data through `AssetManager`/product bundles (same loader as runtime), not from source files; viewport wiring lands with the sandbox editor refactor.
- [x] 8.2 Convert product payloads to UUID-only references: migrate `engine/render/adaptor/src/assets/MaterialAsset.cpp` JSON writes/reads (`:173-175,227-229`, `:192-193`) and any other path-referencing payload writer.
- [ ] 8.3 Make the editor load only from products (never source); delegate a missing product whose source exists to the on-demand cook path (group 10), with no source fallback. (deferred: sandbox editor refactor)
- [ ] 8.4 Add tests: a material product with a texture UUID loads without the source catalog; the editor load path reads no source files. (deferred: sandbox editor refactor)

## 9. Asset async executor (remove taskflow)

- [x] 9.1 Give `AssetExecutor` a dedicated `sky::ThreadPool` and replace `tf::Executor` (`engine/framework/include/framework/asset/AssetExecutor.h`).
- [x] 9.2 Replace `Asset::AsyncTask` / `SavingTask::asyncTask` / `AssetManager`'s `tf::AsyncTask` vector with `sky::TaskNodePtr`/`std::future`; map `dependent_async` to `CreateTask`+`DependsOn`+`Submit`+`GetFuture`, `silent_dependent_async` to `CreateTask`+`Submit`, and `WaitForAll` to `WaitIdle`.
- [x] 9.3 Remove `<taskflow/taskflow.hpp>` from `Asset.h` and `AssetExecutor.h`; verify no `tf::` type remains in `Framework` public headers.
- [x] 9.4 Add tests: dependency-ordered async load runs on `ThreadPool`; deep dependency chain completes; drain-before-save waits for all tasks.
- [x] 9.5 Track the follow-up for full taskflow removal (`core/async/Task.h`, `core/async/NamedThread.h`, legacy `engine/shader`, legacy `engine/render/core`) and dropping `3rdParty::taskflow` from Core — out of scope for this change.

## 10. On-demand cook, completion event, cook modes

- [x] 10.1 Extend `AssetBuildResult` (and the `IAssetEvent::OnAssetBuildFinished` payload) to carry `uuid`, `target`, `retCode`, and an error string so completions can be correlated.
- [x] 10.2 Add the source catalog interface (`ResolvePath`/`Exists`/`GetTarget`; editor: `AssetDataBase`-backed, runtime: empty) and, on an editor load miss with an existing source, schedule a cook and return the asset in a LOADING state (coalesced per UUID); choose the cook target from the effective cook configuration (asset override × preset × platform; default primary bundle `common`); report the asset as missing when no source record exists, and fail hard in runtime.
- [x] 10.3 From the build-finished event, mark the pending asset LOADED on success or FAILED on failure (no source fallback); a later load of a FAILED asset re-attempts.
- [ ] 10.4 Add `ICookRunner` with `InProcessCookRunner` (current `AssetBuilderManager::BuildRequest` on the thread pool) and `OutOfProcessCookRunner` (AssetBuilder process + IPC that raises the same event); select via config. (deferred: out-of-process cook/IPC follow-up)
- [ ] 10.5 Add tests: missing product schedules a cook and the load resumes on success; cook failure fails the load; out-of-process completion raises the same event. (deferred: out-of-process cook/IPC follow-up)
- [ ] 10.6 Add the builder-side `AssetTool` (built independent of `SKY_BUILD_TOOL`, D7), in two parts:
  - [ ] 10.6a Frontend (asset browser): browse the source catalog/manifests, inspect the effective cook config, and trigger cooks. (deferred: AssetTool follow-up)
  - [x] 10.6b Background worker (in-process batch cooking): `CookWorker` drains a work list of `(uuid, target)` jobs (and `CookAll` over registered sources) on the asset/cook pools.
  - [ ] 10.6c Out-of-process cook host: worker process + request/response protocol + worker lifecycle (IPC). (deferred: out-of-process cook/IPC follow-up)

## 11. Asset type identity

- [x] 11.1 Derive the source `AssetTypeId` from the builder registry; remove the `category` field from `AssetSourceInfo` and its (de)serialization in `assets.db`.
- [x] 11.2 Add tests: type-based selection lists only matching assets; a mismatched type is rejected; no category field is written to the manifest or dev cache; the type string is identical across `AssetTraits<T>::ASSET_TYPE`, builder `QueryType`, the handler key, the product header, and the property metadata.

## 12. Cook config, import, compression

- [x] 12.1 Add the project-level cook config (`configs/asset_cook.jsonc`, JSONC): platform→target presets and per-asset-kind settings (encoder, mips, compression).
- [x] 12.2 Add the asset-level cook config as the `cook` block of an asset's `assets.jsonl` line (targets, per-kind overrides, and a free-form `user` block); effective settings = override × project preset × current platform.
- [x] 12.3 Implement multi-platform output: one source → one product per configured target (same UUID, per-bundle), with each bundle's `product.index` mapping the logical path to that UUID.
- [x] 12.4 Implement the import flow: copy source into the writable mount → register (identity + cook-config entry) → optionally cook the current platform (default on, configurable).
- [x] 12.5 Add cook compression: a product-header codec field and loader-side decompression via the existing `CompressionManager` / `ICompressor` (lz4 registered by `CompressionModule`).
- [ ] 12.6 Add tests: import assigns identity and cooks; asset override beats preset; a texture emits BC + ASTC products under one UUID; a compressed product round-trips and an uncompressed one needs no module. (deferred: follow-up tests)

## 13. Asset dependency graph

- [x] 13.1 Add `IAssetDependencyProvider` (forward `Dependencies`, reverse `Dependents`, full-graph iteration) in `engine/framework/asset`.
- [x] 13.2 Assemble the forward graph at runtime from product headers (`AssetManager`) and in the editor from `AssetSourceInfo::dependencies`, persisted only as a derived index in `assets.db`; compute the reverse graph by inversion.
- [x] 13.3 Add tests: forward/reverse correctness; deleting a referenced asset reports its dependents; dependencies are absent from `assets.jsonl`/`product.index`.

## 14. Concurrency and IPC

- [x] 14.1 Run in-process cooks on a separate cook pool (off the loader pool); ensure no loader-pool starvation; document that `BlockUntilLoaded` must not be called on the asset pool.
- [x] 14.2 Serialize `product.index` writes per bundle (lock + atomic temp+rename) so concurrent cooks do not lose entries.
- [x] 14.3 Guard the pending-load table with a lock, make `IAssetEvent` subscription thread-safe, and establish the LOADING wait handle before scheduling the cook.
- [ ] 14.4 Define the IPC length-prefixed frame encoding, route worker logs to stderr, handle partial frames, start the worker with the same mount namespace/platform target, and fail+restart on timeout/crash. (deferred: out-of-process cook/IPC follow-up)
- [ ] 14.5 Add tests: concurrent cooks on one bundle; loader pool not starved under on-demand cook; IPC logs do not corrupt frames. (deferred: out-of-process cook/IPC follow-up)

## 15. Verification

- [x] 15.1 Implement the design Test Cases in `FrameworkTest` / `AssetManagerTest` (manifest, mutation, loading, type identity, cook config, dependency graph, async executor, concurrency/IPC).
- [x] 15.2 Update the affected docs under `docs/` for the source-identity model, manifest format, loading model, and cook configuration.
