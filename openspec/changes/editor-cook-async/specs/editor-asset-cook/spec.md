## ADDED Requirements

### Requirement: Editor cook requests are deferred and deduplicated

Editor-initiated cooking SHALL NOT run inside the pointer/key/text event dispatch. Triggers SHALL
only enqueue a request; the queue SHALL be drained at a safe point in the frame (the editor tick, on
the main thread). Repeated requests for the same asset SHALL be coalesced into a single build.

#### Scenario: Trigger from an event returns immediately
- **WHEN** the user Saves / Closes an asset or picks `Assets > Cook`, inside the event handler
- **THEN** the request SHALL only be enqueued and the handler SHALL return without running any cook

#### Scenario: Drain once per frame
- **WHEN** the editor ticks
- **THEN** the pending cook requests SHALL be submitted on the main thread, once per frame

#### Scenario: Duplicate requests coalesce
- **WHEN** the same asset is requested more than once before the next drain
- **THEN** exactly one build SHALL be submitted

### Requirement: Cooking never blocks the UI thread

The editor SHALL NOT block the main thread waiting for a cook. In-process cook builds SHALL run on a
dedicated cook pool, and the UI SHALL surface cook state without a blocking wait and refresh
locally on completion.

#### Scenario: No blocking wait
- **WHEN** a cook is in flight
- **THEN** the main thread SHALL continue ticking and drawing (no `WaitForAll` on the UI thread)

#### Scenario: Local refresh on completion
- **WHEN** a cook completes
- **THEN** the editor SHALL clear the cook state and refresh locally, without a full shell rebuild
