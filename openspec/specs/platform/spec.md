# platform Specification

## Purpose

Define native platform services, window/input lifecycle, clocks and process execution.

## Scope notes

### linux-platform-backend

Minimal Linux build/platform backend so `Core`, `Framework`, and the `AssetTool` worker
configure, build, and start (handshake) on Linux. Real Linux cook is deferred until the
builder modules build on Linux.

### native-process

Child-process spawn with piped stdio and lifecycle control (wait/timeout/kill) on desktop
platforms, used by the out-of-process asset cook worker. Unsupported on mobile.

## Requirements

### Requirement: Linux build configuration

The build SHALL configure a Linux target: a Linux branch in the platform configuration and in the `Core` and `Framework` CMake projects, with `Core` reusing the existing Unix async backend and `Framework` compiling the shared POSIX process backend plus a Linux platform backend.

#### Scenario: Configure and build the worker subset

- **WHEN** the project is configured and built on Linux
- **THEN** `Core`, `Framework`, and the `AssetTool` worker SHALL compile and link, and the worker SHALL start and complete the protocol handshake

#### Scenario: Non-worker modules skipped

- **WHEN** the project is configured on Linux
- **THEN** modules outside the worker subset (render, aurora, editor, sandbox, and the rest) SHALL NOT be required to build

#### Scenario: Real cook deferred until builders land

- **WHEN** a cook is requested on Linux, where the asset builder modules are not built
- **THEN** the request SHALL fail observably (never a false success or a stranded load); enabling real Linux cook SHALL require the builder modules and their dependencies to build on Linux in a follow-up

### Requirement: Linux platform identity and paths

The Linux platform backend SHALL report `PlatformType::Linux` and SHALL provide internal and bundle paths derived from the running executable, and environment-variable lookup, without creating a window.

#### Scenario: Platform reports Linux

- **WHEN** the platform is initialized on Linux
- **THEN** `GetType()` SHALL return Linux and the internal/bundle paths SHALL be resolvable from the executable location

#### Scenario: Headless initialization

- **WHEN** the platform is initialized on Linux with no display available
- **THEN** it SHALL succeed without creating a window, so the batch/standalone worker can run

### Requirement: Linux priority and scope

Windows and macOS SHALL land before the Linux backend, which SHALL be tracked as lower priority; the Linux deliverable SHALL be the worker building and completing the protocol handshake (real cook deferred until builders build on Linux), and a windowed Linux engine or editor SHALL be out of scope.

#### Scenario: Linux tasks are separable

- **WHEN** the Linux backend tasks are deferred or dropped
- **THEN** the Windows and macOS process/cook functionality SHALL remain complete and unaffected

#### Scenario: Shared POSIX source

- **WHEN** the POSIX process backend is built for Linux
- **THEN** it SHALL be the same source used for macOS, with no Linux-specific fork of the spawn/pipe logic

### Requirement: Native window backend behind unchanged interfaces

The window/event layer SHALL be implemented with native OS APIs behind the unchanged `Platform` and
`NativeWindow` interfaces: Win32 on Windows and Cocoa on macOS. It SHALL NOT depend on SDL. Code above the seam
(launcher, editor host, runtime) SHALL NOT require changes.

#### Scenario: Build and run without SDL
- **WHEN** the project is built for Windows or macOS
- **THEN** the window/event backend SHALL use Win32/Cocoa and SHALL NOT link SDL

#### Scenario: Interfaces unchanged
- **WHEN** an existing host (launcher, editor host) is built against the native backend
- **THEN** it SHALL compile and behave the same without source changes

### Requirement: Multiple windows

The backend SHALL support creating more than one window in a process, each with its own native handle and event
stream.

#### Scenario: Second window
- **WHEN** a second `NativeWindow` is created
- **THEN** it SHALL be created successfully and its events SHALL be dispatched to the correct window

### Requirement: Event loop and event mapping

The backend SHALL own the message/event loop and SHALL map native events to the engine events: window
resize/focus/close to `IWindowEvent`, and key down/up and text to `IKeyboardEvent`.

#### Scenario: Resize event
- **WHEN** a window is resized
- **THEN** an `IWindowEvent::OnWindowResize` SHALL be broadcast with the new size and window id

#### Scenario: Key and text events
- **WHEN** the user presses a key or enters text
- **THEN** `IKeyboardEvent::OnKeyDown`/`OnKeyUp` and `OnTextInput` SHALL be broadcast

#### Scenario: Close event before destruction
- **WHEN** a window receives a close request
- **THEN** an `IWindowEvent::OnWindowClose` SHALL be broadcast for that window before it is destroyed

#### Scenario: Closing the main window exits
- **WHEN** the main window receives a close request
- **THEN** the backend SHALL post a quit message so the message loop exits

#### Scenario: Closing a secondary window does not exit
- **WHEN** a secondary window (not the main window) receives a close request
- **THEN** only that window SHALL be destroyed and the application SHALL keep running

### Requirement: Native handle for the RHI

`NativeWindow::GetNativeHandle()` SHALL return the platform handle used by the RHI to create its surface
(`HWND` on Windows, the window's `CAMetalLayer` on macOS — the Metal swapchain and the MoltenVK
`VK_EXT_metal_surface` path both consume the layer directly).

#### Scenario: Surface from handle
- **WHEN** the RHI creates a swapchain for a window
- **THEN** it SHALL use the handle from `GetNativeHandle()` unchanged

#### Scenario: macOS layer handle
- **WHEN** a `NativeWindow` is created on macOS
- **THEN** its content view SHALL be backed by a `CAMetalLayer` and `GetNativeHandle()` SHALL return that layer

### Requirement: Clipboard and timing

The platform SHALL provide clipboard text access and high-resolution timing without SDL.

#### Scenario: Clipboard round-trip
- **WHEN** text is written to the clipboard and read back
- **THEN** the same text SHALL be returned

### Requirement: Text input and IME

The backend SHALL provide text input. macOS SHALL provide IME (composition) input for CJK by implementing
`NSTextInputClient`; committed text (including an IME commit) SHALL be delivered through `OnTextInput`, while
in-flight composition (marked text) SHALL NOT be broadcast (the event interface has only `OnTextInput`). Any
in-flight composition SHALL be discarded when the window loses focus. Windows IME SHALL be provided in a later
phase; until then, plain text input SHALL work on Windows.

#### Scenario: Plain text input
- **WHEN** the user types ASCII text into a focused window
- **THEN** `OnTextInput` SHALL be broadcast with the entered text

#### Scenario: Command chords do not produce text
- **WHEN** the user presses a Command-modified key (e.g. Cmd+C)
- **THEN** no `OnTextInput` SHALL be broadcast for it

#### Scenario: CJK composition (macOS)
- **WHEN** the user composes CJK text via the system IME on macOS
- **THEN** the committed text SHALL be delivered through `OnTextInput`, and the composition SHALL be dropped if the window loses focus before commit

### Requirement: Window DPI scale

`NativeWindow::GetDpiScale()` SHALL return the window's device-pixels-per-point ratio and SHALL reflect
the display the window currently occupies (Windows: per-monitor DPI; macOS: the window's
`backingScaleFactor`), so consumers such as the sandbox editor viewport receive the correct scale.

#### Scenario: Retina window reports 2.0
- **WHEN** a `NativeWindow` is created on a 2x display on macOS
- **THEN** `GetDpiScale()` SHALL return `2.0f` (not the `1.0f` default)

#### Scenario: Scale follows the display
- **WHEN** the window moves to a display with a different backing scale
- **THEN** `GetDpiScale()` SHALL subsequently report the new scale without recreating the window

### Requirement: Reveal a path in the OS file manager

The platform layer SHALL expose a native shell service to reveal a file path in the operating
system's file manager, implemented per desktop backend and defaulting to a no-op where unsupported.

#### Scenario: Desktop reveal
- **WHEN** a desktop host asks the platform to reveal an existing path
- **THEN** the OS file manager SHALL open with that path selected

#### Scenario: Unsupported backend
- **WHEN** a backend does not implement the service (e.g. mobile)
- **THEN** the call SHALL do nothing and SHALL NOT fail

#### Scenario: Interface home
- **WHEN** the service is declared
- **THEN** it SHALL live on `PlatformBase`/`Platform` alongside the other native shell services
  (open/save dialogs), so hosts do not call OS APIs directly

### Requirement: Native platform window service parity

Cocoa, Android and UIKit windows SHALL register valid WindowIDs and report the current client pixel size. Cocoa SHALL identify the main window, update its title and geometry, provide cursor services, and publish close notifications before owner destruction. Mobile windows SHALL unregister and detach native callbacks on destruction.

#### Scenario: Main window lookup and title
- **WHEN** the first Cocoa window is registered and its title is changed
- **THEN** NativeWindowManager SHALL find it as the main window and the native title SHALL update

#### Scenario: Close a Cocoa secondary window
- **WHEN** a Cocoa secondary window receives a close request
- **THEN** its keyed close event SHALL fire once without exiting the main application or accessing an owner deleted by a listener

#### Scenario: Native input after owner destruction
- **WHEN** a retained native Cocoa view receives an input or text callback after its C++ window owner is destroyed
- **THEN** the callback SHALL be inert and SHALL NOT dereference the former owner

#### Scenario: Native mobile window lifetime
- **WHEN** an Android or UIKit engine window is created and destroyed
- **THEN** manager lookup SHALL succeed only during its lifetime and native callbacks SHALL NOT retain a dangling owner

### Requirement: Cocoa wheel and native shell services

Cocoa SHALL publish mouse wheel cursor position and fractional delta using the shared event contract, convert text-input rectangles from pixels to native points, and implement the existing desktop file reveal service.

#### Scenario: Trackpad scrolling
- **WHEN** a precise scrolling event has a fractional delta over a Cocoa view
- **THEN** the event SHALL carry the cursor's client pixel coordinates and retain the fractional scrolling delta

#### Scenario: Pixel text input anchor
- **WHEN** the host supplies a framebuffer pixel text-input rectangle on a Retina window
- **THEN** the native IME candidate rectangle SHALL use the corresponding point-space position

### Requirement: Mobile activity and native surface lifecycle

The platform SHALL stop application ticks while inactive or without a live native window, while continuing to receive native lifecycle events. Native handle loss/replacement SHALL preserve WindowID and notify keyed window listeners. Explicit application exit SHALL stop an externally owned frame loop.

#### Scenario: Background and resume
- **WHEN** a running mobile app loses activity and later resumes
- **THEN** ticks SHALL pause and resume with a fresh clock sample that excludes background time

#### Scenario: Android surface recreation
- **WHEN** the Android native window is destroyed and a replacement is supplied
- **THEN** the engine window SHALL retain its WindowID, publish unavailable/replacement handle notifications, and gate ticks until the platform is active with a live surface

#### Scenario: Exit with an external loop
- **WHEN** the application requests exit during an iOS frame callback
- **THEN** the display link SHALL stop and SHALL NOT call a destroyed application

### Requirement: UIKit scene-based lifecycle

The iOS launcher and native test host SHALL declare a single application scene and use UIWindowSceneDelegate. Native windows SHALL bind to an explicitly connected UIWindowScene and derive geometry from that scene's UI. Scene activity SHALL be filtered by the connected scene identity. UIApplication-wide activity SHALL NOT resume an inactive/disconnected scene.

#### Scenario: Scene connection precedes UI startup
- **WHEN** UIKit connects the configured application scene
- **THEN** the host SHALL bind that scene before creating its engine window and SHALL initialize GameApplication at most once per process

#### Scenario: Scene disconnect and reconnect
- **WHEN** the bound scene disconnects and a scene later reconnects
- **THEN** the UIKit representation SHALL detach safely, native handle notifications SHALL expose loss/replacement, and the engine window ID and GameApplication SHALL remain stable without repeating module initialization

#### Scenario: Foreign or stale scene lifecycle callback
- **WHEN** an activation or disconnection callback identifies a scene other than the connected scene
- **THEN** it SHALL NOT change window attachment, frame activity or the frame epoch

#### Scenario: Resume after scene replacement
- **WHEN** a disconnected scene reconnects and becomes active without an intervening inactive tick
- **THEN** a fresh frame epoch SHALL exclude the disconnection interval and an explicitly stopped frame loop SHALL remain stopped

#### Scenario: Native scene regression host
- **WHEN** the iOS native tests launch
- **THEN** UIKit SHALL connect a real UIWindowScene using the checked-in scene manifest before tests execute, and reports SHALL distinguish installed-runtime execution from unavailable iOS 27 validation

### Requirement: Portable input units and primary-touch mapping

Pointer coordinates and window client sizes SHALL use framebuffer pixels. Wheel deltas SHALL retain fractional precision; discrete wheel steps SHALL use 120 units. Mobile touch mapping SHALL track one primary contact and SHALL release it on cancellation or inactivity.

#### Scenario: Scaled pointer input
- **WHEN** native pointer input is expressed in points on a scaled view
- **THEN** button/motion coordinates and relative movement SHALL use the same pixel space as resize events

#### Scenario: Secondary touch and cancellation
- **WHEN** a secondary contact moves or the primary contact is cancelled
- **THEN** secondary input SHALL NOT synthesize duplicate primary button events and cancellation SHALL release the pressed button

#### Scenario: Text composition
- **WHEN** text is committed through a mobile soft keyboard
- **THEN** UTF-8 committed text SHALL reach OnTextInput and in-progress composition SHALL NOT be emitted as committed text

### Requirement: Platform clock and worker synchronization

Each supported platform SHALL provide a nonzero monotonic clock frequency. Application timing SHALL be per instance and SHALL preserve precision when subtracting large counters. The Android worker semaphore SHALL block until permits are available and SHALL retain counted permits.

#### Scenario: Variable frame rate
- **WHEN** two consecutive frames are sampled on a 30 Hz or 120 Hz cadence
- **THEN** application delta SHALL reflect elapsed time rather than always using 1/60

#### Scenario: Counted worker wakeup
- **WHEN** three workers wait on a zero-count semaphore and two permits are signaled
- **THEN** exactly two workers SHALL return until another permit is signaled

#### Scenario: Shutdown with queued work
- **WHEN** a worker has queued signal closures or an application still has asset tasks during teardown
- **THEN** the worker SHALL finish before its semaphore is destroyed and asset work SHALL finish before its owning services and resources are destroyed; defining module code SHALL remain process resident

### Requirement: Packaged startup configuration and honest services

Startup config SHALL prefer writable/project config and then read-only packaged config. Android packages SHALL include configs in their assets. A failed application initialization SHALL NOT start its tick loop. Unsupported mobile shell commands SHALL return failure.

#### Scenario: Fresh APK installation
- **WHEN** the writable directory contains no game config
- **THEN** startup SHALL read configs/modules_game.json from APK assets without requiring an extracted native file

#### Scenario: Project override
- **WHEN** writable/project config and packaged config both exist
- **THEN** the writable/project file SHALL take precedence without changing the writable filesystem

#### Scenario: Initialization failure
- **WHEN** mobile application initialization fails
- **THEN** the launcher SHALL NOT mark the application started or invoke its tick loop

### Requirement: Executable regression validation

The change SHALL include executable tests for the shared clock, counted synchronization, config precedence and mobile input/lifecycle policies, plus native Cocoa/UIKit coverage and mobile target compilation where SDKs are available. Validation reports SHALL distinguish executed tests from compile-only and manual-device cases.

#### Scenario: Portable tests on a desktop host
- **WHEN** CoreTest and FrameworkTest run without an Android/iOS device
- **THEN** the shared and mobile policy regression cases SHALL execute using the same policies used by native adapters

### Requirement: Reproducible native template package validation

One minimal generated game SHALL exercise the public Android APK, unsigned iOS IPA and macOS app packaging entry points. Preset native build overrides SHALL survive the first configure and reach the selected native build tool. Collected resources SHALL reside in the paths used by the platform bundle filesystem. Apple archives SHALL retain executable permissions and framework symbolic links.

#### Scenario: Generated Android native build
- **WHEN** the minimal project's Android export preset is generated into Gradle
- **THEN** the native build SHALL receive the selected engine path, module packaging, cache variables and compiler options without Groovy interpolation of path contents

#### Scenario: Unsigned iOS package collection
- **WHEN** an iOS device-SDK build completes with signing disabled
- **THEN** collection SHALL produce an explicitly unsigned IPA containing `Payload/<name>.app`, its executable, scene manifest, configs, products and required embedded frameworks; validation SHALL NOT claim ordinary device installation without valid signing

#### Scenario: Relocated macOS app
- **WHEN** the generated macOS app is moved away from its build output and launched from an unrelated working directory
- **THEN** its runtime module loading and bundled configuration SHALL resolve inside the app, and a bounded smoke invocation SHALL report initialization failure through a nonzero exit status

#### Scenario: Signed package request
- **WHEN** the resource collector is requested with Apple signing enabled
- **THEN** it SHALL fail before building or changing the bundle rather than emitting an archive with an invalidated signature

### Requirement: Child process launch

The platform layer SHALL provide a way to start a child process with an explicit argument vector, environment, and working directory, without going through a shell.

#### Scenario: Start a child with arguments

- **WHEN** the caller starts a process with an argument vector, environment, and working directory
- **THEN** the child SHALL run with exactly those arguments, environment, and working directory, and the caller SHALL receive a handle to its standard input, output, and error streams

#### Scenario: Missing executable fails fast

- **WHEN** the executable path does not exist or cannot be executed
- **THEN** the launch SHALL fail and return an error rather than start a partially initialized process

#### Scenario: Working directory set without unsafe fork work

- **WHEN** a working directory must be applied on a platform whose spawn API supports it
- **THEN** the launch SHALL set it through the spawn API; if a `fork`+`exec` fallback is used, only async-signal-safe operations SHALL run between `fork` and `exec` (pre-built argument/environment buffers, no allocation, no locks, no logging)

### Requirement: Standard stream pipes

The launched child's standard input, standard output, and standard error SHALL be connected to pipes owned by the parent, and the parent SHALL be able to write to stdin and read stdout and stderr without shell interpretation.

#### Scenario: Write stdin and read stdout

- **WHEN** the parent writes bytes to the child's standard input
- **THEN** the child SHALL receive them unmodified, and bytes the child writes to standard output SHALL be readable by the parent

#### Scenario: Separate error stream

- **WHEN** the child writes to standard error
- **THEN** those bytes SHALL be readable from the error stream independently of the standard output stream

#### Scenario: Partial reads and writes are handled

- **WHEN** a read or write transfers fewer bytes than requested
- **THEN** the operation SHALL resume from the correct offset without losing or duplicating bytes

### Requirement: Wait, timeout, and kill

The platform layer SHALL allow the parent to wait for the child to exit, optionally with a timeout, and to forcibly terminate a running child.

#### Scenario: Wait returns exit status

- **WHEN** the parent waits for a child that exits
- **THEN** the wait SHALL return the child's exit status

#### Scenario: Wait times out

- **WHEN** the parent waits with a timeout and the child has not exited within it
- **THEN** the wait SHALL report a timeout without blocking indefinitely

#### Scenario: Kill terminates a running child

- **WHEN** the parent kills a running child
- **THEN** the child SHALL be terminated and a subsequent wait SHALL complete

### Requirement: Platform support scope

Child-process execution SHALL be supported on desktop (Windows, macOS, Linux) and SHALL report an explicit unsupported error on mobile (Android, iOS). Windows and macOS are the primary targets; Linux is supported through a shared POSIX backend and is lower priority.

#### Scenario: Unsupported platform reported

- **WHEN** a process is requested on a platform that has no process backend
- **THEN** the launch SHALL fail with an unsupported error and SHALL NOT crash or hang

#### Scenario: One POSIX backend for macOS and Linux

- **WHEN** the POSIX backend is compiled for macOS or Linux
- **THEN** it SHALL use the same portable source and only the subset portable across POSIX systems (`posix_spawn`, `posix_spawnattr_*`, `pipe`+`fcntl`, `waitpid`, `kill`) and SHALL NOT require Linux-only calls such as `pipe2` or parent-death signals

#### Scenario: Teardown kills the whole child tree

- **WHEN** the parent kills a child that started subprocesses
- **THEN** the child process group SHALL be terminated so no descendant survives

### Requirement: iOS native framework linkage

The Framework iOS target SHALL declare the Apple system frameworks used by its native platform implementation, including CoreGraphics for rectangle operations. Applications SHALL receive these dependencies through the existing target rather than application-specific linker workarounds.

#### Scenario: Link native text input in an iOS application

- **WHEN** an arm64 iOS application links Framework's native text-input implementation
- **THEN** the link SHALL resolve its CoreGraphics rectangle APIs through Framework's dependency interface

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

Destroying and recreating an Android Activity in one process SHALL create a new runtime session while reusing existing process metadata definitions and process-resident defining DLL code. The host SHALL NOT clear the shared Environment, re-reflect built-in types, or add duplicate-registration guards to permit recreation.

#### Scenario: Destroy and reopen in one process

- **WHEN** a launcher Activity is destroyed and a new launcher Activity is initialized in the same process
- **THEN** built-in and module metadata SHALL preserve their existing definition identity
- **AND** the new runtime session SHALL initialize without duplicate reflection
- **AND** retained definitions and callbacks SHALL retain their data owners while defining DLL code remains process-resident

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
