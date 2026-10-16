## 1. Generic reflection and real file persistence

- [x] 1.1 Resolve effective field order as explicit appearance > reflected ORDER > zero, stably preserving existing reflection iteration (currently key order) before root/nested field construction.
- [x] 1.2 Implement FilePreferenceStorage with distinct absent/read errors, complete same-directory temporary writes, safe replacement, failure byte preservation and temporary cleanup.
- [x] 1.3 Add real temporary-file and generic metadata regressions without changing Framework ordering ABI or the prior checked JSON codec contract.

## 2. Host registration, application and bounded wiring

- [x] 2.1 Own built-in page registrations by exact identity, reject unrelated reserved-id conflicts and remove only owned registrations on reset/destruction, retaining plugins/newer owners.
- [x] 2.2 Wire built-in/contributor registration before generic Load, load hub settings, inject shared persistence into Shell/Dialog and replace the legacy Save-only preference hook.
- [x] 2.3 Separate main/floating device DPI from the user multiplier, apply one normalized/clamped product policy, connect editor.uiScale through startup/page application, and establish the target theme/layout before pointer/key/text input.
- [x] 2.4 Preserve all editable/stored fields while exposing the other eight built-in settings as stored-only through page/field metadata; do not claim restart or rendering effects with no consumer.
- [x] 2.5 Restrict SandboxModule/header edits to authorized settings wiring/lifetime declarations plus forwarding the existing GUI callback's surfaceId through PaintGui to EditorShell::PaintSurface; preserve surrounding renderer initialization/tick/surface creation/interfaces/resources/refactor and Core Event scope.

## 3. Frozen remaining-screen composition

- [x] 3.1 Migrate FileBrowserDialog outer regions/control slots through shared containers, retaining specialized rows/popups and browser model/business behavior.
- [x] 3.2 Migrate NewWorldDialog panel/header/field-row/footer/control allocation through shared containers, retaining text editing/create/cancel behavior.
- [x] 3.3 Migrate ProjectManagerView banner/top actions/selection actions/header/list/status slots through shared containers, retaining recent-project/selection/confirmed-action behavior.
- [x] 3.4 Migrate WorldConfigPanel outer header/list/form allocation through shared containers, retaining generic reflected fields, document/provider lifetime and subsystem enable/save behavior.
- [x] 3.5 Use arranged compatibility queries, explicit logical sizes, theme metrics, viewport-clipped hits and bounded small/degenerate layouts; keep Paint free of layout/tree repair.

## 4. Regression and scope closeout

- [x] 4.1 Cover field order overrides/reflection/ties/nesting, file persistence success/failure/cleanup and built-in/plugin registration ownership.
- [x] 4.2 Cover checked Load of legacy JsonOutputArchive bytes and real-file injected persistence, result-aware Apply, main/floating user×DPI scale, cross-window pointer/key/text/modal input and stored-only settings presentation.
- [x] 4.3 Cover each frozen view's first allocation/input before Paint, resize/theme/small-window/clipped queries and representative preserved business/lifetime paths.
- [x] 4.4 Build/run affected targeted portable normal/ASan suites and bounded settings-source integration checks; record actual commands/counts and separate unrun native execution.
- [x] 4.5 Strictly validate the change and review tracked/untracked whitespace, the narrow settings exception and unchanged renderer/RHI/RDG/backend/Core Event scope.
- [x] 4.6 Update canonical T11 status/inventory/application mapping with implementation evidence, remaining stored-only consumers and open Windows/native/display/GPU gates.

Implementation and portable acceptance are complete (2026-10-17). Normal and ASan each passed all six suites: Core 160, Widgets 33, Shell 127, UICore 185, Text 25 and UIEditor 3, totaling 533 per profile, including 42 new cases. ASan used `ASAN_OPTIONS=detect_leaks=0` and reported no errors. Targeted builds and the bounded SandboxModule settings-source syntax check passed; no native link/run was performed. All 22 active UI changes passed strict validation, tracked plus 289 untracked files passed whitespace checks, and 262 protected-file hashes stayed unchanged; the two settings-only SandboxModule exception files were reviewed separately.

T11 platform acceptance remains pending in canonical §12.3/T08 for Windows/native/display/cross-DPI/GPU execution. Eight built-in setting consumers remain explicitly stored-only follow-up features. Commit and OpenSpec archive require separate explicit user confirmation.
