## ADDED Requirements

### Requirement: Standard Android Back routing

The Android launcher SHALL route a valid legacy Back release and API 33+ system Back invocation through the existing standard Back dispatcher and platform Activity policy. It SHALL NOT force finish or process termination solely because Back was invoked. Non-Back keys SHALL retain their existing dispatch path.

#### Scenario: Valid legacy Back release

- **WHEN** a first Back key-down is followed by a tracked, uncanceled key-up
- **THEN** the launcher SHALL invoke the standard Back dispatcher exactly once
- **AND** repeated key-down events SHALL NOT invoke navigation

#### Scenario: Canceled or untracked release

- **WHEN** a Back key-up is canceled or has no tracked key-down
- **THEN** the launcher SHALL NOT invoke Back navigation

#### Scenario: API 33 system invocation

- **WHEN** the system commits a Back invocation on API 33 or later
- **THEN** the Activity-owned platform callback SHALL forward it to the same standard Back dispatcher
- **AND** the manifest SHALL enable that callback path
- **AND** Activity destruction SHALL unregister the exact callback before its owner is released

### Requirement: Back preserves the system Activity policy

The launcher SHALL preserve the standard dispatcher's actual system decision to background or finish the Activity. It SHALL NOT force either outcome solely to satisfy an assumed root-task policy. A background application SHALL pause game/frame updates, retain its existing runtime session, and resume through the normal lifecycle when foregrounded. A finished Activity SHALL follow ordered shutdown and lifecycle destruction. A surviving process SHALL NOT by itself be treated as an exit failure or proof of Activity survival.

#### Scenario: System Back backgrounds the application

- **WHEN** the standard Back dispatcher and platform policy background the Activity
- **THEN** the launcher SHALL follow the ordinary pause lifecycle
- **AND** it SHALL NOT convert that transition into a forced finish or process kill

#### Scenario: System Back finishes the Activity

- **WHEN** the standard Back dispatcher and platform policy finish the Activity
- **THEN** the launcher SHALL perform ordered application shutdown and lifecycle destruction
- **AND** the continued existence of the process SHALL NOT be treated as failure

#### Scenario: Return to the existing task

- **WHEN** the backgrounded task returns to the foreground without Activity destruction
- **THEN** its existing application session SHALL resume updates
- **AND** background elapsed time SHALL NOT be delivered as one accumulated simulation frame

#### Scenario: HOME backgrounds without requesting exit

- **WHEN** HOME backgrounds a running Activity without destroying it
- **THEN** game updates SHALL pause without application Shutdown
- **AND** returning to that Activity SHALL resume its existing runtime session

### Requirement: Explicit exit drains lifecycle through destruction

A business exit SHALL request Activity finish at most once and perform application Shutdown exactly once for its initialized runtime session. The Android host SHALL continue lifecycle dispatch until native DESTROY is observed, retaining callback owners for all commands in that interval. Duplicate exit requests and late commands SHALL NOT repeat shutdown.

#### Scenario: Exit before DESTROY arrives

- **WHEN** a running application requests exit before Android delivers native DESTROY
- **THEN** finish SHALL be requested once and application Shutdown SHALL run once
- **AND** lifecycle commands SHALL continue to be processed until DESTROY
- **AND** callbacks SHALL retain a valid owner throughout that drain

#### Scenario: Duplicate exit and late destruction

- **WHEN** another exit request or a late DESTROY follows an already-started exit
- **THEN** no additional finish request or application Shutdown SHALL occur
- **AND** the host SHALL release its lifecycle callback owner only after destruction is observed

### Requirement: Polling distinguishes renderer drain and native lifecycle wait

Ordinary paused/background lifecycle processing SHALL use WAIT behavior without advancing game simulation. While the renderer host waits for CPU/GPU shutdown to reach STOPPED, lifecycle pumping SHALL use NON_BLOCKING behavior so it can observe completion without another OS event or game update. After renderer/application shutdown completes, the native host MAY use normal WAIT(-1) until DESTROY; it SHALL continue dispatching and acknowledging lifecycle commands with a live callback owner.

#### Scenario: Ordinary background operation

- **WHEN** an application is backgrounded without an explicit exit request
- **THEN** lifecycle polling SHALL retain WAIT behavior
- **AND** game updates SHALL remain paused until normal resume

#### Scenario: Renderer completes without another OS event

- **WHEN** renderer shutdown is waiting for CPU/GPU completion and Android delivers no new lifecycle event
- **THEN** lifecycle pumping SHALL use NON_BLOCKING behavior so the host can observe renderer completion
- **AND** it SHALL NOT resume game updates to complete teardown

#### Scenario: Native host waits for late DESTROY

- **WHEN** renderer/application shutdown has completed but native DESTROY has not arrived
- **THEN** the native host MAY block in normal WAIT(-1) for lifecycle commands
- **AND** it SHALL continue processing and acknowledging those commands until DESTROY without requiring a busy polling loop

### Requirement: Same-process recreation preserves metadata and leases

Destroying and recreating an Android Activity in one process SHALL create a new runtime session while reusing existing process metadata definitions and their required code leases. The host SHALL NOT clear the shared Environment, re-reflect built-in types, or add duplicate-registration guards to permit recreation.

#### Scenario: Destroy and reopen in one process

- **WHEN** a launcher Activity is destroyed and a new launcher Activity is initialized in the same process
- **THEN** built-in and module metadata SHALL preserve their existing definition identity
- **AND** the new runtime session SHALL initialize without duplicate reflection
- **AND** retained definitions and callbacks SHALL keep the code leases they require

### Requirement: Overlapping Activities hand off engine session ownership

The official GameActivity runtime SHALL be the sole native Activity lifecycle owner. Before initializing a replacement NativeCode, it SHALL terminate and complete the previous native session, including application teardown and its shared text-input ownership. An obsolete Activity's late callbacks or destruction SHALL NOT act on the replacement native owner. The launcher SHALL NOT add a parallel Activity gate, lifecycle proxy or private vendor patch. Main-role ownership SHALL be released only after the previous application's pending completion notifications and final callbacks are collected.

#### Scenario: Clear-task starts a replacement Activity during destruction

- **WHEN** a replacement Activity starts while the previous native session is still DESTROYING
- **THEN** upstream GameActivity SHALL complete the previous native session before initializing replacement native state
- **AND** the replacement SHALL initialize without an already-active World provider or overwritten platform owner

#### Scenario: Obsolete Activity receives an insets callback

- **WHEN** a previously replaced Activity receives a late insets callback or onDestroy
- **THEN** the upstream native-owner check SHALL reject access to the replacement's NativeCode and shared text-input state
- **AND** the launcher SHALL NOT duplicate that protection with an old-version compatibility layer

### Requirement: Initialization failure retains late callback ownership

If application initialization fails, the Android host SHALL request platform closure without dropping the lifecycle callback owner before native DESTROY. Failure teardown SHALL release every initialized resource once and SHALL NOT require a successfully started game loop.

#### Scenario: Failed initialization followed by late DESTROY

- **WHEN** application initialization fails and Android later delivers pause, window or DESTROY commands
- **THEN** each command SHALL reach a live callback owner
- **AND** initialized resources SHALL be released once
- **AND** the host SHALL NOT wait for a game tick to complete failure teardown

### Requirement: Renderer terminal events remain observable during teardown

Renderer teardown SHALL continue event polling through a real empty result after the renderer reaches STOPPED. Successful internal CPU completions SHALL be consumed without producing a false empty public poll when a later public failure or stopped event is already available. This behavior SHALL use the existing private runtime/host boundaries without introducing a public test injection API.

#### Scenario: Success precedes the terminal event

- **WHEN** successful internal CPU completions precede a queued public failure or stopped event
- **THEN** polling SHALL continue consuming those internal completions until the public event or actual queue exhaustion
- **AND** the host SHALL observe the terminal event before completing its event drain

#### Scenario: Renderer state stops before the final poll

- **WHEN** renderer state is STOPPED while public teardown events remain queued
- **THEN** the host SHALL continue polling until the queue is actually empty
