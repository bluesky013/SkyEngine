## 1. Backend

- [x] 1.1 Split live capabilities from presentation-option negotiation; avoid stable-frame enumeration.
- [x] 1.2 Invalidate on native SUBOPTIMAL/OUT_OF_DATE, retry failed negotiation and preserve acquisition/presentation retirement.

## 2. Verification

- [x] 2.1 Add native call-count and invalidation/retry/format/color-space/mode regression coverage.
- [x] 2.2 Build Debug and Release and run FrameOutputs lifecycle regressions on Vulkan and DX12.
- [x] 2.3 Measure the same rotating-camera scene in Debug/Release, remove temporary probes and rebuild ordinary binaries.
- [x] 2.4 Review implementation and record evidence, performance limits and Develop migration assessment; validate OpenSpec.
