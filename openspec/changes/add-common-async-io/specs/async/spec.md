## ADDED Requirements

### Requirement: Runtime loading has nonblocking admission

Framework SHALL provide a runtime asset entry point with explicit bounded admission. It SHALL schedule product byte IO through the shared Core service and run typed CPU decoding through the existing CPU executor infrastructure. Cache and in-flight locks SHALL NOT cover file open, reads, checksums, decoding or waits. Synchronous tool/compatibility entry points SHALL remain explicit and SHALL NOT be used by runtime producer or streaming tick paths.

#### Scenario: Concurrent loads of different products
- **WHEN** two products are requested while one read is deliberately blocked
- **THEN** the other request can be admitted and make progress without waiting for a global load lock held across the blocked read

#### Scenario: Duplicate request before metadata completes
- **WHEN** multiple callers request the same current product before its metadata completes
- **THEN** a registered in-flight operation serves them without duplicate physical loading or orphaned Loading assets

### Requirement: Shared readiness graph has stable selected identities

Framework SHALL distinguish CurrentLoadKey, selected LoadOperationId/instance and each consumer's publication serial. Normal duplicate lookups and dependency references SHALL join the selected operation; explicit reload SHALL advance the registry's selection epoch. Required edges SHALL target MetadataReady or CpuReady in one shared serialized graph, with insertion and cycle detection in the same transaction. Reference-only edges SHALL NOT create CPU waits. Node/edge/depth/control/handle expansion limits SHALL have explicit failure propagation to required parents.

#### Scenario: Concurrent roots discover a cycle
- **WHEN** roots for A and B concurrently discover required CpuReady edges A-to-B and B-to-A
- **THEN** the shared graph rejects the cycle and required parents receive terminal failure instead of creating fresh operation identities that evade cycle detection

#### Scenario: Reload while normal consumers join
- **WHEN** duplicate normal requests join a current load and a later explicit reload advances selection epoch
- **THEN** normal duplicates share the old selected operation, new selection uses a distinct operation, and old results cannot overwrite the new cache entry

### Requirement: Dependency activation does not block a worker

Accepted asset dependency state machines SHALL activate ready CPU work through bounded scheduling with explicit Busy handling. Dependencies SHALL NOT be awaited from their producing worker pool, and completion-triggered child activation SHALL NOT use a full-queue spinning Schedule path. Busy work SHALL remain owned for later activation or explicitly terminate on cancellation/shutdown.

#### Scenario: CPU queue full at dependency completion
- **WHEN** a dependency completes while the shared CPU queue is full
- **THEN** its dependent work remains pending under the scheduler's budget without blocking the completing worker, losing work or publishing the parent prematurely

### Requirement: Resolved identity is not loaded readiness

Asset request resolution SHALL mean only that an asset identity/result pointer has been resolved. Metadata readiness, typed CPU loaded status and consumer residency SHALL be tracked separately. Declared required CPU dependencies SHALL finish before typed CPU readiness; metadata dependencies on streamable resources SHALL NOT imply that every bulk chunk or GPU resource is resident.

#### Scenario: Resolved pointer before CPU decoding
- **WHEN** an asynchronous request resolves its AssetPtr while decoding is still running
- **THEN** consumers continue to observe pending CPU readiness and do not read unfinished typed data

### Requirement: Application drain covers the complete preparation chain

The existing application/tool shutdown owner SHALL close external asset roots, drain accepted IO scopes, CPU preparations and destination continuations, and preserve required handler/backend instances, buffers and service/data owners until actual work and owned cleanup have ended. Defining DLL code SHALL remain process-resident under the loader, independently of retained byte budgets and CPU/GPU resource lifetimes. It SHALL keep required destination pumps progressing rather than waiting on work that needs the blocked destination thread.

#### Scenario: Shutdown during decode and late IO completion
- **WHEN** shutdown races a blocked file read, queued decode and accepted main-thread completion
- **THEN** every accepted operation reaches one terminal state, all accepted notifications drain, and required handler/backend objects, result data and byte accounting remain owned through their actual cleanup while defining DLL code remains process-resident

### Requirement: Independently verifiable product chunks

Framework SHALL provide flat UUID products with bounded metadata and independently addressable, compressed and checked physical blocks. Typed logical resources SHALL be allowed to span multiple checked decoded-block slices rather than requiring one mip, LOD or tile layer to fit one physical block. Runtime metadata loading SHALL NOT require scanning all payload bytes or eagerly reading bulk. Metadata SHALL bind block identities, ranges, stored/decoded sizes, codecs and checksums. Counts, ranges, overlap, decoded sizes and format versions SHALL be validated before payload allocation. The v2 field encoding and support limits SHALL follow product-format.md. Old unsupported formats SHALL fail explicitly and require recooking.

#### Scenario: Metadata and one selected chunk
- **WHEN** a product contains many chunks and one chunk is requested
- **THEN** runtime reads and validates bounded metadata and that chunk without reading unrelated chunk payloads

#### Scenario: Large logical mip spans blocks
- **WHEN** a mip exceeds the supported decoded-block size
- **THEN** its BlobRef can reference multiple independently checked blocks while each read/decode stays within physical limits, and full-resource gather requires its own budget

#### Scenario: Small corrupt metadata with huge declarations
- **WHEN** an outer-valid product declares an excessive count, decoded size or overflowing range
- **THEN** decoding fails before allocating according to that declaration

#### Scenario: Full offline audit
- **WHEN** packaging or explicit deep inspection validates a product
- **THEN** every chunk is checked and an unread corrupted chunk causes audit failure even if metadata alone is valid

### Requirement: Immutable product read snapshots

Every range request SHALL retain the selected product's stable read handle, metadata and loaded-instance identity/revision. Reload SHALL publish a new instance without changing retained snapshots. Requests and cache entries SHALL NOT reopen a UUID path to satisfy reads from an older instance. Persistent product groups or generation directories SHALL NOT be introduced.

#### Scenario: Reload during a deferred read
- **WHEN** a new product replaces a UUID path before an old snapshot's queued read executes
- **THEN** the queued read returns old-instance bytes and the new instance independently returns new bytes

### Requirement: Bounded preparation makes progress

Preparation SHALL account for compressed input, decoded output, conversion/native output, scratch and retained buffers. After bounded metadata discovery and required-readiness graph validation, the scheduler SHALL reserve the required closure's conservative peak before any member starts bulk preparation. Units SHALL borrow credit from that escrow in dependency order. A multi-block publication unit SHALL have capacity to complete the entire unit; required handles and control expansion SHALL also be covered. A lowest required working set above hard capacity SHALL explicitly fail. Temporarily unavailable external credit SHALL remain observable and cancellable without occupying a worker. Accepted roots SHALL NOT pin partially prepared dependencies while mutually waiting for unreserved output memory.

#### Scenario: All inputs need output space
- **WHEN** admitted input reads complete and all preparations also require decoded output and scratch
- **THEN** at least one admitted unit can finish without circularly waiting for input memory to be released, and all hard limits remain satisfied

#### Scenario: Two roots pin separate dependencies
- **WHEN** H is 64 MiB and two roots each require a 20 MiB dependency followed by 30 MiB of preparation
- **THEN** closure escrow admits payload work for a complete feasible closure before partial dependencies of both roots can consume 40 MiB and block both parents

#### Scenario: Multiple coordinates with multiple layers
- **WHEN** small Ready capacity and out-of-order layer completion affect several navigation coordinates
- **THEN** admission guarantees completion of accepted whole-coordinate units rather than retaining only partial units that prevent every coordinate from advancing

### Requirement: Shared versioned chunk preparation

Framework SHALL coalesce compatible preparations by selected immutable snapshot identities, block/blob selection, representation recipe/version and complete output-affecting parameters. Compound results SHALL include the full input dependency-snapshot closure in their sharing key. Shared PreparedBacking SHALL contain device-independent immutable data; each consumer SHALL hold a distinct PublicationTicket carrying its owner session, device epoch where applicable, dependency identities and request serial. A consumer serial SHALL NOT distinguish the shared current-load lookup. The owning producer SHALL validate its ticket and change caches/active domain resources in one serialized transaction. Stale tickets SHALL release their own interest without invalidating other consumers or resurrecting detached, replaced or restarted state.

#### Scenario: Material override or renderer restart during preparation
- **WHEN** image, mesh or shader preparation completes after its material override, scene session or renderer device epoch changes
- **THEN** the producer rejects the obsolete result and does not update its active cache or resource bindings

#### Scenario: Shared bytes with independent owner sessions
- **WHEN** two scenes share compatible preparation and one detaches or restarts its renderer
- **THEN** only that scene's ticket becomes stale, the other scene can publish, and owner-specific resource keys are not reused across device epochs

#### Scenario: Moving navigation focus retains useful work
- **WHEN** focus changes but a requested tile remains in the desired set
- **THEN** its interest remains active, while work leaving the desired set becomes cancellable according to hysteresis and request identity

### Requirement: Preparation and publication have distinct owners

Workers SHALL consume immutable inputs and produce owned data without modifying live world, scene-cache, renderer-device or Detour state. The domain owner SHALL perform publication. A navigation coordinate SHALL publish all layers transactionally; preparation or native publication failure SHALL preserve the previous complete resident coordinate. Failed units SHALL enter explicit failure/retry state without starving healthy new work.

The initial full-native-candidate navigation strategy SHALL allow at most one candidate per world, account for active/candidate/retired state and scratch, and coalesce desired changes without invalidating candidates solely on focus movement. Tick publication SHALL try-acquire its backend write protection rather than wait for a query; publication-pending gates SHALL cover direct backend and asynchronous query entries, including rechecking after lock acquisition. Swapping SHALL replace the complete mesh/query/cache bundle. All failure/stale/cancel paths SHALL resolve the gate and retire large old states outside Tick.

#### Scenario: Native failure in a later layer
- **WHEN** all layers prepare but native installation of the coordinate cannot complete
- **THEN** the old complete coordinate remains queryable and no partially installed new coordinate is published

#### Scenario: Candidate cannot fit beside active state
- **WHEN** active native state is 100 MiB and another 100 MiB candidate is required under a 150 MiB hard limit
- **THEN** preparation reports CapacityExceeded and preserves active state rather than waiting indefinitely or exceeding the limit

#### Scenario: Queued direct query during publication
- **WHEN** a direct QueryPath caller passed an early gate check but obtains its mutex after publication-pending becomes true
- **THEN** it rechecks the gate and does not start another old-state search, while Tick either publishes with try-lock or returns pending without waiting

#### Scenario: Permanently bad nearest tile
- **WHEN** the nearest tile fails repeatedly while farther healthy tiles are desired
- **THEN** retry backoff/fairness allows healthy tiles to become resident without repeated global query cancellation caused solely by the bad tile

### Requirement: CPU readiness is distinct from domain residency

Resolved request identity, metadata readiness, typed CPU readiness and domain residency SHALL be observable as distinct stages. Image and mesh consumers SHALL send owned CPU payloads through Aurora's existing uploader, GPU completion and retirement protocol. Shader byte reads SHALL use the common byte service; decoding or development compilation SHALL use CPU preparation scheduling. Common IO completion SHALL NOT imply GPU residency, navigation publication, mip/LOD demand streaming or lock-free query safety.

#### Scenario: Bytes ready while GPU upload remains pending
- **WHEN** a scene image finishes reading and CPU conversion while upload is incomplete
- **THEN** the CPU stage reports ready but the renderer does not publish the resource as resident before its existing completion checks pass

### Requirement: Streaming diagnostics expose actual work

Consumers SHALL expose queued, physically active, ready, resident, failed and retained-byte measurements as applicable, with IO, preparation and publication timing separated. Cancellation SHALL NOT release actual in-flight accounting before work terminates. Mixed-load validation SHALL record producer submission latency and frame time independently of background throughput.

#### Scenario: Cancelled work still running
- **WHEN** a cancelled native read or navigation query remains executing
- **THEN** active work and retained memory still include it and new admission cannot bypass its configured capacity

### Requirement: Checked stable range sources

Core SHALL provide a stable read-handle abstraction and checked 64-bit positional reads independent of asset, renderer and plugin-domain types. Backend capabilities SHALL distinguish immutable-version content from mutable sources, along with alignment, transfer, execution, concurrency and cancellation capabilities. Open, range validation, short read, unsupported capability and native IO failure SHALL have explicit results. Range arithmetic and allocation limits SHALL be checked before allocating. An accepted open operation SHALL perform file open/stat outside the submitting producer thread. Stable object identity SHALL NOT be advertised as preservation of content against in-place writes.

#### Scenario: Large offset and invalid range
- **WHEN** a source supports a valid range above 2 GiB, or a caller submits an overflowing or out-of-bounds range
- **THEN** the valid request reads the specified bytes without narrowing the offset, while the invalid request fails explicitly without allocating its declared payload

#### Scenario: Replaced path
- **WHEN** a path is atomically replaced after a read handle is opened
- **THEN** reads on that handle continue to address the selected old object, or the source explicitly reports that this capability is unsupported before accepting snapshot reads

#### Scenario: Mutable file changes in place
- **WHEN** a mutable source is overwritten or truncated after a handle is opened
- **THEN** the service does not claim immutable old-content semantics, and expected-content validation failure invalidates the preparation instead of publishing new content under an old snapshot identity

### Requirement: Bounded nonblocking admission

The shared IO service SHALL bound queued requests, active native reads, live consumer interests, retained result bookkeeping, open handles and bytes. Submission SHALL return Accepted, Busy, Closed or an explicit invalid/unsupported/capacity status without file IO, waiting for capacity or spinning on a full execution queue. Rejected submissions SHALL preserve caller-owned inputs. Accepted work SHALL remain owned until one terminal completion, including cancellation/drain, and SHALL NOT be silently dropped while awaiting external capacity.

#### Scenario: Saturated service
- **WHEN** native reads are blocked and the configured request or byte capacity is exhausted
- **THEN** another submission returns Busy without waiting, no work is silently dropped, and admitted requests retain their resources

### Requirement: Priority and progress

The service SHALL support priority, consumer fairness and aging for admitted work. Shared queued operations SHALL inherit the highest live priority and earliest live deadline, recomputing them on attach, update and cancellation. Per-backend active operations SHALL be controlled separately from blocking-worker count. Deficit scheduling configuration SHALL permit at least one maximum supported segment to be dispatched. Deadlines SHALL be soft scheduling targets and SHALL NOT imply hard real-time completion or early release of in-use resources. CPU decoding and domain callbacks SHALL execute outside IO workers.

#### Scenario: Continuous high priority arrivals
- **WHEN** background reads are admitted before a continuous stream of higher-priority work
- **THEN** the configured fairness and aging policy allows admitted background work to advance while maintaining all capacity limits

#### Scenario: Critical consumer joins a background read
- **WHEN** a Critical consumer attaches to an already queued Background physical read
- **THEN** the shared operation is reprioritized for all eligible live sponsors without duplicate unbounded queue records, and cancellation of that consumer recomputes the remaining priority

### Requirement: Control operations retain progress capacity

Cancellation, close, native completion and CPU completion SHALL NOT require space in a saturated ordinary admission queue. Dispatch SHALL reserve its completion-record capacity before work starts. Control updates SHALL be coalesced within bounded accepted-ticket state. A root SHALL reserve finite control and required-handle expansion credits; internal expansion exceeding that plan SHALL explicitly fail rather than indefinitely hold parents while waiting for child capacity.

#### Scenario: Parents consume all ordinary capacity
- **WHEN** accepted parent scopes discover dependencies while ordinary request capacity is full
- **THEN** their reserved expansion credits allow bounded continuation or explicit CapacityExceeded failure, and cancellation/completion can still drain the system

### Requirement: Completion follows actual native work

Requests SHALL reuse the Core Completion protocol. Cancellation SHALL be a request, not proof that native work or reader ownership has ended. The service SHALL retain the handle, buffer and required backend instance/state until actual work exits and terminal publication occurs. The loader SHALL keep defining DLL code process-resident independently of these data owners; code residency SHALL NOT establish initialized service readiness. Short reads and cancellation SHALL NOT expose partially filled buffers as successful results.

#### Scenario: Cancellation during a noninterruptible read
- **WHEN** a running read is cancelled but the backend cannot interrupt it
- **THEN** its active slot and writable buffer remain owned until the read exits, after which one terminal result is published

### Requirement: Owned buffers retain byte accounting

Successful data SHALL be returned through immutable owned buffers or views. The byte-accounting lease and backend ownership state SHALL remain valid until the last retained view releases them, including after service Close. Physical backing shared by multiple views SHALL be charged once; decoded output and scratch SHALL be separately accounted for by their preparing owner.

#### Scenario: Retained view after completion and close
- **WHEN** a consumer retains a result view after native completion and service close
- **THEN** its bytes remain readable and charged, and neither its budget state nor owned backend state references a destroyed service object

### Requirement: Shared reads preserve consumer interests

Exact duplicate handle/range reads SHALL support sharing one physical read. Each consumer SHALL retain independent interest and cancellation state; cancelling one consumer SHALL NOT cancel another consumer's needed work or modify an already published shared result. Physical resources SHALL remain owned until actual shared work has ended.

The final-interest cancellation, closing the join gate and removal from the coalescing table SHALL be one serialized transition. An operation closed to joins SHALL NOT be resurrected; a later request SHALL use a new operation identity. No new segment SHALL issue after final-interest cancellation, while already issued segments SHALL drain.

#### Scenario: One of two consumers cancels
- **WHEN** two consumers request the same handle/range and one cancels
- **THEN** one physical read serves the remaining consumer successfully and all consumers eventually observe their own terminal outcome

#### Scenario: Last cancellation races a new join
- **WHEN** the last live consumer cancels concurrently with a new identical request
- **THEN** the join is either registered before closure or receives a new operation, with no lost completion, revived cancelling operation or reuse of a still-written buffer

### Requirement: Explicit continuation destination

External continuations SHALL use the existing destination-executor subscription protocol, including capacity reservation, Busy behavior and single delivery. Accepted callback data and destination-executor instances SHALL remain owned through notification drain, while the loader keeps defining DLL code process-resident. IO completion SHALL NOT execute world, UI or renderer mutations inline on an IO worker. Polling SHALL remain supported without requiring a continuation.

#### Scenario: Completion races subscription
- **WHEN** a read completes concurrently with an accepted subscription
- **THEN** the callback runs once on the selected executor and observes the same immutable published result

### Requirement: Close drains accepted scopes

The process-shared service SHALL have explicit initialization, root-admission close and drain behavior. Closing root admission SHALL reject new roots while allowing bounded internal work of accepted scopes, or explicitly cancelling those scopes and completing their parent state machines. Child registration and scope cancellation SHALL be serialized; registration leases SHALL account for accepted producers that can still add children. Generic CPU/owner work obligations SHALL explicitly complete once after actual work exits. Runtime shutdown SHALL withdraw services and drain started work and accepted continuations before destroying their actual data, backend and destination-executor owners. Defining DLL code SHALL remain resident until process exit; binary replacement or removal SHALL take effect after process restart. Session identity SHALL reject old completions after restart.

WorkDrained SHALL mean all work and registration obligations have ended and terminal completion can publish. Notification delivery and retained memory SHALL remain separately observable; completion publication SHALL NOT depend on delivery of callbacks that publication itself enables.

#### Scenario: Close between metadata and dependent reads
- **WHEN** shutdown begins after metadata read but before an accepted scope schedules dependencies
- **THEN** the scope either completes its bounded internal work or terminates explicitly as cancelled/failed, with no indefinitely pending parent or lost notification

#### Scenario: Cancellation races dependency registration
- **WHEN** cancellation races a parser registering its next required child
- **THEN** cancellation rejects later registration, or the registered child is counted and drained, and the parent cannot publish while a registration lease can still create accepted work

#### Scenario: Restart with retained old results
- **WHEN** a new service session starts while external code still retains old completed buffers
- **THEN** those buffers remain valid but their old-session work cannot enter the new session's queues or consumer state
