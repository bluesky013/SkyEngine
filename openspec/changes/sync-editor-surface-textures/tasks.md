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
- [ ] 3.5 Run real GPU validation for floating icons/wheel, late changes/removal, and close/reopen. Keep this visual gate pending when only CPU/static checks are available.

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
