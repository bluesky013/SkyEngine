# Common asynchronous IO implementation tasks

Status: detailed industry-backed design and adversarial source review completed on 2026-11-01. All unchecked tasks below are future implementation work. Implement design.md, io-contract.md and product-format.md together; primary-source evidence is in industry-references.md. Preserve existing directories/targets and the current flat-product ownership contract.

Common IO implementation awaits user review. Some baseline navigation fixes are supplied by the separate aurora-navigation-integration checkpoint and are marked below; they do not implement this proposal's IO service. Design-only checks and limitations are recorded in verification.md.

## 1. Correct the reviewed baseline

- [x] 1.1 Supplied by navigation checkpoint: validate tile/raw lengths before allocation, with huge-count/truncation cases.
- [ ] 1.2 Validate image slice count and encoded table size before allocation; add outer-valid malformed image products.
- [x] 1.3 Supplied by navigation checkpoint: freeze submitted filters, document world-thread service operations and test later edits.
- [x] 1.4 Supplied by navigation checkpoint: retain cancelled jobs in actual in-flight accounting and verify blocked-query admission.
- [x] 1.5 Supplied by navigation checkpoint: fresh-first/backoff retry, real pending coordinate count and permanent-failure fairness.

## 2. Core checked reads and common scheduler

- [ ] 2.1 Add checked read-handle/backend capabilities, distinguishing immutable-version and mutable content; implement Native and immutable Memory sources with explicit short-read/unsupported results.
- [ ] 2.2 Verify 64-bit ranges, allocation bounds and old-handle reads after native path replacement; characterize package-source capability explicitly.
- [ ] 2.3 Implement explicit service Init/Close/session ownership using the existing Core target and Environment-backed Singleton contract.
- [ ] 2.4 Implement the IO control thread, per-backend active slots and small blocking-worker backend, with bounded WDRR/priority/aging/segment admission and nonspinning dispatch.
- [ ] 2.5 Add immutable result buffers, retained byte/handle leases and exact-range sharing; test priority donation and last-cancel/new-join linearization across all sponsors.
- [ ] 2.6 Implement generic scope registration/work obligations, finite control/handle expansion credits, root Close and cancel gates, reusing Completion/ContinuationExecutor.
- [ ] 2.7 Reserve control/completion capacity before dispatch, coalesce updates and verify flood cannot suppress cancel/close/drain; distinguish WorkDrained, notifications drained and retained objects.
- [ ] 2.8 Test real cancellation/subscription races, segmented reads stopping after orphaning, retained results after Close/restart and a stalled backend without unsafe retirement.

## 3. Framework product and asset pipeline

- [ ] 3.1 Implement the specified 64-byte prefix, 20-byte dependency entries and 48-byte block entries, typed BlobRef slices and explicit old-format rejection.
- [ ] 3.2 Implement metadata-only validation, independent stored/decoded CRC32C and LZ4 blocks, all decode limits and explicit deep audit; test a large logical resource spanning multiple blocks.
- [ ] 3.3 Add stable AssetReadSnapshot and versioned chunk preparation/coalescing; test queued old reads during atomic product replacement.
- [ ] 3.4 Implement required-closure escrow before any member's bulk work, borrowing unit credits topologically; test input-full, dual-root pinned-dependency and partial-layer budget cycles.
- [ ] 3.5 Add bounded CPU TryDispatch/ready scheduling that also handles dependency-completion activation; test completion while the CPU queue is full.
- [ ] 3.6 Implement the serial AssetManager coordination loop and stable CurrentLoadKey/LoadOperation identities; move IO/decode outside loadMutex and preserve policy-isolated duplicate/reload behavior.
- [ ] 3.7 Expose request resolution, metadata readiness and typed CPU readiness distinctly; retain explicit synchronous tool wrappers and migrate runtime callers.
- [ ] 3.8 Extend existing Application/tool drain to root Close, accepted internal scopes and CPU/continuation completion before module unload.
- [ ] 3.9 Update shared product writers, cook/readback and package deep audit; recook fixtures and verify every supported source adapter.
- [ ] 3.10 Implement the shared phase-aware required graph, transactional cycle detection and finite closure expansion; test simultaneous reverse edges and reference-only cycles.
- [ ] 3.11 Separate device-independent PreparedBacking from per-consumer PublicationTicket; include complete compound input identities/recipe parameters in shared keys and test independent scene lifetimes.

## 4. Navigation as the first range consumer

- [ ] 4.1 Write a bounded navigation manifest mapping coordinate/layer BlobRefs to physical blocks with validated build/backend signatures; define region products for oversized manifests.
- [ ] 4.2 Implement world-owned tile state, desired-set hysteresis and session/instance/request-serial checks; retain useful requests during focus movement.
- [ ] 4.3 Prepare one full native candidate per world from frozen resident state plus coalesced deltas; reserve active/candidate/retired/scratch peak, rejecting impossible capacity explicitly.
- [ ] 4.4 Implement checked bundle TryPublish, base-revision verification and backend-wide query gates with post-lock recheck; preserve the old complete coordinate on failure and retire large old state off Tick.
- [ ] 4.5 Integrate an explicit production streaming-focus owner, read/prepare/ready/publish/unload budgets and truthful diagnostics.
- [ ] 4.6 Test real Recast with delayed/failed/out-of-order reads, direct and async queries, continuous focus changes, unload/reload/detach, failure gate cleanup and bounded complete-candidate memory; report remaining query/query serialization separately.

## 5. Scene assets, materials and shader artifacts

- [ ] 5.1 Migrate scene asset requests to nonblocking Framework admission and freeze immutable conversion inputs on the producer.
- [ ] 5.2 Run full image/mesh CPU conversion on bounded preparation workers; pass owned buffers/views into the existing uploader without redundant full-copy staging where its contract permits.
- [ ] 5.3 Prepare material/instance metadata and dependency recipes asynchronously, preserving parent/override/index/artifact closure identities and separate texture residency; package exact resource keys only on the producer.
- [ ] 5.4 Route shader index/blob bytes through Core IO; cache index by stable snapshot and coalesce blob preparation by artifact identity.
- [ ] 5.5 Move allowed development shader compilation to a bounded CPU/cook job while keeping device/cache mutations with their existing owner.
- [ ] 5.6 Reject late scene/material/shader completions by session, deviceEpoch, dependency identities and request serial; test reload, override, detach and device restart during preparation.
- [ ] 5.7 Verify existing ResourceUploader chunk budgets, GPU completion, resource pins and retirement remain authoritative; do not claim partial mip/LOD residency from this full-resource migration.

## 6. Verification and reporting

- [ ] 6.1 Build affected real Core/Framework/Recast/Aurora libraries and module entries as well as tests; rerun CMakeSourceOwnership whenever build organization changes.
- [ ] 6.2 Run deterministic saturation, shared-cancel, no-interest, shutdown/pump, stale-session and cross-stage-memory-progress tests with a controllable IO backend.
- [ ] 6.3 Run supported native stable-handle/atomic-replace tests and product recook/deep audit, including files above 2 GiB without giant test allocations.
- [ ] 6.4 Run mixed navigation, texture/mesh, material/shader demand and record submission latency, actual read bytes, IO/CPU/GPU readiness, frame percentiles and peak memory.
- [ ] 6.5 Compare whole-product loading and range reads on the same dataset; report outstanding synchronous compatibility paths and distinguish disk paging from complete-resource async upload.
- [ ] 6.6 Update verification evidence and review remaining domain work: partial mip/LOD GPU residency and independent-context/versioned Detour queries require their own implementation review before claims of support.
