## Why

The iOS scene fixture exposed missing World startup/cook support and an incomplete launcher scene protocol. A callback-only hook suppresses simulation during renderer backpressure and cannot return entity retirement events. Copying authoring World JSON into a shipping app also bypasses the existing asset product/index/dependency pipeline.

## What Changes

- Cook editable `.world` JSON into a checked, versioned binary World asset through the existing builder/product pipeline; stop shipping authoring World JSON.
- Preserve actor identities, component persistence Data, hierarchy, typed subsystem configs and asset dependency roots using reflected typed values.
- Load the configured `defaultScene` as a ProductsOnly World product, update simulation independently of renderer submission, and present it through the existing Aurora host.
- Replace the callback setter with a single-owner scene session, precise registration, explicit view/output context, error returns and event forwarding; drain an old scene before replacement.
- Preserve logical World identities across internal symlinks and audit selected World product paths using existing product indexes.
- Keep camera/content in a reproducible fixture and use the ordinary project template; provide a parameterized simulator preset and build recipe.
- Declare a launch screen for modern native iPhone canvases.
- Verify the same World/project on Android using default platform asset filters. Asset storage is defined by the successor `simplify-asset-product-storage`: checked flat UUID products, no runtime groups and no obsolete-layout fallback.
- Finalize portable SPIR-V artifacts without optional source declarations that older software Vulkan parsers reject; bind this policy into the SPIR-V producer identity.
- Transfer drain callbacks with an explicitly empty source to break libc++ small-function ownership cycles, and apply surface output mapping exactly once.

## Impact

Existing Core task dispatch, Framework world/serialization/application targets, Content builder/metadata, Aurora adaptor/core/shader, AssetTool, Android launcher Gradle template and packaging/fixture tools. No new target, module ownership change, loader entry point, existing directory relocation, commit or archive. This replaces the earlier temporary World-document skip/verbatim-staging approach in the same change.

## Product storage successor

`simplify-asset-product-storage` replaces this change's temporary groups/.groups directory workarounds and group compatibility checks. Fresh validation must use cleared and recooked flat products. Earlier verification records remain historical evidence of the prior implementation.
