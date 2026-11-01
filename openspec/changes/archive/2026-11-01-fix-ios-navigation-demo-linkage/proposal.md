> Status (2026-11-01): **ready-to-archive**；owner：`fix-ios-navigation-demo-linkage`。已交付范围和既有验证齐备，主 spec 同步与 archive 等待用户确认；不扩大已记录的平台验收边界。

## Why

A real iPad build of the navigation demo reaches the final native link but fails on CoreGraphics rectangle APIs from Framework's iOS text-input implementation and C++ typeinfo referenced by the Jolt adaptor. The iOS dependency closure must match the implementation that consumes it.

## What Changes

- Link CoreGraphics from Framework's existing iOS target dependency list.
- Enable C++ RTTI for the iOS Jolt SDK, matching its existing engine consumers and the other Clang-platform overrides; keep exceptions disabled.
- Rebuild pinned iOS Debug/Release dependencies and verify the actual signed application, installation, asynchronous path and rendered result on the connected iPad.

## Capabilities

### Modified Capabilities

- `platform`: the iOS Framework target owns the system frameworks used by its native implementation.

- `physics`: the iOS SDK supplies the typeinfo expected by the existing adaptor.

## Impact

Production changes are limited to the existing Framework iOS dependency list and Jolt's iOS bootstrap override. No target, module ownership, exception path or RTTI downcast is added. The local demo under `output/NavMeshValidation` supplies deployment and visual evidence; it remains generated validation data. Signing keys stay in the user keychain.
