## ADDED Requirements

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
- **THEN** the worker SHALL finish before its semaphore is destroyed and asset work SHALL finish before its module code is unloaded

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
