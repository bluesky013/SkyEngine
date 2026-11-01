> Status (2026-11-01): **ready-to-archive**；owner：`fix-ios-navigation-demo-linkage`。已交付范围和既有验证齐备，主 spec 同步与 archive 等待用户确认；不扩大已记录的平台验收边界。

## 1. Dependency closure

- [x] 1.1 Add CoreGraphics to Framework's iOS dependencies.
- [x] 1.2 Add the iOS Jolt RTTI override while preserving other platform options and exception settings.
- [x] 1.3 Rebuild pinned iOS Jolt Debug and Release artifacts through the existing bootstrap.

## 2. Verification

- [x] 2.1 Run source-ownership and strict OpenSpec checks.
- [x] 2.2 Build and sign the real iOS Launcher, including its staged products and Metal framework.
- [x] 2.3 Install on the connected iPad and verify actual asynchronous routing and visible animation.
- [x] 2.4 Record artifact paths, device evidence and remaining validation limits for review.
