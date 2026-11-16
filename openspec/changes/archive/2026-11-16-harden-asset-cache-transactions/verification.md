# Initial asset remediation verification

Date: 2026-11-15. Existing macOS x86_64 Xcode Debug cache and third-party root were retained. No commit, archive, target relocation or source-owner change.

## Implemented boundary

- Manifest and product-index readers retain immutable snapshots. Reload does not publish; atomic replacement precedes cache publication. Per-slot serialization prevents late cache reads replacing saved state. Clear preserves active serialization points while releasing idle slots.
- Partial parsing preserves physical line diagnostics, isolates duplicate key/UUID participants, validates logical paths and sorts in batches. Failed strict parsing preserves output; incomplete snapshots cannot be saved.
- Directory scans return errors, warnings and blocked directories. Healthy writable directories batch missing identities; damaged/read-only directories remain unwritten. Manifest identities supersede cache identities, including conflicting records in other directories.
- Import uses exclusive writes. Duplicate prepares cook and generated metadata before one source identity commit. Creation results distinguish commit/repair and expose cleanup failures/residual paths. Move and generated authoring saves reuse destination validation.
- Source enumeration returns copied fields rather than a reference to a mutable map. UUID settings tests cover an invalid second field without applying the first.
- The path-resolution delta modifies the complete existing requirement; source-first AllowCook, product-only ProductsOnly and missing-on-ambiguity are explicit.

## Evidence

Final full default-target Debug build succeeded with the existing Xcode cache. The final suites passed with no failures or skips:

| Suite | Passed |
| --- | ---: |
| FrameworkTest (including Cocoa/WindowServer tests) | 346 |
| EditorCoreTest | 209 |
| EditorShellTest | 168 |
| Total | 723 |

OpenSpec strict validation, git diff whitespace checks and modified C++ ASCII checks passed. Build organization was unchanged.

Retained build and suite logs: `cmake-build-debug/asset-cache-validation/`.

Native macOS check used the freshly built editor and modules with Metal and the existing disposable Creation Defaults Validation project. Opened `engine-assets-unified.world`, selected Cube and verified Inspector displays `Engine/shapes/cube.glb` and the Engine material path. Selection geometry and transform handles are visible; scene contents were not edited or saved. Retained screenshot: `cmake-build-debug/asset-cache-validation/editor-asset-paths.png`. The editor remains open.

The status bar displays 0 fps. Repository search finds `EditorShell::SetFrameStats` only at its declaration and definition, with no production callers, so that label is not evidence of measured rendering throughput. This was a native asset/path and interaction smoke check, not a frame-rate, lighting or complete renderer acceptance test. Cross-platform native acceptance and the later C/D/E checks remain open.

The scan I/O regression uses 101 assets in one directory: missing manifest requires one lock and one save, at most two read attempts; the next scan reads once, takes no write lock and performs no save. Independent-worker interleaving is injected between planning and authoritative reread; it must preserve generated metadata and publish the child catalog.

## Remaining work and limits

- This is the initial asset implementation, not completion of all 29 tasks. The global asset lock still spans directory I/O; B3 lock phases, request generations, version-aware coalescing and pre-replacement worker validation remain open.
- The continuation introduces a private source-manifest transaction and typed mutation results for create/import/duplicate/move/remove/delete/persist/cook settings. Generated catalog and product publication statuses carry commit state and recovery diagnostics. Registration retains its source-pointer interface and logs persistence/catalog failures; B3 still owns its request submission and generation protocol.
- Atomic JSON saves now return checked write/cleanup diagnostics. Failed exclusive writes with uncertain ownership report residual files rather than deleting a potentially unrelated file. Cross-file move compensation can require manual repair; no crash-atomic cross-directory guarantee is claimed.
- Snapshot publication is serialized, not a persisted cross-process revision scheme. Filesystem locks and authoritative rereads still govern writes. Mtime-based background cache refresh is not implemented.
- Cross-file operations are not power-loss atomic. Directory damage repair is explicit; scans do not rewrite partial manifests.
- C/D/E editor interaction, settings switching, shared payload budgets and command incarnation work are tracked but not implemented by this phase. Windows/Linux native execution is not established by the macOS build.

## Transaction continuation

- `SourceManifestTransaction` reuses the existing cache and native lock/replace APIs; no target or directory relocation was introduced.
- Manifest commit is authoritative: failed delete cleanup leaves the identity deleted and reports its staged file; failed move compensation withdraws the old catalog entry and reports manifest/source recovery locations.
- Generated metadata is returned after a durable commit even when catalog publication fails. A fault-injection regression forces a post-commit identity conflict, removes the conflict and verifies `RefreshOwner` replays the committed child IDs.
- Complete validated product replacement followed by index failure retains the products and reports repair. The specification explicitly permits checked UUID reads while cook/package completion remains failed; recook repairs the index. Partial replacement still invalidates the batch, and failed compensation reports residual product paths.
- Builders preserve publication state/stage/path diagnostics in their existing build error. New generated-source authoring uses the Framework create transaction; existing authoring updates retain their prior atomic source-write behavior.
- Engine fallback material resolution is a pure registered-identity query. The regression covers unregistered failure without side effects, Project overlay isolation, and an already registered Engine identity without touching its source file or manifest.

Transaction continuation: the full default-target build passed; FrameworkTest 350, EditorCoreTest 209, EditorShellTest 168 and Aurora.CookTest 67 passed (794 total). The source-ownership checker ran 26 cases with one platform-dependent skip. Subsequent source-capture changes are verified separately below. Native screenshots above belong to the initial batch; they are not a fresh renderer/performance acceptance result for this continuation.

## Source capture continuation

- Source snapshots copy fields and retain the owning filesystem under the asset mutex. Source path/type queries, cook metadata access and build request preparation use those values. Snapshot fields remain unchanged after move, metadata edits, removal and filesystem teardown.
- Registration releases the asset mutex before opening its cook input and queueing work; a regression probes concurrent source reads during that input-open callback.
- This is a foundation for B3, not completed versioning or lock separation. Directory I/O in mutations/scans still holds the global asset mutex; source bytes/settings can still change between capture and publication. The cook coalescing key is intentionally unchanged until publication guards are implemented.

Final source-capture acceptance: the full default-target Debug build passed. FrameworkTest **352**, EditorCoreTest **209**, EditorShellTest **168**, Aurora.CookTest **67** passed: **796 tests**, no failures or skips in these four suites. OpenSpec strict validation, whitespace and changed-source ASCII checks passed. Logs are retained under `cmake-build-debug/asset-cache-validation/transactions/`. No commit or archive was performed.
