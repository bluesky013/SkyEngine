> 2026-11-01 整理：**partially-superseded**。现行 owner：共享 CPU 图片目录契约仍有效；GPU/浮窗验收归 `aurora-renderer-platform-parity` P02-EDITOR。
> 剩余：3.5 保留未验收；旧 inflight=1/BeginFrame GPU 段不得原样同步，先保留 CPU 契约再按 owned-frame 机制整合。下方较早状态与验收记录保留为历史。

## 1. Shared CPU image ownership

- [x] 1.1 Add a backend-free shared-image catalog with explicit id domains, RGBA8 validation, initial replay, and live registration/update/removal.
- [x] 1.2 Integrate UITextSystem attachment/detachment while preserving GetRegistry and providing a separate shared-image entry point.
- [x] 1.3 Route SVG icons, color-wheel registration, and the reflection-demo owned image through the shared entry point; preserve wheel reset/reopen behavior.

## 2. GPU generation lifetime

- [x] 2.1 Queue CPU updates without mapping/releasing submitted resources and create a new image/staging/group generation during upload.
- [x] 2.2 Retain released/replaced generations until a host-provided completed-frame collection point; include RegisterImage replacement.
- [x] 2.3 Integrate main/live-floating collection immediately after the inflight=1 frame wait, including minimized/acquire-failure paths.

## 3. Verification and platform gate

- [x] 3.1 Add fake-registry coverage for replay, late images, resized updates, remove/recreate, reset/re-ensure, detach/destruction, and resource/group isolation.
- [x] 3.2 Run an isolated ASan shared-catalog detach/destroy/update/glyph-growth reproduction.
- [x] 3.3 Build UI/UIRender/editor consumers and run relevant text/widget regressions, including the existing ColorPicker reset test.
- [x] 3.4 Validate this change strictly and check whitespace; record exact checks without committing or archiving.
- [ ] 3.5 Transferred, not accepted: `aurora-renderer-platform-parity` P02-EDITOR owns real floating icons/wheel, late changes/removal and close/reopen validation under the current owned-frame protocol.

ASan evidence (2026-10-16): a standalone C++17 probe built from UISharedTextureRegistry,
UITextSystem, UIFontAtlas, and UIBuiltinFontProvider passed with `ASAN_OPTIONS=detect_leaks=0`.
It attached a second registry, detached/destroyed it, resized/updated a shared image, grew atlas
pages, replayed into a replacement registry, and removed the shared image. Leak detection is not
supported by the current macOS ASan runtime; this records address-safety coverage only.

`OPENSPEC_TELEMETRY=0 openspec validate sync-editor-surface-textures --strict` and
`git diff --check` passed. Runtime integration is recorded below; the real GPU/display gate remains pending above.

Commit and archive require user confirmation.

Integration verification (2026-10-16): the full macOS build (including UIRender, EditorRender,
SandboxModule and renderer consumers) passed. UITextTest 25 and EditorWidgetsTest 28 passed
normally and with AddressSanitizer (`detect_leaks=0`), including the ColorPicker reset/reopen
regression. The text total includes T01 and the full-range texture-id regression. The isolated
Sandbox=OFF/FreeType=OFF full build and text suite also passed.
Native startup was retried after rebuilding stale runtime modules: exit 0, but Metal device
creation failed, so no rendered frame/visual GPU success is claimed. Task 3.5 remains open.

## 2026-11-01 契约接续

`UITextSystem::GetSharedRegistry`、CPU 图片 replay/update/remove、ID 隔离与 detach 安全仍由现有 `UISharedTextureRegistry` 和 `UITextTest` 覆盖，不能因 GPU 实现替换而丢弃这两条 CPU 契约。GPU 端已改为 `UiSnapshotProducer` 发布精确版本的 owned payload / released resources，再由 renderer 保活退休；旧 `UIRenderer` 的 inflight=1 BeginFrame collection 段只是历史实现，不能原样同步。P02-EDITOR 接收尚未执行的浮窗视觉矩阵。
