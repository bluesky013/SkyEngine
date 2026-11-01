## ADDED Requirements

### Requirement: Native HarmonyOS product scope
The supported product target SHALL be Huawei native HarmonyOS. Build records SHALL distinguish the HarmonyOS product SDK and compile API from the OHOS native toolchain identity and minimum runtime API. Generic OpenHarmony device compatibility SHALL NOT be inferred from shared native API names or a successful SDK probe.

#### Scenario: HarmonyOS SDK contains OpenHarmony native tools
- **WHEN** the selected Huawei SDK locates its toolchain under `openharmony/native` and defines `__OHOS__`
- **THEN** the engine export platform SHALL remain `harmonyos`, while CMake and native ABI selection SHALL use the SDK's OHOS toolchain

#### Scenario: Compile SDK is newer than a target device
- **WHEN** a package built with the selected compile SDK is intended for a lower API device
- **THEN** its minimum API and used native APIs SHALL be checked against that device before compatibility is claimed

### Requirement: Distinct HarmonyOS build identity
HarmonyOS SHALL have a distinct engine platform identity and SHALL use the selected OHOS SDK toolchain. Its build SHALL NOT select Linux X11, Android GameActivity or host-only editor/cook sources merely because the compiler also defines Linux macros. Target libraries and caches SHALL be isolated by platform, ABI and relevant SDK/build configuration.

#### Scenario: Compiler defines both OHOS and Linux
- **WHEN** the arm64 OHOS compiler defines both `__OHOS__` and `__linux__`
- **THEN** engine platform selection and name round-tripping SHALL identify HarmonyOS, preserving existing platform values

#### Scenario: Missing or incompatible SDK
- **WHEN** a HarmonyOS build has no usable toolchain or selects incompatible target dependencies
- **THEN** configuration SHALL fail with an actionable diagnostic before attempting a mixed-platform link

### Requirement: Android dependency parity with separate target artifacts
HarmonyOS SHALL use the same third-party package set, sources and pinned versions as Android. It SHALL inherit Android package options before applying required HarmonyOS-specific overrides. Libraries SHALL be rebuilt with the OHOS toolchain into an independent HarmonyOS output and cache; Android binaries SHALL NOT be reused. OHOS SDK dependencies SHALL be adapted separately.

#### Scenario: Android dependency set changes
- **WHEN** the shared manifest adds, removes or changes an Android third-party dependency
- **THEN** a parity check SHALL detect any divergent HarmonyOS package set, source or pinned version

#### Scenario: Cross compilation requires host tools
- **WHEN** a dependency such as Slang generates source using build-host executables
- **THEN** those generators SHALL run on the build host and the target libraries SHALL still use the OHOS ABI

#### Scenario: Only some dependencies build successfully
- **WHEN** one or more target dependency builds or installs fail
- **THEN** validation SHALL identify the incomplete packages and SHALL NOT report the full dependency set as built

### Requirement: Native platform ownership and explicit errors
The OHOS implementation SHALL extend existing Core and Framework targets, reuse supported Unix primitives and expose native functionality through existing engine platform interfaces. Recoverable initialization and native API failures SHALL propagate explicit results without exceptions, process termination or silent success.

#### Scenario: Initialization fails partway
- **WHEN** callback registration, native context acquisition or required service initialization fails
- **THEN** initialization SHALL report failure and release acquired resources without leaving callbacks able to access destroyed state

### Requirement: Reusable native session host

The existing Framework target SHALL own the OHOS native session host, lifecycle queue, frame scheduling and stale-callback filtering. The project launcher SHALL supply application construction and initialization and retain its N-API/XComponent entry wiring. Host lifetime SHALL remain independent of the platform object released by application teardown. Native window references SHALL be retained before event delivery, shared through window and renderer retirement, and released using serialized SDK reference operations.

#### Scenario: Customize a project launcher without copying its scheduler
- **WHEN** a project customizes its application factory and regenerates the HarmonyOS package
- **THEN** its owned source SHALL be preserved and linked to the reusable Framework host without compiling another copy of the host implementation

#### Scenario: Stop and restart a native session
- **WHEN** the external N-API owner stops a session and configures another in the same process
- **THEN** the host SHALL join the previous engine thread before releasing session resources and SHALL preserve a stable callback receiver with monotonic generations across sessions
- **AND** callbacks from the previous session SHALL NOT invoke the replacement application

### Requirement: Stable window and lifecycle generations

The platform SHALL expose a live `OHNativeWindow` through the existing window handle contract, preserve the engine WindowID across native surface replacement, and serialize lifecycle events with frame ownership. Backgrounding or loss of a usable surface SHALL pause application ticks while lifecycle delivery continues. Resume SHALL reset frame timing; stale session/surface callbacks SHALL be ignored safely.

#### Scenario: Immersive game window
- **WHEN** the HarmonyOS game launcher creates its main window on a phone
- **THEN** the platform launcher SHALL configure full-screen layout and hide the status bar, navigation bar and gesture indicator before loading the game page
- **AND** configuration failures SHALL be reported explicitly, and completion for a destroyed window stage SHALL NOT load a stale page

#### Scenario: Surface destruction and replacement
- **WHEN** XComponent destroys a native surface and later supplies a replacement
- **THEN** the platform SHALL notify loss/replacement with the same WindowID and SHALL prevent new rendering against the retired surface

#### Scenario: Resume without an inactive frame callback
- **WHEN** the app is backgrounded and resumed between two frame callbacks
- **THEN** the next tick SHALL exclude the suspended interval and use the current surface generation

#### Scenario: Exit while a frame callback is queued
- **WHEN** application exit begins while NativeVSync or lifecycle work remains queued
- **THEN** the host SHALL stop new frames, collect pending work and complete teardown without invoking a destroyed application

### Requirement: Input and application services
OHOS input SHALL follow the engine pixel-coordinate, primary-contact and committed-text contracts. Bundle reads SHALL use application resources, writable files SHALL use the application sandbox, and timing SHALL be monotonic with nonzero frequency. Unsupported services SHALL return the existing explicit failure or Unsupported result.

#### Scenario: Touch cancelled during backgrounding
- **WHEN** the primary contact is cancelled or the app loses activity
- **THEN** the engine SHALL receive release/cancellation state and SHALL NOT retain a pressed button or synthesize duplicate primary input from secondary touches

#### Scenario: IME commit
- **WHEN** the user commits composed text through the soft keyboard
- **THEN** committed UTF-8 text SHALL reach the engine once, separately from key events and uncommitted composition

#### Scenario: Packaged resource and writable configuration
- **WHEN** the runtime opens a cooked asset and saves user configuration from an arbitrary working directory
- **THEN** the asset SHALL resolve through the packaged resource filesystem and configuration SHALL resolve inside the writable application sandbox

### Requirement: Shared native mobile text dialog contract
Framework SHALL expose the same text-dialog options and callback contract to HarmonyOS, Android and iOS. Each backend SHALL use its native UI and thread model. Initial text, placeholder, title, Unicode-scalar maximum length, multiline and password options SHALL have consistent meanings; zero maximum length SHALL mean unlimited, and a multiline password request SHALL be rejected explicitly. Unsupported, invalid or busy requests SHALL return false without a callback; accepted requests SHALL complete through the engine owner-thread callback. Production platform code SHALL NOT write input contents to logs, and password contents SHALL NOT appear in diagnostics.

#### Scenario: Native UI edits committed text
- **WHEN** a native editor commits a changed text snapshot
- **THEN** the owner-thread callback SHALL receive CHANGED with the complete committed UTF-8 snapshot, without preedit or duplicate identical snapshots
- **AND** maximum length SHALL count Unicode scalars without splitting UTF-8 sequences or surrogate pairs

#### Scenario: The soft keyboard occupies the game viewport
- **WHEN** the native keyboard is shown for an accepted dialog
- **THEN** the native editor and confirmation/cancellation actions SHALL remain visible and usable
- **AND** password requests SHALL use the native secure input mode

#### Scenario: Native control closes with an invalid final snapshot
- **WHEN** CONFIRMED, CANCELLED or ERROR carries malformed UTF-8, embedded NUL or text exceeding the accepted scalar limit
- **THEN** the accepted request SHALL complete once as ERROR using its last valid snapshot and error -2, preserving an existing nonzero native error from ERROR
- **AND** invalid CHANGED snapshots, invalid enum values and retired tokens SHALL NOT replace valid text or terminate another request

#### Scenario: A native bridge cannot convert callback text
- **WHEN** JNI, N-API or a platform text bridge fails to read the native callback snapshot for an accepted token
- **THEN** failure completion SHALL atomically use the latest valid snapshot, including accepted CHANGED events still queued for dispatch
- **AND** duplicate or retired failure callbacks SHALL be rejected without affecting a later request

#### Scenario: Cancel drains an accepted request and supports reopening
- **WHEN** the owner hides a native dialog or the application loses the UI session
- **THEN** queued changes and the single accepted terminal event SHALL be collected on the owner thread before releasing callback ownership
- **AND** a terminal callback MAY open the next dialog after the old token is retired, while application shutdown SHALL disable new requests before draining callbacks
