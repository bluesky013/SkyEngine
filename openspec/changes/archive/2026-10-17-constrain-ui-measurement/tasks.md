## 1. Add pure constraints and the element entry point

- [x] 1.1 Add engine/ui UISize/UIConstraints normalization, deflation, finite Constrain, and pure ResolveMeasureConstraints utilities with UI.Interface-only dependencies.
- [x] 1.2 Add non-virtual MeasureDesired, protected OnMeasure, and MeasureForLayout; preserve the one-way legacy bridge and clamp callback output centrally.
- [x] 1.3 Resolve known own Fixed/Fill/Percent/anchor axes before callbacks and use the same budget in Layout, preserving Fixed overflow versus parent-tight-slot authority.
- [x] 1.4 Sanitize padding and keep excessive-padding ContentRect empty within bounds; keep all measurement queries free of layout/tree/scroll/dirty mutations.

## 2. Propagate budgets through containers and scrolling

- [x] 2.1 Migrate HBox/VBox to rule-aware measurement and nonoverlapping remaining Fill allocation.
- [x] 2.2 Migrate StackPanel/DockPanel/GridPanel/Overlay constrained measurement and final parent-owned Arrange paths without resolving child rules twice.
- [x] 2.3 Add ScrollAxes Both/Horizontal/Vertical/None, viewport-reference content-rule resolution, and hard viewport maxima on non-scrolling axes.
- [x] 2.4 Align ScrollView content extent, final arranged rectangle, and clamped offsets; clear cached content pointers after inherited child removal/clear.
- [x] 2.5 Keep ListView measurement independent of factories/live rows and arrange visible items into authoritative final row rectangles.
- [x] 2.6 Measure AUTO ScrollView viewports in two stages, freezing candidate width for Vertical/Both/None or candidate height for Horizontal before remeasuring the dependent axis.

## 3. Add consistent wrapped text and an editor consumer

- [x] 3.1 Add default-off byte-wise soft wrapping through one shared line-layout algorithm used by Text measurement and paint.
- [x] 3.2 Honor Text padding and per-line alignment at the effective content width while retaining old no-wrap text/UiDraw entry points.
- [x] 3.3 Expose boolean text.wrap and registered scalar properties through generic nested JSON props dispatch to SetProperty; preserve C++/loader parity and existing layout/visual/button specialized parsing.
- [x] 3.4 Measure AssetViewer custom content at the actual column width including scrollbar reservation and arrange using the resulting height.

## 4. Verify and document measurement behavior

- [x] 4.1 Add pure constraint normalization/deflation/clamp tests including invalid values, infinities, contradictory bounds, and excessive padding.
- [x] 4.2 Add legacy/new-hook/rule-budget tests for no recursion, hard tight slots, ordinary Fixed overflow, finite unbounded Fill/Percent, and measurement state preservation.
- [x] 4.3 Add nested container and scroll/list regressions covering all axis modes, extent/rectangle/clamp agreement, removed content, and no factory calls during measurement.
- [x] 4.4 Add shared-line-layout/Text padding/alignment/no-wrap/soft-wrap and JSON/C++ property regressions, plus actual-width AssetViewer integration coverage.
- [x] 4.5 Build and run the relevant UI core/text/data/editor targets and record unavailable platform validation rather than marking it passed.
- [x] 4.6 Update the layout/chrome plan's measurement/scroll/text contracts, inspect contract/runtime/backend target boundaries, and run strict OpenSpec validation for constrain-ui-measurement.
- [x] 4.7 Add regressions for Percent content in AUTO ScrollView candidate viewports and exactly fitting wrapped text at fractional origins, including a meaningful narrower-width control.
- [x] 4.8 Add generic nested scalar type, single specialized-group dispatch, JSON/C++ Text measure/paint parity, default wrap=false, and unknown/unsupported property regressions.


## Verification record (2026-10-16, macOS)

- Configured the existing Ninja Release `build` checkout and built `UICoreTest`, `UITextTest`, `UIEditorTest`, `EditorWidgetsTest`, `EditorShellTest`, `EditorRender`, `Aurora.Render.Editor`, `SandboxModule`, `Content.Views`, and `SandboxEditor`.
- Runtime/editor regression results: UICoreTest 122 passed; UITextTest 16 passed; UIEditorTest 3 passed; EditorWidgetsTest 14 passed; EditorShellTest 37 passed. UIShaderTest 6 passed, 3 DXIL cases skipped on this platform. Total: 198 passed, 3 skipped.
- Coverage includes invalid/contradictory/unbounded constraints, pure padding operations, one-way legacy callbacks, Fixed overflow versus authoritative slots, container width-dependent budgets, all scroll axes and AUTO viewport remeasurement, list factory/dirty-state preservation, actual wrapped glyph placement, fractional-origin exact fits, JSON/C++ measurement and paint parity, and AssetViewer scrollbar-column/resize integration.
- An isolated C++17 translation unit including only `engine/ui/include` compiled successfully with UISize/UIConstraints/ResolveMeasureConstraints. Reconfigured the Sandbox=OFF, Test=ON, FreeType=OFF headless checkout at `/private/tmp/sky-ui-headless-build` and built its UI.Interface target. This is not a full headless runtime build.
- At this checkpoint, engine UI contained contracts, referenced data and pure utilities while runtime measurement/container/text/document implementations lived under `plugins/ui/core`. No new renderer/editor/backend dependencies were introduced in UI.Interface or UI core.
- Strict OpenSpec validation and `git diff --check` passed. Win32/VS2022 and interactive GPU/window visual validation were not run. Existing third-party macOS deployment-target and duplicate-link-library warnings remain in the build.
- No commit or OpenSpec archive was performed; both still require explicit user confirmation.

Placement update (2026-10-17): runtime implementation now lives in `engine/ui/core` under the user's UI foundation exception; root `UI.Interface` remains the pure contract target. The historical counts/builds above predate that relocation. Current relocation/review verification is recorded separately in `simplify-ui-foundation-boundaries` and canonical §19.
