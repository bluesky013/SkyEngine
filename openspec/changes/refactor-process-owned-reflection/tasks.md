## 1. Approval and implementation

- [x] 1.1 Obtain approval for Aurora.Metadata, Content.Metadata and Animation.GraphAssets and definition/session ownership (2026-10-25: 用户回复“按此方案实施”)。
- [x] 1.2 Add existing Aurora/Content metadata owners and remove their runtime/cook/editor/window duplicate reflection entry points without directory moves; Animation.GraphAssets is delivered with the complete animation workflow.
- [x] 1.3 Restore strict type/name/component checks and separate process definitions from runtime sessions.
- [x] 1.4 Retain callback code and share live DLL leases across sessions; keep runtime-only modules unloadable.
- [x] 1.5 Wire editor/game/template/AssetTool metadata dependencies and executable/test-environment fixtures.
- [x] 1.6 Reconcile current rules and design documents with the no-guard registration contract.

## 2. Verification and delivery

- [x] 2.1 Complete Windows Debug build and resolve compile/link failures.
- [x] 2.2 Pass Framework metadata retry and lifecycle tests with metadata count exactly one.
- [x] 2.3 Pass real Animation module cook/runtime/editor reload and repeated window creation tests.
- [x] 2.4 Pass Aurora/Content/Animation asset, cook, runtime and editor regression suites.
- [x] 2.5 Pass dynamic/static renderer module tests, including compiler DLL final-release behavior.
- [x] 2.6 Run bounded editor and AssetTool startup/product validation; record platform limits.
- [x] 2.7 Validate process ownership in the isolated material/Shader prerequisite snapshot, its exact patch and corresponding OpenSpec before commit approval.

No commit or OpenSpec archive has been authorized.

Validation evidence (2026-10-25, Windows VS 2022 Debug): Framework 205/205; Animation 38/38; Content 8/8; EditorCore/Widgets/Shell/SandboxProject 375/375; Aurora Adaptor/Cook 118/118. Renderer module dynamic/static lifecycle 6/7 passed, with two package cases skipped in each ordinary run; isolated cooked products subsequently passed Core 1, Vulkan 2 and D3D12 2. Python shader headers/project 10/10 each and animation package 3/3. Bounded editor initialized the actual animation project and shut down after 8 frames. Logs: `cmake-build-debug/cleanup-review-2026-10-25/`. Native Metal, Linux/mobile and clean builds are not claimed.

Final follow-up (2026-10-26): Reflection Demo metadata now lives in EditorCore and is owned by SandboxModule; its constructor only binds. NetworkWorld and Bullet test metadata also have explicit entry owners. Editor 376/376, NetworkWorld 5/5, Bullet 8/8 and actual Animation module lifecycle 2/2 passed after this cleanup. Full Debug incremental build passed (`build-final-callers-2.log`), followed by the GPU test-helper rebuild (`build-gpu-picking-final.log`). Bounded editor startup was rechecked in `editor-reflection-final.log`.

Native GPU regression initially passed 174/177. The three static-picking failures were false negatives in the polling helper: the final blocking Tick advanced the scene from revision 9 to 10, but the helper returned false without rechecking the condition. Keeping all behavior assertions, the corrected helper passed all three backends (`gpu-picking-final.xml`). This is 174 passes plus 3 targeted rerun passes, not a single full-suite green run. Policy scan, four strict OpenSpec validations and exact commit lists are complete; the original index is unchanged. Commit/archive still require user confirmation.


## 2026-10-26 规范接续复核

已逐项修正相关主 spec 与 active delta 的现行契约，并将已存在的 ADDED/RENAMED/REMOVED 操作重设为可再次合并的 delta。只读合并检查与 strict 校验分开记录；未执行 archive。历史平台结果不覆盖后续四组/typed selection/metadata owner 修改。完整范围、证据和剩余项见 `docs/aurora/consistency-review-2026-10-26.md`。

## Two-commit delivery snapshot

- [x] 3.1 Build the material/Shader prerequisite snapshot after the three committed prerequisites, including generic reflection ownership and excluding animation integration.
- [x] 3.2 Run framework, metadata/asset/editor and compiler lifetime regressions in that snapshot.
- [x] 3.3 Verify spec applicability, patch reconstruction and unchanged primary index.

Earlier integrated validation above is historical evidence; it does not substitute for the isolated first snapshot. Exact current results are in verification.md. No commit/archive approval exists for either remaining batch.
