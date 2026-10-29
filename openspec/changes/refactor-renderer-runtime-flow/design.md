## Context

The existing ECS pipeline is implemented across `CpuRenderCoordinator`, `CpuSceneStore`, `PreparedScene` and `ScenePrepareScheduler`. `RendererRuntime` implements the coordinator hooks; it does not replace the scheduler. Its current long inline functions hide this division of responsibilities.

## Goals / Non-Goals

Goals: expose the ECS lifecycle at the runtime entry points, isolate resource/output/GPU orchestration, and preserve observable rendering and completion behavior.

Non-goals: new systems or scheduling frameworks, moving files or targets, changing component schemas, render algorithms or public APIs, and changing the existing scene/view chunk partitioning.

## ECS lifecycle and ownership

1. The host transfers an owned `FramePacket` to `CpuRenderCoordinator`.
2. The coordinator validates and applies packet and deferred commands through `CpuSceneStore::Apply`. This is the structural ECS mutation boundary.
3. `RendererRuntime::Applied` synchronizes resources and output metadata on that same coordinator thread, before the CPU store freezes. Reading mesh/material bindings here establishes catalog subscriptions; it does not compute derived components or extract draws. The subscription set includes selected scenes and changed scenes with suspended outputs.
4. `CpuSceneStore::Freeze` pins ECS storage. `RendererRuntime::Build` calls `BuildPreparedSceneProgram` with CPU resource metadata, without RHI resource backings.
5. For each scene, `PrepareRenderObjects` declares reads of transform, mesh/material bindings, visibility and properties, and writes derived `RenderObjectData`. Resource-version changes participate in invalidation. Each `ExtractView` declares a read of that scene's derived data and an explicit dependency on its producer. The scheduler executes partitioned kernels and publishes staged writes and products only under its existing completion contract.
6. The coordinator waits for preparation and then `WaitUntilUnfrozen`, including captured read views, before calling `Prepared` or applying another packet. Cancellation alone is not completion.
7. `CollectPreparedScene` turns successful ECS products into an owned `PreparedFrame`. The runtime attaches resource pins, output versions and feature leases and queues this as a `PreparedRenderFrame`. GPU stages consume this snapshot; they never read or mutate the ECS registry.

Failure isolation remains scene-scoped where the current system identifiers permit recovery. Failed preparation must not queue partial scene products. Shutdown must wait for CPU borrowers before runtime teardown.

## Runtime organization

Keep the public coordinator hooks first, with short phase calls for `Applied`, `Prepared` and `Pump`. Keep detailed private methods out of the class body, ordered by lifecycle. `Build` and `RecoverPrepareFailure` retain their direct relationship to the ECS program.

Packet stages: apply feature/output/retirement effects; synchronize ECS resource subscriptions; consume CPU resource payloads; freeze resource dependencies; publish/prune catalog state; snapshot output surfaces before ECS freezing.

GPU progress stages: poll uploader completions and forward them to the scheduler; pump output progress; collect/submit encoded frames; publish/prune resources; start one pending graph when encoding capacity permits; acknowledge retirements. The uploader remains the sole GPU completion poller.

Graph stages: prepare features; admit uploads and resolve graph-local resources; acquire needed outputs; import resource images and output images; build feature passes from prepared views and extensions; export/retain outputs; append readbacks; retain pins; freeze, compile and dispatch. A private stack-local graph state groups temporary handles and borrowers, with no new service, queue or ownership authority.

Graph-stage errors return `FrameError` to one cleanup boundary. That boundary releases acquired borrowers and resets the builder before abandoning output/slot ownership. A failed view feature clears the composer's pointer to the temporary prepared view before returning. The no-work path retains its explicit completion and release behavior. `ResourceUploader::Prepare` returns an invalid batch ID when nothing is admitted; there is no batch to discard. Remove the previous invalid `Discard` call instead of ignoring its error. Real batch discard results remain checked in cleanup.

## Risks and validation

- Moving statements across phase boundaries could change resource readiness or suspended-output behavior: preserve statement order and run resource/viewport GPU regressions.
- Temporary graph borrowers must drain before abandonment: retain centralized cleanup and run encode, failure and retirement coverage.
- ECS publication and borrowing must remain intact: run coordinator, write-view, parallel/chunk prepare and prepared-scene tests, including producer-before-consumer, dirty/resource invalidation and retained-reader tests.
- Build the real Aurora core/runtime targets and native GPU test target using the existing VS 2022 cache. No CMake source organization changes are intended.

The source-level review compares this refactor against the saved working baseline, separately from earlier Party implementation changes. Neither change is archived or committed without the user's required review.
