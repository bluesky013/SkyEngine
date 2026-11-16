## Context

This is the approved A/B correctness and performance follow-up to fix-asset-reference-boundaries, with the subsequently approved C/D/E editor, payload and command-lifetime work. Acceptance is tracked separately from implementation.

## Goals / Non-Goals

Goals: stable identity, immutable readers, bounded directory I/O, explicit commit/recovery results and reliable cook generation checks. No target reorganization, asset relocation, exception-based errors or new generic infrastructure layer.

## Decisions

1. Both AssetIndexFileCache instances return shared immutable snapshots. Filesystem instance and normalized directory identify a cache slot; the slot retains its filesystem. A per-slot mutex serializes loading and publication, outside the cache-map mutex. Reload reads without publishing. Save publishes only after successful atomic replacement. This initial serialized implementation avoids an optimistic reader publishing stale data; future optimistic reads must compare their starting publication generation. Clear discards idle slots and invalidates active slots without detaching their serialization point. Invalidate discards the published snapshot. Active slots retain their filesystem; remounting creates a separate filesystem identity.
2. Snapshot entry views remain valid while their snapshot is retained. Mutable candidate Set/Remove invalidate borrowed views. Partial parsing records physical line numbers, isolates all duplicate-key/identity participants, sorts once, and marks the result incomplete. Incomplete candidates cannot be saved. Strict parsing preserves the old output on failure.
3. Source manifest is the identity commit point. Generated catalog repair can replay that manifest; product index repair additionally requires validated products. CommittedNeedsRepair is not an instruction to erase committed source state. Compensation failures retain stage/path diagnostics. Cross-file moves are recoverable operations, not crash-atomic transactions.
4. Scan reports distinguish errors, warnings and blocked (mount,directory,reason,recoverable) entries. Valid manifest identities supersede stale assets.db records. Cache identities only seed missing entries in healthy directories. Read-only and blocked directories are never written by scans or migration.
5. Directory scanning performs a planning read and at most one authoritative locked reread and one necessary save. SaveAtomic provides whole-file replacement, not freshness. Batch parsing builds lookup sets and sorts once. RegisterAsset skips generated refresh only when both persisted and previously published child sets are empty.
6. Lock reduction follows the snapshot/transaction migration. Capture immutable request fields under memory protection; serialize directories, commit files, release file locks, publish memory, then submit cook. No assetMutex/file-lock mutual waiting. Busy retries have deadlines and cancellation. Cook coalescing and product replacement validate source incarnation and relevant content/settings versions, not unrelated directory changes. Every subscriber completes, including stale/cancelled requests. Dependency publication also checks the captured source incarnation under writer/memory serialization.
7. Destination validation returns mount, filesystem and canonical relative path. Same-mount collision fails; cross-mount overlay remains supported. Duplicate prepares metadata before creating the destination and avoids partially registered intermediate steps. Uncommitted cleanup and committed repair are separate results.
8. AllowCook selects the source overlay before products. ProductsOnly uses published products; a newly added uncooked Project source does not rewrite the old published snapshot. Publishing Project selects it; later Engine cooking cannot steal the path. UUID product loading and explicit Engine source lookup remain independent. Different UUIDs at a product path remain missing, not arbitrary selection.

## Risks / Trade-offs

- Per-slot serialization retains I/O blocking for readers of that slot. The separate writer mutex serializes mutations, while assetMutex no longer spans disk I/O. Independent writes remain serialized as an explicit first implementation trade-off.
- Damaged manifests remain read-only even if some entries parse. Repair is explicit; scans never invent replacement UUIDs for damaged identities.
- Product cleanup and callback results preserve committed repair states; consumers must not interpret every failure as rollback.
- Editor-shell, editor-core and aurora-renderer deltas cover the implemented follow-ups; directory/target proposals remain separately reviewed.

## Implemented follow-up scope

C: surface interaction scopes, complete tooltip invalidation, layout cache, limited packed-ID reuse, atomic SettingsTarget requests, registration handles, three-platform leave handling.

D: define cache-owned payload budget, share immutable storage before deduplicating accounting, retain full-clear accounting, support late preview providers and future-session defaults.

E: document-session/world-runtime/actor/component incarnation validation before command execution and dirty callbacks; preserve valid undo detach/reattach. Assess Aurora.Assets include isolation separately, then reassess ResolveCached only if repetition remains.

## Transaction implementation follow-up

`SourceManifestTransaction` is private to the existing Framework implementation. It owns a directory lock and a checked, unpublished candidate; commit atomically saves and publishes through the existing cache. Multi-directory moves acquire these transactions in lexical order. No new target or generic transaction subsystem is introduced. Source writers now use a separate serialization mutex; memory protection is limited to publication.

Source mutation results carry commit state, the primary error stage, and all recovery paths. Deletion commits when its manifest entry is removed; failed staged-byte cleanup leaves the identity deleted and reports repair. A failed pre-commit rename compensation or cross-directory move compensation reports RollbackFailed and withdraws the stale source from the in-memory catalog. Generated-catalog failure after manifest commit returns committed metadata and CommittedNeedsRepair; RefreshOwner replays it after the conflict is resolved.

Atomic JSON writes clean their owned temporary file after a failed replacement and report cleanup failures. A failed exclusive write does not prove ownership of a surviving path; that path is reported without deleting it. Products are derived data: partial batch replacement invalidates the affected products and requires recook; complete validated replacement followed by index failure retains UUID-addressable products and reports CommittedNeedsRepair. Cook/package callers preserve recovery diagnostics and still report failure. Generated-source creation uses the same checked create/manifest transaction as imports, avoiding a redundant registration/persistence/compensation chain.

## Source capture foundation

`CaptureSource` copies source fields and retains the owning filesystem in one asset-mutex scope. Cook submission, source path queries, input opening and cook metadata reads use these captured values. `Gather` and enumeration return copied source fields. Registration releases its own asset lock before opening the cook input and queueing work; the path-only overload no longer retains an outer recursive lock across that submission.

Cook capture now owns the input bytes and retains a source publication guard. The guard validates source incarnation, persisted identity/cook settings and byte fingerprint under directory serialization. Configuration guards retain their mutex, generation and declared preset-file fingerprint through product publication. Builders declare configuration files alongside their existing LoadConfig implementation. Coalescing includes content/settings fingerprints and local incarnations. Generated metadata validation runs inside its existing source transaction; product publication holds the guard through replacement and index save. Worker protocol v2 carries the fingerprint and returns the actual completion result. The stable fingerprint uses resolved target/bundle/settings; the host active-platform selector only chooses targets and remains covered by local configuration generation, so workers need no matching UI selection state.

Writers are initially serialized globally by a separate shared recursive mutex; readers still use only assetMutex. This deliberately preserves write ordering while removing reader blocking on I/O. File-lock retries cap at 250 ms and observe Reset cancellation. Product-index selection captures catalog information before file locks, rereads the product index under product.lock, and preserves concurrently changed or unknown overlay mappings. Source writers and authoring services cooperate via the manifest lock. Arbitrary external programs that ignore that lock cannot be made transactionally atomic; input bytes and declared configuration files are revalidated before publication, and worker sources are rescanned only when the requested identity/path is missing.

Tooltip scopes reuse existing tree layout/paint generations as conservative layout and occlusion signals; visibility, child ordering, clipping, geometry and render transforms all propagate paint revisions. The shell supplies modal/menu interaction roots, router capture or asset-viewer capture and theme revision. Stable built targets skip hit testing. Pointer movement and input cancellation invalidate the cache; paint revalidates the current scope. Text layout is cached by target text, font, atlas, wrap width and theme revision.

SettingsTarget is one request containing registry, persistence, title and onApplied. PreferencesDialog queues the last complete request during reentrant edits and immediately cancels the replaced onApplied callback. Teardown submits an empty target. The legacy field setters remain for standalone dialog consumers; shell switching uses only the atomic API.

SceneMeshResources and material dependencies retain shared const payloads. Geometry conversion is cached independently from material overrides; image keys include backend, device epoch and usage. Each owning payload cache charges bytes at publication; shared shader artifacts are counted once across retained material payloads. Refresh clears all payloads and the accounting ledger. External retained snapshots, loader assets, compiler jobs and renderer packet copies are outside this cache budget.

World commands retain a weak document session plus editor-binding lifetime, exact runtime World identity, Actor incarnation and component type/incarnation. These checks precede nested commands and dirty callbacks. Retained detached actors/components preserve their incarnation for valid undo/redo, while replacements receive new identities. Aurora skeletal commands enforce their own world/actor/component identity inside the document-scoped outer command. See architecture-follow-up.md for deferred target/include changes and the ResolveCached decision.
