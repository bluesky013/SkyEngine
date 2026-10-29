## 1. Native correctness and resource-use facts

- [x] 1.1 Produce compile-local typed resource uses and unify pre-cull declaration validation.
- [x] 1.2 Consume raster uses for Vulkan/DX12 storage synchronization and combined physical-buffer read roles.
- [x] 1.3 Lower Metal attachment preservation and resolve in one render pass.
- [x] 1.4 Validate and consume Metal plans safely and propagate recording failures without successful receipts.
- [x] 1.5 Add focused content, storage synchronization, resolve and plan ownership regressions.

## 2. Immutable binding groups

- [x] 2.1 Introduce immutable logical group references with independent per-use dynamic offsets and migrate graph consumers.
- [x] 2.2 Reuse compatible group target encoding and uniform uploads within graph/lease composition.
- [x] 2.3 Materialize/reuse native descriptors per compatible group and retain exact resource/range/view keys.
- [x] 2.4 Verify sparse sets, differing layouts/ranges, multiple draws and in-flight lifetime/reuse behavior on the available host; record Windows native follow-up separately.

## 3. Public protocol boundaries

- [x] 3.1 Separate named parameter updates from numeric descriptor writes and migrate consumers.
- [x] 3.2 Remove unused DescriptorBatch public protocol and preserve relevant native writer tests.
- [x] 3.3 Isolate direct-frame compatibility in an explicit interface/factory with checked begin/reset.
- [x] 3.4 Complete feature graph declaration operations and separate resource/draw services.
- [x] 3.5 Migrate API contract fixtures and add a GPU-only transient-buffer feature regression.

## 4. Verification and documentation

- [x] 4.1 Configure the existing macOS build and build affected interfaces, libraries, backends and tests.
- [x] 4.2 Run current applicable CPU/native regressions and record unavailable Windows/DX12 checks explicitly.
- [x] 4.3 Check scoped formatting, ASCII/includes, forbidden patterns, whitespace and strict OpenSpec validation; record unavailable clang-tidy.
- [x] 4.4 Review the final diff and update current API/design rules and verification evidence without commit/archive.
- [ ] 4.5 Run the Windows Vulkan/DX12 native storage/group-reuse and GPU lifecycle regressions listed in verification.md on the Windows validation host.

## 5. Physical Vulkan device compatibility

- [x] 5.1 Preserve explicit compiler modes, default to AUTO and reject invalid configuration; verify public/default/non-Vulkan behavior and actual capability selection.
- [x] 5.2 Validate version-correct Vulkan 1.1/KHR feature and command negotiation, mandatory synchronization and unsupported direct-rendering errors.
- [x] 5.3 Recook Android SPIR-V 1.3 products, pass the 40-check physics APK audit, verify falling/settled bodies over the 121-second device run and record the separate 15-second presentation-progress sample with controlled cleanup.
- [ ] 5.4 Complete separately scoped visual-quality/validation review of observed shadow striping and vendor warnings; the physics/package run does not establish full visual acceptance or a validation-clean device run.
