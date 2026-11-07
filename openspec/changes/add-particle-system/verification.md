# Verification - 2026-11-07

## Scope and result

The reviewed CPU particle, scene-extension and indirect-command corrections are implemented. Existing directories and production targets are retained. No commit or OpenSpec archive was performed. GPU particle simulation remains the separate `add-particle-gpu` follow-up.

## Corrections

- One core module schema drives source/compiler validation. Wrong JSON types and unsupported value semantics produce diagnostics without replacing valid output. Source roundtrips include bounds, space, warmup, module names and all typed-value fields.
- Program hashes cover simulation/render semantics. Binary version 3 preserves random initialization and simulation space; version 2 reads as WORLD and receives a canonical hash. Solver initialization rejects malformed programs without destroying existing valid state.
- Random initialization is deterministic, update modules retain authored order, spawn work is bounded by free capacity, death reclamation is linear, and event drains reuse capacity.
- World ticks request/poll products without waiting. Effect replacement and changed program hashes install complete solver state; unrelated publication does not restart live particles. WORLD spawn transforms apply during warmup, while LOCAL extraction follows the actor.
- World-scoped snapshots are encoded into the existing frame extension payload with exact output/texture versions. Failed source appends preserve caller/source state; unused published resources are released. The render feature owns GPU preparation state; the source owns mutable host snapshots and camera information.
- Assets is the unique authoring metadata owner. Editor properties and structure operations use document commands with stable before/after snapshots. Preview reuses compiled programs; invalid drafts remain editable and cannot overwrite source files.
- Scene registration/projection, ordering and retirement have one registry owner. GPU and presentation leases share one retained owner. Shared scene attachments enforce LOAD/STORE.
- Shader inventory entries/variants use the cooker schema; project cook collects the Particle engine inventory. Particle.Render.Static no longer links unused Particle.Assets.Static or repeats Aurora.Pipeline.Static.

## Environment and build

Existing `cmake-build-debug` cache was preserved: Xcode, Debug, MacOS-x86, third-party root `build_3rd/MacOS-x86`. Third-party precheck reported 25 stale metadata entries and 0 missing outputs. No third-party rebuild or generator change was made.

Actual production targets built successfully: Particle; Particle.Assets.Static/Assets; Particle.Builder.Static/Builder; Particle.Adaptor.Static/World; Particle.Render.Static/Render; Particle.Editor.Static/Editor; Aurora.RHI.Static, Aurora.Metal, Aurora.Vulkan, Aurora.Core.Static, Aurora.Pipeline.Static, Aurora.Adaptor.Static, Aurora.Render, Aurora.Render.Editor; SandboxModule, SandboxEditor, AssetTool and ShaderCookTool. Source ownership checks were repeated after dependency simplification.

```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Debug --parallel 8 --target \
  ParticleTest Particle.AssetsTest Particle.BuilderTest Particle.WorldTest \
  Particle.ModuleTest Particle.RenderTest Particle.EditorTest \
  Aurora.RHITest Aurora.CoreTest Aurora.PipelineTest Aurora.AdaptorTest \
  Aurora.Renderer.ModuleTest Aurora.Renderer.StaticModuleTest \
  Aurora.Renderer.CoordinatorTest Aurora.Renderer.StateTest \
  Aurora.Renderer.ResourceTest Aurora.Renderer.PrepareTest \
  EditorRenderTest SandboxModule SandboxEditor AssetTool ShaderHeaders.particle.Check
```

Run module tests from `output/bin/Debug`, as declared by their CMake working directory. Native Metal/Vulkan tests require access to host GPU services; sandbox-only runs cannot enumerate those devices.

| Check | Final result |
| --- | --- |
| ParticleTest | 20 passed |
| Particle.AssetsTest | 5 passed |
| Particle.BuilderTest | 6 passed |
| Particle.WorldTest | 7 passed |
| Particle.ModuleTest | 1 passed |
| Particle.RenderTest | 5 passed |
| Particle.EditorTest | 6 passed |
| Aurora.RHITest | 210 passed, including native Metal indirect dispatch |
| Aurora.CoreTest | 127 passed; 1 distribution-fixture test skipped in the default run |
| Aurora.PipelineTest | 45 passed |
| Aurora.AdaptorTest | 89 passed |
| Aurora.Renderer.ModuleTest | 10 passed; 3 distribution-fixture tests skipped in the default run |
| Aurora.Renderer.StaticModuleTest | 11 passed; 3 distribution-fixture tests skipped in the default run |
| Aurora.Renderer.CoordinatorTest | 18 passed |
| Aurora.Renderer.StateTest | 14 passed |
| Aurora.Renderer.ResourceTest | 39 passed |
| Aurora.Renderer.PrepareTest | 3 passed |
| EditorRenderTest | 45 passed on final run |
| Python shader_project / module_resources | 13 + 10 passed |
| CMakeSourceOwnership | 25 passed, 1 skipped |
| ShaderHeaders.particle.Check | passed |
| Metal shipping package: real cook + relocated compiler-free consumers | 4 passed (Core 1, native 3), including material/image versions |
| OpenSpec strict validation | all three reviewed changes passed |
| SandboxEditor --console --frames 2 | exit 0; Metal initialized; all five Particle modules loaded and host stopped |

The default C++ suites above total 661 passing tests; distribution-fixture checks are listed separately below.

The first EditorRenderTest run returned PENDING in `CpuViewportPickerTest.RandomTransformedTrianglesMatchWorldSpaceOracle`. Its unchanged query has a 1 ms budget, but the test assumes a single Query completes. Focused reruns demonstrated timing sensitivity (8/10, then 100/100); the final full suite passed. Picker implementation/tests match HEAD and were not modified.

## Shader distribution integration

The actual Metal shipping cook includes both `particle/ParticleSprite.slang` entries (VSMain/FSMain). The relocated compiler-free Core consumer resolves every request, including both particle entries, from the binary products after source removal. The native scene and compute consumers also passed.

The additional material consumer exposed an existing fixture gap: PackageMaterial declared only its default variant while SceneAssetSource requests all SHADOWS/IBL combinations. Diagnostics confirmed `published shader request/environment not found` for that material. The fixture now lists all four combinations and the test reports explicit renderer errors; no production material loading behavior was changed. After the fixture correction, `python3 python/tests/run_shader_package_tests.py --target metal` exited 0: Core 1/1 and native 3/3 passed. The same generated requirements include both ParticleSprite entries. Logs: `/tmp/sky-package-material-fixed.log` (passing), `/tmp/sky-package-material-old.log` (confirmed missing material variant).

## Performance evidence

A Debug microbenchmark reclaimed all particles using the production death-sweep storage primitive, with the same capacities and host:

| Particles | Previous slot-search deletion | Known-index deletion |
| --- | --- | --- |
| 8,000 | 10.753 ms | 0.903 ms |
| 16,000 | 40.091 ms | 1.743 ms |
| 32,000 | 159.177 ms | 3.566 ms |

This demonstrates linear reclamation scaling; it is not a release frame-time benchmark. The 100,000-particle spawn/update test and the 20,000-particle death/reuse/event regression also passed.

## Remaining scope and limits

- GPU particle state/simulation/indirect sprite rendering, GPU budgets and fallback are not implemented here. GPU/CUSTOM and unresolved parameter/attribute bindings return explicit unsupported diagnostics.
- No particle visual golden or complete HDR/non-HDR extension rendering matrix was run. The editor smoke checks module startup in hub mode, not a visible effect scene.
- DX12 was not compiled or executed on this macOS host. New Vulkan/DX12 frame-compiler native cases belong to the existing WIN32-gated Aurora.Renderer.GpuSubmissionTest target and were not run here; Metal native dispatch and Vulkan logical-feature tests did run.
- The editor emits existing duplicate Objective-C class warnings from its executable and SandboxModule. Startup and shutdown still succeeded; this unrelated packaging issue was not changed.
- SUB_UV dimensions are 1..65535; warmup is at most 60 seconds. The current scalar kill-volume module checks a Y interval in authored update order.

## Dependency review corrections (2026-11-07)

- Removed the single-slot `Interface<ISceneRenderExtensionSource>` usage: hosts now consume the process-wide `SceneRenderExtensionSourceRegistry` (ordered append, identity-safe unregister, first-error propagation). `Aurora.AdaptorTest` gained registry ordering/error tests; `Particle.Render` no longer fails module startup when another scene-contributing subsystem is present.
- Replaced hardcoded scene feature ids with named constants per host (`RUNTIME_SCENE_FEATURE_ID`, `SCENE_FEATURE`) and a named particle extension index.
- `Particle.Editor.Static` now declares its `Framework` link; the adaptor test main no longer registers the particle builder (the cooking suite registers it once in `SetUpTestSuite`).
- Cached the default white texture payload in `ParticleRenderSource`; the remaining per-frame allocation is the immutable render-frame snapshot.
- `python/shader_project.py` uses an explicit engine inventory list instead of globbing `engine/*/resources.json`.
- Editor automation flags are documented in `docs/editor/editor-framework-design.md`; a timed-out screenshot now logs the occlusion hint.

Verified on macOS Debug: `ParticleTest`, `Particle.AssetsTest`, `Particle.BuilderTest`, `Particle.WorldTest`, `Particle.ModuleTest`, `Particle.EditorTest`, `Particle.RenderTest`, `Aurora.AdaptorTest`, and `CMakeSourceOwnership`. The local demo (`output/ParticleDemo`, git-ignored) renders three spark jets through `--world/--play/--screenshot` and two captures at different frames differ, confirming live PIE simulation.

## Cleanup pass (2026-11-07, second review)

- Compiler now rejects `lit`, `soft`, and `PARAMETER`/`ATTRIBUTE` value bindings with explicit diagnostics instead of silently lowering them to unlit/opaque/constant values (core tests extended).
- Removed dead API: unused `ParticleValue::Evaluate*` helpers and `ParticleRange::Evaluate`; `ParticleRenderEmitter` no longer carries the never-enabled `lit`/`soft` flags.
- Removed duplicate frame-revision assignment and pre-reserved the frame emitter array.
- Replaced the hand-rolled BMP screenshot writer with `stb_image_write` PNG output (`--screenshot <file.png>`); editor docs updated.
- Pruned unused includes (`SceneAssetSource` from the render feature, payload header from the public render feature header) and added the missing `<optional>` include to the extension-source interface.

Re-verified: all particle tests, `Aurora.AdaptorTest`, `CMakeSourceOwnership`, and the live PIE demo capture (PNG, correct orientation).

## Live asset preview alignment (2026-11-07)

Engines such as Niagara/VFX Graph preview the working asset through the real renderer without saving or cooking. The particle preview now follows that model:

- `ParticlePreviewEffectRegistry` (assets layer, cross-DLL `Singleton`) holds compiled `ParticleEffectAssetData` keyed by preview world and effect UUID.
- `ParticleSystem` consults the registry before the product cache, so an unsaved draft runs through the same solver/render scene extension as runtime.
- The editor writes the compiled working effect on every document revision and clears it on close; entries are world-scoped so PIE and the edit world keep using saved products.
- The CPU solver overlay remains strictly a fallback when the preview host or `Particle.World` is unavailable, or while the render preview is not ready.
- Asset format compatibility remains strict (`version = 1`, product `MAGIC/VERSION`); invalid or old data reports diagnostics instead of being misread.

Verified: `Particle.WorldTest` gained a no-product in-memory preview case (8/8), `Particle.EditorTest` 6/6, `Particle.RenderTest` 6/6, `ParticleTest` 20/20.

Fixes from the preview verification pass:

- `ParticleEditorView` looked up the component metadata as `"Particle"` while `ParticleComponent::Reflect` registers `"ParticleComponent"`; the render preview therefore failed at init and silently fell back to the CPU overlay. The lookup now matches the registered type, and the preview texture reaches `Ready` with the in-memory effect applied.
- Added `--asset <uuid>` (with catalog wait and retry) alongside `--world/--play` so asset views can be opened and verified from the command line; documented in `docs/editor/editor-framework-design.md`.

## Preview visibility fix (2026-11-07)

The asset-tool preview was blank for two independent reasons, both now fixed:

1. `ParticleEditorView` resolved the component metadata with `FindType("Particle")` while the registered type is `"ParticleComponent"`, so `RenderPreview::Init` failed before creating the session.
2. `ParticleEditorDocument` did not report `FitsContentToViewport()` and `ParticleEditorView` did not implement `OnMeasure`, so the asset-tool layout measured the editor at one row (20 px); the preview rect became 1 px tall and the preview viewport was resized to `697x1`.

After both fixes the preview viewport is `697x1304` and a GPU readback of it shows the three spark jets. The layout now fills the asset-tool body, and `--asset <uuid>` allows opening the asset view directly for verification.

## Generic preview-region alignment (2026-11-07)

The particle editor originally drew its own split preview inside the content widget, which diverged from the asset viewer's left preview region and measured to a 1 px height. It now follows the same structure as the animation asset editors:

- `ParticleEditorDocument::CreatePreview` returns a `ParticlePreviewView` for the generic left preview region; the content widget only hosts the emitter/module stack and the reflected form.
- The document owns the `ParticlePreviewSession` (render preview world + CPU fallback) and ticks it, mirroring `GraphEditor`/`CharacterPreviewView`.
- Layout fill (`FitsContentToViewport`, `OnMeasure`) keeps the right content full-height; the left preview viewport is arranged to 38% of the body and reaches `993x1388` in the asset tool window.
- Readback of the generic preview viewport shows the three spark jets (verification capture), confirming the real render path is used for the preview.

## Emitter / renderer authoring surface (2026-11-07)

- The stack tree selects effect / emitter / module / renderer; the right reflected form edits the selected object's scalar and struct fields. Edits go through `CommandService` (undoable) and apply to the source buffer, which the preview session picks up on the next revision.
- Added the missing `Add renderer` action (SPRITE/ALPHA default); `Remove selection` already removes a selected renderer. `Add module` appends an INITIALIZE module and the form's `type` field changes the module kind.
- Removed the unimplemented `lit` / `soft` flags from the reflected renderer form so users cannot enable options the compiler rejects; the fields remain in the serialized format for future support.
- Known gap: module `parameters` (and curve/gradient key arrays) are reflected vectors and are not editable in the form yet; authoring these currently happens in the `.particle` JSON. A schema-driven parameter grid is the follow-up.

## Parameter editing and cleanup (2026-11-07)

- The stack now exposes schema parameters under the selected module; selecting one binds the reflected form to its `ParticleValue` (kind/constant/vector/range). Missing parameters are materialized into the edit snapshot and committed with the normal undoable edit. Curve/gradient key lists remain a documented follow-up (`7.8`).
- Compile-rejected enum options are no longer offered in the form: `ParticleSimTarget` (GPU), `ParticleSimSpace` (Custom), `ParticleRendererType` (Mesh), `ParticleValueKind` (Parameter/Attribute).
- Replaced deprecated `std::filesystem::u8path` with `sky::PathFromUtf8` (`core/file/FileUtil.h`) in the particle editor and sandbox preferences.
- Known pre-existing issue (not introduced here): the Cocoa `SkyCocoaView` / window delegate classes are compiled into both `SandboxEditor` and `SandboxModule.dylib`, producing duplicate-Objective-C-class warnings; fixing it needs a single platform-image owner (shared platform library or explicit build split).
