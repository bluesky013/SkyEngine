## Why

The Android launcher used GameActivity 1.2.2, whose native key path consumes Back and whose Activity recreation can destroy shared native text-input state still referenced by a replacement Activity. Application exit and same-process recreation need explicit lifecycle ownership so a finish request cannot bypass DESTROY or repeat persistent metadata registration.

## What Changes

- Bridge valid legacy Back releases and API 33+ system Back callbacks to the existing AndroidX dispatcher, preserving the system decision to background or finish the Activity.
- Use official stable GameActivity 4.4.2 as the sole native Activity owner and consume its replacement/late-callback fix without a parallel launcher gate or Java insets workaround.
- Keep ordinary background polling blocking; use nonblocking lifecycle pumping while waiting for renderer CPU/GPU shutdown, then continue acknowledging commands with normal native WAIT permitted until DESTROY. Request finish and application shutdown once.
- Retain lifecycle callback owners through late DESTROY, including initialization failure.
- Drain renderer terminal events through a real empty poll, including when successful internal completions precede public failure/stopped events.
- Initialize built-in Core metadata through one process-shared context owner, while module metadata remains owned by RegisterMeta and tests only consume existing definitions.
- Add focused lifecycle/ownership regressions and physical-device verification without new modules or targets.

## Capabilities

### New Capabilities

- `android-application-lifecycle`: standard Back routing, background/resume, explicit exit drain, callback lifetime and same-process Activity recreation.

### Modified Capabilities

- `module-registration`: distinguish built-in context initialization from module RegisterMeta and require tests to consume the same unique metadata owners without reflection guards or Environment resets.

## Impact

Existing Android launcher Java/manifest, Gradle/Prefab integration and native entry, Framework Android lifecycle polling and application teardown, process-shared Core serialization context initialization, and affected executable metadata setup. The AndroidPhysicsDemo project is an ignored validation fixture; it is not a shipped special case. Previous FrameGraph, shader compatibility, flat-product and physics/package verification remain separate changes. GameActivity is upgraded to 4.4.2; existing AppCompat/AndroidX versions remain unchanged. No force-finish/kill policy, vendor source patch or general platform rewrite is introduced.
