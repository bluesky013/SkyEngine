## 1. Standard Back bridge

- [x] 1.1 Bridge tracked, uncanceled legacy Back release to the existing dispatcher; consume repeats without navigation and preserve other native keys.
- [x] 1.2 Add API 33+ system callback registration, manifest opt-in and exact onDestroy unregistration for the retained AndroidX dispatcher, without a forced-exit policy.
- [x] 1.3 Keep the ignored AndroidPhysicsDemo Java/manifest consistent with the public templates and cover public create/generate materialization.
- [x] 1.4 Integrate official GameActivity 4.4.2 Java/native/Prefab ownership and verify resolved AndroidX versions; remove provisional old-version insets workarounds.

## 2. Native exit and lifecycle ownership

- [x] 2.1 Use NON_BLOCKING lifecycle pumping while awaiting renderer CPU/GPU STOPPED, preserving ordinary background WAIT and allowing normal native WAIT after shutdown until DESTROY.
- [x] 2.2 Request finish and application Shutdown once, retaining lifecycle callback ownership through native DESTROY and duplicate/late commands.
- [x] 2.3 Preserve the callback owner and release initialized resources once when initialization fails before the game loop starts.
- [x] 2.4 Drain renderer events after STOPPED and skip successful internal completions until a public event or actual queue exhaustion.
- [x] 2.5 Verify overlapping Activity handoff with upstream native ownership and no parallel launcher gate/proxy.
- [x] 2.6 Add checked Main-role release after executor/Main notification drain and final application callbacks are released, preserving owner assertions, Clear drop semantics and other roles.

## 3. Process metadata and repeated sessions

- [x] 3.1 Give built-in Core metadata one process-shared context creation owner with an explicit context factory and no recursive singleton construction.
- [x] 3.2 Remove repeated Core reflection from application/test consumers while preserving module RegisterMeta and metadata/code leases.
- [x] 3.3 Add focused regressions for stable metadata across multiple applications, exit/late DESTROY, initialization failure and polling modes without weakening duplicate-owner checks.

## 4. Verification and cleanup

- [x] 4.1 Build affected host targets and run focused lifecycle, metadata and template-generation tests; record actual results and preserved failure evidence.
- [x] 4.2 Repackage/audit the physical-device fixture and record the installed APK identity for the tested source.
- [x] 4.3 Verify actual Back key and API 33+ gesture against the observed system policy, plus independent HOME background/resume.
- [x] 4.4 Verify the separately identified 4.4.2 native exit/failed-Init/recovery probe and ordinary same-process recreation.
- [x] 4.5 Record final device/application cleanup, remaining coverage boundaries, OpenSpec strict validation and diff checks.
