## Context

The navigation demo is being deployed to a physical iPad through the current project-owned iOS launcher and Metal framework. A real arm64 link reports unresolved `CGRectGetMaxY`, `CGRectIntersection` and `CGRectIsNull` from `IOSNativeTextInput`, plus base-class typeinfo for `JPH::GroupFilter` and `JPH::DebugRendererSimple` referenced by adaptor subclasses.

## Decisions

Framework owns its native API dependencies, so its existing iOS `FM_EXT_LIB` list gains CoreGraphics. Individual applications do not patch around that missing transitive dependency.

The Jolt manifest already enables C++ RTTI for Android, macOS and Linux consumers. Add the equivalent iOS override and rebuild both Debug and Release through `third_party.py`. Consumer compilation flags, exception settings, pinned revision and Apple native-symbol isolation remain unchanged. RTTI support does not introduce `dynamic_cast` or exception-based errors.

The existing `simplify-asset-product-storage` change owns the broader platform/consumer ABI contract. This change adds the concrete iOS case without redefining that shared requirement or changing other platform options. Package fingerprints include the complete manifest entry: other platforms can become metadata-stale even though their effective options and installed outputs are unchanged. Do not rewrite their metadata to hide that condition.

## Validation and delivery

Use the real signed Launcher link and physical-device startup as the acceptance path, together with source-ownership checks. The demo uses existing runtime interfaces and render bridges to show the actual asynchronous path, green navigation surface, yellow route and moving red agent. Resources are staged before signing, and the embedded Metal framework is signed by Xcode. Report build, installation, runtime logs and visual inspection separately.

The user authorized committing this follow-up fix on 2026-11-01. Keep the two production build changes and this change's artifacts in the same commit. Archive remains a separate user decision; local demo packages and signing material are excluded.
