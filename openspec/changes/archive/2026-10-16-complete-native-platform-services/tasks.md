## 1. Shared platform contracts

- [x] 1.1 Add frame activity/stop and stable-window surface-change interfaces; retain fractional wheel input.
- [x] 1.2 Use a per-application clock and skip suspended ticks; stop external callbacks before teardown.
- [x] 1.3 Implement counted Unix/Android semaphore synchronization and add regression cases.
- [x] 1.4 Read startup configs through writable/bundle filesystems and stage Android package configs with tests.

## 2. Native backends

- [x] 2.1 Complete Cocoa events, pixel coordinates, identity/geometry/cursor and native shell services with native tests.
- [x] 2.2 Complete Android lifecycle, clock, registration, input/text and launcher initialization with portable mapper tests.
- [x] 2.3 Complete UIKit lifecycle/display-loop shutdown, primary input/text/scale and window lifetime with tests.

## 3. Validation and documentation

- [x] 3.1 Wire portable/native regression cases into the existing test targets and run CoreTest/FrameworkTest.
- [x] 3.2 Compile Android Core/Framework, launcher and platform regression cases against the installed NDK.
- [x] 3.3 Compile UIKit Core/Framework, launcher and native regression cases against the installed iOS SDK; execute simulator cases when a runtime is available.
- [x] 3.4 Update platform-support documentation and record exact executed, compile-only and manual validation limits; validate OpenSpec and review the combined diff.

## 4. UIScene migration and code cleanup

- [x] 4.1 Replace application-wide UIKit activity handling with an explicit, identity-filtered single-scene host interface and scene-bound native windows.
- [x] 4.2 Migrate launcher configuration/delegates to UIScene; initialize the engine once, preserve it on disconnect, and keep failed initialization/termination frame-safe.
- [x] 4.3 Migrate the checked-in native test host and simulator recipe to an actual UIWindowScene; cover scene activity, foreign callbacks, disconnect/reconnect, stable identity and delayed native input.
- [x] 4.4 Add generated scene-manifest/launcher packaging regression coverage and consolidate UIKit object/observer ownership and obsolete lifecycle helpers.
- [x] 4.5 Build the iOS SDK targets and launcher, run portable/host and native simulator regressions, and independently review callback lifetime and reentrancy.
- [x] 4.6 Update architecture/spec/validation records with executed coverage and the unavailable iOS 27 SDK/runtime limit; validate the final combined change.

## 5. Template package smoke validation

- [x] 5.1 Add a reproducible minimal game fixture and SDK-independent package regression cases; preserve first-configure plugin overrides and forward Android native preset arguments.
- [x] 5.2 Repair generated game static-module registration and Apple runtime module/framework deployment exposed by the smoke build.
- [x] 5.3 Run actual Gradle APK and device-SDK unsigned IPA builds; inspect identity, architecture, native dependencies, configs and products, recording installation limits.
- [x] 5.4 Build a macOS app and ZIP, relocate the app and run a bounded startup/shutdown smoke test.
- [x] 5.5 Record commands, artifacts, executed checks and remaining environment/runtime blockers; run regressions and validate/review the final change.

## 6. Cross-platform issue consolidation

- [x] 6.1 Consolidate resolved platform/runtime, UIScene, build/package and AssetTool issues with their causes, changes, evidence and remaining limits.
- [x] 6.2 Reconcile stale scope, bundle-path and simulator cleanup statements; distinguish historical checkpoints, SDK-only builds, package audits and actual runtime execution. Synchronize existing packaging/build requirements with bundle paths, module packing and unsigned Apple collection.
- [x] 6.3 Cross-link the current support/build guides and retained historical iOS proposals; validate documentation links, strict OpenSpec and diff whitespace without changing implementation or archiving changes.
