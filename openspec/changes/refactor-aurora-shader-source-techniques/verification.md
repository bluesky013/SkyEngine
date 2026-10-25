# Two-commit snapshot verification — 2026-10-26

User requested exactly two remaining feature commits: material/Shader first, complete animation second. Generic process metadata ownership is part of the first commit. No commit or archive approval is implied by these checks.

## Material / Shader snapshot

Base: `60a162424fdf5659963e1f46a8f4014d61d5c344`. Isolated source/cache: `cmake-build-debug/split-src` and `cmake-build-debug/split-verify`; Windows VS 2022 Debug, existing third-party configuration. The first snapshot contains no Animation plugin or new Skin/Skeleton product implementation. Shader SKINNED input/deformation/layout support is available; Scene palette upload and pose snapshots follow with animation.

Framework, Aurora Core/Pipeline/Adaptor/Cook/Shader, editor tests, SandboxModule/Editor, AssetTool and actual dynamic/static renderer module and GPU test targets built successfully. Initial extraction caught GraphCanvas test enumeration and WorldEditorState callers crossing the boundary; those callers remain with animation. The final successful build is `split-v4/04-build-final.log`; earlier targets completed in `04-build.log`.

| Suite | Passed | Skipped |
|---|---:|---:|
| FrameworkTest | 204 | 0 |
| Aurora.CoreTest | 99 | 1 |
| Aurora.PipelineTest | 48 | 0 |
| Aurora.AdaptorTest | 54 | 0 |
| Aurora.CookTest | 44 | 0 |
| Aurora.ShaderTest | 64 | 0 |
| EditorCoreTest | 179 | 0 |
| EditorWidgetsTest | 35 | 0 |
| EditorShellTest | 144 | 0 |
| EditorRenderTest | 53 | 0 |
| SandboxProjectTest | 11 | 0 |
| Aurora.Renderer.ModuleTest | 6 | 2 |
| Aurora.Renderer.StaticModuleTest | 7 | 2 |
| native | 222 | 3 |

Total: **1170 passed, 8 skipped, 0 failed**. The 225-case native selection covers EditorHost, RendererHost, RendererRuntime and IndexedRaster; this is a targeted run, not all GPU tests. The 8 skips require a separately cooked products fixture (including three relocated RendererRuntime cases). Python shader header/project tests passed 20/20. Bounded editor startup exited successfully after 8 frames.

Tests use the normal Windows execution environment. Restricted sandbox runs denied temporary file creation/cleanup; their failed logs are retained and do not replace the successful normal-environment results. Builds and native tests were sequential to avoid locked DLL copies.

## Exact reconstruction and documents

`split-v4/series.json`, `04-files.txt`, `05-files.txt` and the two patches define the reviewable snapshots. Applying both patches plus the separately retained 14 unrelated document changes reconstructs the complete working tree. The original primary index remains unchanged. The built first snapshot was checked against 1,986 non-document tree paths, with no content mismatch or extra source file.

First-snapshot delta merge preview passed 56/56; complete working-tree preview passed 70/70. Full working-tree strict validation is 209/214, with five known no-delta plans. First-snapshot strict validation is 202/213: the same five plans plus six console specs whose unrelated corrections remain pending outside these two commits. All affected feature changes/specs validate; no archive was run.

## Integrated animation evidence

The second snapshot restores the previously built and tested complete implementation. SHA-256 comparison against the earlier preflight verified all 515 changed non-document paths unchanged. Full integration evidence is reused on that basis; this preparation did not rerun a separate full second-snapshot build. Its documentation/ownership edits do not alter implementation.

Historical integrated runs include Framework 205; Animation Graph/Builder/World/Editor/Module 14/10/4/8/2; Content Adaptor/Builder 7/1; Aurora Adaptor/Cook 66/52; EditorCore/Widgets/Shell/SandboxProject 179/40/144/13; Network 5 and Bullet 8. The prior integrated native run was 174/177, followed by three successful focused picking reruns after correcting the polling helper; those are distinct runs, not a single all-green full suite. See `split-v3/verification.json` and the underlying XML/logs for exact evidence.

Native Metal, Linux/mobile, clean builds and outstanding composite animation/UI acceptance are not claimed. Their OpenSpec tasks remain open. Source-techniques and animation are not ready for complete archive on this evidence alone.

All evidence paths above are under `cmake-build-debug/cleanup-review-2026-10-25/`.
