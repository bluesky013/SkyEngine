## Why

macOS, Android and iOS expose common platform interfaces but do not yet provide consistent input, window lifecycle and application timing. Android additionally cannot reliably load its packaged startup configuration, so a successful compile does not establish a usable native platform backend.

## What Changes

- Complete Cocoa window events, client pixel coordinates, live geometry, cursor and native shell services.
- Complete Android window registration, GameActivity input, monotonic timing, app/surface lifecycle and packaged configuration startup.
- Complete UIKit input and scene-based active/background lifecycle, frame callback ownership and shutdown.
- Migrate the iOS launcher and native test host to UIScene, bind windows to UIWindowScene and preserve the engine application/window across scene disconnection and reconnection. This addresses the scene lifecycle requirement for apps built with the iOS 27 SDK.
- Add common frame suspension and native surface change notifications. Keep window identity across mobile surface recreation.
- **BREAKING**: preserve fractional wheel deltas as `float`; executable and dependent modules must be rebuilt together.
- Implement the Android/Unix semaphore used by application workers.
- Require GameActivity 1.2.2's official Prefab static library instead of compiling incompatible 1.1.0 vendor sources inline with the current NDK.
- Add executable portable regression cases, native platform tests and target compilation checks.
- Exercise one minimal generated game through actual Android APK, unsigned iOS IPA and macOS app packaging. Repair template module registration, native build options, bundle layout and runtime library deployment exposed by this validation, and retain a reproducible smoke-project recipe.
- Repair AssetTool filesystem ownership, post-registration builder configuration and batch completion/failure reporting exposed by the package smoke test; propagate failed cooks to the package command.

## Capabilities

### New Capabilities

- `native-platform-services`: mobile lifecycle, frame gating, input units, platform service contracts and executable regression coverage.

### Modified Capabilities

- `native-platform-backend`: extend native window event, DPI and service behavior to the mobile backends and align Cocoa behavior.
- `asset-packaging`: align cooked-product locations with the iOS and macOS bundle filesystems.
- `game-project-build`: select shared/static module packing through presets and distinguish unsigned Apple package collection from signed distribution.

## Impact

Changes include Core async primitives/dynamic module loading, Framework native platforms/application startup, launcher/game templates, packaged startup configuration, AssetTool bootstrap/result reporting and their tests. The packaging follow-up includes static game-module registration, Apple default backend selection, and the iOS dynamic framework build/load contract needed for a correctly structured package. No GPU algorithm, graphics capability probing, shader compilation/asset conversion algorithm or editor UI work is included. Signing identities, provisioning and store distribution remain outside this smoke test; unsigned artifacts do not establish device or GPU validation.
