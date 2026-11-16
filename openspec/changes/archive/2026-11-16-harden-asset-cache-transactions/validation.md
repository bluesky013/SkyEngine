# Validation — 2026-11-16

## Status

Implementation and acceptance are complete; all 29 tasks are checked. Native scene/Inspector, create/delete/undo/redo, settings and user-assisted asset hover checks passed. Four key screenshots are retained. No commit or archive was performed.

## Build and regression

- Existing Xcode Debug cache and third-party paths preserved. Full default build succeeded, including real libraries, plugins, AssetTool, SandboxModule and SandboxEditor.
- All 22 regression programs exit 0. Main run: 1,566 passing gtest cases; six initially skipped. The three shader shipping cases were then exercised successfully with the existing stripped-package harness. Net: 1,569 distinct passing cases and three optional external SoftBody fixture cases unavailable. The package harness also reruns one Core case; it is not double-counted.
- Renderer.ContractTest is a standalone executable with exit-status acceptance, not a gtest case count.
- Source ownership: 26 tests run, 25 passed and one platform-specific skip. OpenSpec strict validation and git diff whitespace checks passed.
- Initial Metal ModuleTest execution from the repository root could not locate its relative shader inventory. Rerunning from output/bin/Debug (its resource directory) passed; no production-code change was needed for that fixture error.

| Program | Passed | Initial skips |
| --- | ---: | ---: |
| FrameworkTest | 364 | 0 |
| EditorCoreTest | 210 | 0 |
| EditorShellTest | 169 | 0 |
| EditorWidgetsTest | 63 | 0 |
| EditorRenderTest | 50 | 0 |
| Aurora.AdaptorTest | 109 | 0 |
| Aurora.CookTest | 67 | 0 |
| Aurora.CoreTest | 137 | 0 |
| Aurora.Renderer.ModuleTest | 13 | 3 |
| Aurora.Renderer.ResourceTest | 39 | 0 |
| Aurora.Renderer.ContractTest | 0 | 0 |
| Aurora.Renderer.ProductsTest | 13 | 0 |
| Aurora.Renderer.PrepareTest | 3 | 0 |
| Aurora.Renderer.CoordinatorTest | 18 | 0 |
| UICoreTest | 194 | 0 |
| RecastNavigation.RenderTest | 6 | 0 |
| Animation.BuilderTest | 4 | 0 |
| SoftBody.BuilderTest | 2 | 3 |
| JoltPhysicsTest | 81 | 0 |
| Content.BuilderTest | 1 | 0 |
| Particle.WorldTest | 8 | 0 |
| Animation.WorldTest | 15 | 0 |

## Cook and concurrency evidence

- Real AssetTool batch: 11 products, zero failures for macOS in the isolated project.
- Real AssetTool protocol v2: valid fingerprint succeeds; old fingerprint after an external edit is rejected; refreshed fingerprint succeeds; invalid World JSON returns the actual builder failure. Validation source bytes are restored afterward.
- The real worker check exposed a host-only active-platform selector in the fingerprint. Stable fingerprints now contain resolved target/bundle/settings instead; local configuration generation still invalidates old in-process requests. A dedicated regression verifies equal worker/host inputs and differing local generations.
- Regressions cover immutable snapshots across replacement/invalidation, failed publication, concurrent generated-identity writers with warm scans, Reset cancellation, bounded Busy, source/config reincarnation, stale product/dependency publication and completion fan-out.
- Editor regressions cover tooltip occlusion/capture/lifecycle, reentrant target switching, late preview defaults, shared material/geometry payloads, and document/actor/component replacement before undo/dirty callbacks.

## Scan scaling

Measured during the concurrent regression run; these are observations, not timing thresholds or a baseline speedup claim. For D directories, cold scan asserts at most 2D manifest reads, D locks and D saves; warm scan asserts D reads, zero locks and zero saves. Path invalidation now uses the existing per-mount path index instead of scanning all source records.

| Directories | Files per directory | Cold ms | Warm ms |
| ---: | ---: | ---: | ---: |
| 1 | 10 | 7.799 | 0.763 |
| 1 | 100 | 65.579 | 5.714 |
| 1 | 1000 | 619.779 | 48.146 |
| 4 | 10 | 25.175 | 1.583 |
| 4 | 100 | 212.980 | 21.497 |
| 4 | 1000 | 1852.474 | 121.053 |

## Native coverage

- Interactive inspection exposed a crash selecting Cube: the reflected default helper copied a non-copyable component into unconstructed Any storage. The helper now owns the constructed baseline directly with the registered deleter. A non-copyable polymorphic-root regression covers cleanup, reset and undo; EditorCore (210), EditorShell (169) and EditorWidgets (63) passed after the fix, and SandboxModule/SandboxEditor rebuilt successfully.
- Native scene selection and Engine asset path labels passed. Created a Plane, deleted it, undid and redid deletion, then undid creation to restore the original object graph without saving. Preferences and Project Settings opened correctly in sequence; the latter displays Engine/materials/basic_lit.mat. Cancel returned to a working Inspector.

- Executed on macOS x86_64 / Intel Iris Plus 655 Metal. Native Metal image/readback tests and stripped shader-product tests passed.
- A bounded editor run opened engine-assets-unified.world and exited 0 without an error log. The readback request produced no screenshot while the session was locked. This proves startup/shutdown, not interactive visual acceptance.
- User-assisted native hover displayed Engine/materials/basic_lit.mat, Type: AuroraMaterial and its GUID. Clicking File dismissed the visible tooltip; Escape closed the menu. The latest editor remains open on Cube in the isolated project. Modal/occlusion/floating-surface edge cases have automated coverage; exhaustive native visual coverage is not claimed.
- Win32 and Linux leave/re-entry code was reviewed. Native Windows/Linux execution is not claimed.
- Read-only/broken-manifest behavior is covered by filesystem tests. External tools that ignore source locks remain outside crash-atomic multi-file guarantees. Cache byte accounting excludes renderer packet copies, loader/compiler storage and snapshots retained externally after Refresh.

## Artifacts

Local logs and isolated project: `cmake-build-debug/asset-cache-validation/final/`.

- `results.json`, per-program logs, `build.log`, `openspec.log`, `source-ownership.log`
- `AssetTool-cook.log`, `real-worker.log`, `worker-stderr.log`, `shader-package.log`
- `editor-smoke.log`, `editor-interactive.log` (earlier launch), `inspector-build.log`, `Project/`
- `editor-inspector.png`, `editor-project-settings.png`, `editor-undo-restored.png`, `editor-asset-hint.png` (current native acceptance)
