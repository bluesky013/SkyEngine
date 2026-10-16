## 1. Borrowed registry lifetime

- [x] 1.1 Add idempotent UIFontAtlas removal without invoking or releasing the detached registry.
- [x] 1.2 Forward removal through UITextSystem, clear a removed default, and support reattachment replay.
- [x] 1.3 Detach renderers on explicit surface destruction, OS-close, and global shutdown before destruction.

## 2. Regression and verification

- [x] 2.1 Add persistent regressions for duplicate add/remove, unknown/null removal, replay, and preserving live registry updates, and actual current page pixels after reattaching a backend retaining an old page.
- [x] 2.2 Add a regression destroying a detached registry before new glyphs and atlas-page growth.
- [x] 2.3 Build affected UI/editor consumers and run the relevant tests.
- [x] 2.4 Run the lifetime reproduction under ASan and record the result.
- [x] 2.5 Update task status/documentation, run strict OpenSpec validation and whitespace checks, and record unavailable platform/GPU checks without committing or archiving.

Verification (2026-10-16): full macOS `cmake --build build --parallel 4` passed, including
UI/UIRender/EditorRender/SandboxModule and all current runtime modules. UITextTest passed all
25 tests normally and with AddressSanitizer (`ASAN_OPTIONS=detect_leaks=0`); this includes
registry detach/destruction, atlas growth, and current-pixel replay on reattachment. The isolated
Sandbox=OFF/FreeType=OFF full build and its 25 text tests also passed. These totals include T07's
shared-image regressions rather than representing 25 independent font-lifetime cases.
Strict OpenSpec and whitespace checks passed. Real floating-window GPU checks and Win32/DXIL
remain tracked in T08; no commit/archive was performed.
