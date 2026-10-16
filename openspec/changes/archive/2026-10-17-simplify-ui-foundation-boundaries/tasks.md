## 1. Foundation ownership

- [x] 1.1 Move UI implementation targets to engine/ui core/render/freetype/editor while retaining public target/include names.
- [x] 1.2 Remove plugins/ui registration and update configured test/tool/current documentation paths and foundation specs.
- [x] 1.3 Preserve moved non-CMake bytes and keep render/backend behavior and resource contracts outside the edit scope.

## 2. Exception and callback lifetime review

- [x] 2.1 Remove broad Session/FileStorage/Any and codec-operation catches; use RAII/error-return APIs and narrow existing provider failure boundaries.
- [x] 2.2 Own capture descriptors and Session source pages across reflected-copy reentry; preserve replacement sessions/history during Synchronize/Apply.
- [x] 2.3 Guard shell/dialog continuation when history observers destroy or replace Preferences owners.
- [x] 2.4 Route native floating-window key/text to existing surface APIs without changing renderer/window ownership.
- [x] 2.5 Add five copy lifetime/reentry regressions and a shell reopening destruction regression; retain prior provider exception tests.

## 3. Verification and evidence

- [x] 3.1 Build/run affected normal suites including optional FreeType, and six ASan suites; record exact results.
- [x] 3.2 Run bounded module source syntax verification; record Windows/native/display/GPU checks separately.
- [x] 3.3 Verify moved/protected bytes, strict active OpenSpec validation and tracked/untracked whitespace.
- [x] 3.4 Update canonical review/task evidence, preserving historic checkpoints and pending platform gates.

Commit and OpenSpec archive require separate explicit user confirmation.

Execution evidence (2026-10-17): normal and ASan targeted builds passed after relocation. Both ran EditorCoreTest 165, EditorWidgetsTest 33, EditorShellTest 128, UICoreTest 185, UITextTest 25 and UIEditorTest 3 (539 per profile). Normal additionally built/ran UITextFreeTypeTest 2, totaling 541. The six new cases cover copy-source/registry/session/history reentry and shell destruction during reopening. ASan used `ASAN_OPTIONS=detect_leaks=0` with no reported errors. The configured SandboxModule source passed `-fsyntax-only`; existing warnings remain, and this is not a native link/run result.

Scope evidence: all 71 moved non-CMake files retain their prior bytes. Of 262 protected paths, 261 retain identical bytes after path mapping; the remaining renderer CMake file changes only its IDE folder label. No missing protected file or implementation mutation was found. All 23 active UI changes pass strict validation. Windows file replacement, actual native floating keyboard/text, display/DPI/GPU and the eight stored-only settings consumers remain separate pending work in canonical §12.3/T08 and §19.

Final review found no additional actionable issue in 150 changed/untracked production C/C++ files and related build configuration: no new catch outside the reviewed Preferences boundaries, cross-module static registration guard, stale plugins/ui build/code path or UI dependency reversal. Tracked whitespace and all 233 untracked files pass whitespace checks. Historic baseline findings outside this change were not silently rewritten.
