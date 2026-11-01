# Design

## Root cause

Implementation and module entry were compiled in one module target. Tests needing private implementation classes could not link that DLL API, so implementation sources were copied into the test target instead of providing an implementation library. The shared codec was also compiled by three targets. The same shortcut existed outside physics. Build helpers accepted arbitrary sources and verification checked behavior only, so neither configuration nor tests enforced source ownership.

## Ownership without another architecture layer

| Existing location | Implementation owner | Consumers |
| --- | --- | --- |
| plugins/jolt/adaptor | JoltPhysics.Adaptor.Static | JoltPhysicsModule, JoltPhysicsTest |
| plugins/jolt/render | JoltPhysics.Render.Static | JoltPhysicsRenderModule, JoltPhysicsTest |
| plugins/jolt/builder | JoltPhysics.Builder.Static | JoltPhysics.Builder, JoltPhysicsTest |
| plugins/jolt/shared | JoltPhysics.AssetCodec.Static | adaptor and builder implementation libraries |
| plugins/animation/editor | Animation.Editor.Static | Animation.Editor, Animation.EditorTest |
| plugins/network-enet | EnetNetwork.Static | EnetNetwork, EnetNetworkTest |
| engine/sandbox/module | SandboxModule.Static | SandboxModule, SandboxProjectTest |
| engine/framework/platform/android | Framework.AndroidInput | native Android Framework, host FrameworkTest |

These are compiled implementation targets within the existing responsibilities, not new core modules. Module factories retain their original runtime targets. Private test headers remain PRIVATE include paths; native Jolt compile requirements remain confined to the adaptor and tests inspecting native data. New static implementation targets use PIC when linked into shared modules. Android export includes its input dependency.

## Enforcement

The common functions include installs a deferred validation pass at the top-level directory. A second deferred call places validation after ordinary deferred source additions. Both engine and independent game projects use this entry.

Inspect final `SOURCES` / `INTERFACE_SOURCES` for every build-system target, not textual CMake spelling. Normalize absolute paths, dot segments, real paths and Windows case. Detect native translation units by suffix or explicit source language; skip files genuinely marked HEADER_FILE_ONLY and references to compiled OBJECT targets. Reject unresolved source generator expressions rather than silently ignoring them; choose source sets with configure-time conditions.

Each production translation unit has one primary compile owner. A target may compile production files under its own source directory, or explicitly generated files under the top-level build directory. A test created by sky_add_test may compile only test fixtures or generated test code; disabling the production target therefore cannot hide direct source borrowing. Publishing translation units through INTERFACE_SOURCES is forbidden.

`test`, `tests`, `*_test` and `*_tests` directories identify test-only fixtures relative to the engine/project source roots, not arbitrary parent directory names. Shared mains, mocks and DLL probes may be compiled into separate test binaries. This allowance does not cover any production path.

Aurora.Renderer.StaticModuleTest intentionally recompiles AuroraRegistry.cpp to test its differently tagged static factory even under dynamic packaging. Preserve this behavior with `sky_source_compile_variant`: exact existing owner, exact source, nonempty reason and required distinct target definitions. Validate both source memberships and definitions; stale, overly broad or non-test declarations fail. This declaration does not change Aurora target ownership, loading entry or architecture. There is no global opt-out or production-source whitelist.

## Verification and limits

Negative fixtures invoke real CMake configuration for explicit paths, GLOB, late/deferred target_sources, normalized aliases, Windows case, disabled owners, same-directory tests, interface propagation, unknown expressions and invalid variants. Positive fixtures cover static/object target reuse, test helpers, generated launcher registration and source properties. CI runs these without third-party SDKs; CTest exposes the same suite.

Validate the current Windows target graph and rebuild affected implementation libraries, DLLs and unit/lifecycle tests. Inspect every repository CMake source list for inactive-platform patterns as well. Native Android/iOS/macOS builds require their SDKs and are not claimed by the Windows verification. The guard evaluates targets present in each configuration; it is not a parser for inactive CMake branches or C++ textual inclusion. New feature configurations receive the same checks when configured. Normal code review remains responsible for semantic layering and intentionally different compile contracts.
