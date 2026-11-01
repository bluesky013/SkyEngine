# Physical iPad verification — 2026-11-01

## Build and dependency evidence

Host: Xcode 16.4, iPhoneOS 18.5 SDK, arm64 Release, deployment target 13.0. The existing desktop build cache was preserved. The local project has a native iPad device-family declaration, fixed landscape orientation and fullscreen presentation; runtime modules are static and Aurora.Metal is an embedded signed framework.

The required iOS Jolt SDK was built through `python3 python/third_party.py -p IOS -o build_3rd -t jolt -j 4`. The initial SDK exposed the confirmed RTTI mismatch; after adding the iOS override, both Debug and Release rebuilt successfully with CPP_RTTI_ENABLED=ON and exceptions still OFF. Recast was also refreshed through the same pinned bootstrap. Other platforms' effective Jolt options were not changed, and no stale metadata was rewritten.

Framework now propagates CoreGraphics to its iOS consumers. After explicitly regenerating the Xcode project, the real Launcher link resolved both the CoreGraphics rectangle calls and the Jolt base-class typeinfo. The initial Xcode ZERO_CHECK regeneration updated the project while that invocation retained its previous build graph; an explicit configure before the next build was required.

`python3 python/tests/test_cmake_source_ownership.py` passed 26 tests with one existing skip. Strict OpenSpec validation passed. The final signed Xcode build passed; `codesign --verify --deep --strict` accepted the app and embedded Metal framework. No new target, owner, source relocation, exception path or RTTI downcast was introduced.

## Installed runtime and visual evidence

The app `com.skyengine.navmeshvalidation.ios` was installed and launched on the connected iPad mini (6th generation), running iPadOS 26.6.1. Developer Mode and DDI services were enabled by the user. The user explicitly authorized creation of the Apple Development certificate; its private key remains in the keychain.

The actual device log reports Apple A15 GPU initialization, all required static modules loaded, the default product-backed world ready with eight actors, and a successful asynchronous route with five points and length 17.600 m. All 11 cooked common products and the iOS Metal shader products were staged before signing.

The local demo uses the existing renderer scene-session interface and navigation render bridge, with independent yellow path and red agent materials. The user confirmed the green navigation surface, yellow detour route and moving red agent. The saved Xcode device screenshot was subsequently inspected and shows those elements on the native landscape iPad viewport.

## Artifacts

- App: `output/NavMeshValidation/build/ios/bin/Release/NavMeshValidation.app`.
- Development IPA: `output/NavMeshValidation/build/ios/NavMeshValidation-ios-development.ipa`.
- Device screenshot: `output/NavMeshValidation/validation/ios/ipad-navigation.png`.
- Machine-readable evidence: `output/NavMeshValidation/validation/ios/deployment.json`.
- Final build, runtime, cook and source-ownership logs are copied into that `validation/ios` directory.

The IPA was collected with ditto after signing, extracted again, and its extracted app passed deep/strict signature verification. Its personal-team provisioning profile expires at 2026-11-08 09:56:42 UTC. The IPA is a development package for the provisioned device, not an App Store distribution.

This validates the actual iPad scene and final production link; it does not claim a new macOS/Windows regression run or iOS platform unit-test execution. The user authorized committing the two production build fixes and this change on 2026-11-01. The local generated demo remains under ignored `output/`, and the independent common-IO proposal is excluded. No archive or push is part of this follow-up commit request.
