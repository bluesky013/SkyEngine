> Status: active; implementation authorized. Checkboxes remain pending until the corresponding implementation or validation is verified. This change does not authorize commit or archive.

## 1. Generic protocol and surface host

- [x] 1.1 Add optional UIElement tooltip query/build types with layout-coordinate anchors, stable target identity and weak binding lifetime.
- [x] 1.2 Add EditorWidgets TooltipHost with a 500 ms delay, deferred content construction and provider/attachment/binding revalidation.
- [x] 1.3 Implement input/drag/root/surface invalidation and non-interactive themed text layout, wrapping and bounded overlay placement.

## 2. First tooltip providers

- [x] 2.1 Expose reflected TOOLTIP metadata and invalidate targets on Bind, Unbind, Refresh and field replacement.
- [x] 2.2 Expose complete catalog-only asset path/type/status/GUID for asset picker entries and asset references without product loading; extra AssetBrowserPanel tooltips are outside this first provider set.
- [x] 2.3 Expose hovered picker result details with popup priority, visible clipping and stable identity across filtering/recycled rows.
- [x] 2.4 Migrate ToolBar action labels to the shared provider protocol and remove its local tooltip painter.

## 3. Shell and manual routing integration

- [x] 3.1 Own one host per main/floating surface and supply pointer, time, current bounds and effective input root.
- [x] 3.2 Integrate main modal, Preferences, floating-panel and independent AssetViewer query routes, including close prompts and popup priority.
- [x] 3.3 Clear state on press/scroll/key/text/cancel/focus reset, root rebuild, panel transfer, content rebind and surface destruction.
- [x] 3.4 Query after layout/theme synchronization and paint each tooltip last under its surface scale outside child clips.
- [x] 3.5 Add native Cocoa/Win32/X11 hover-leave notifications carrying WindowID and cancel only the matching surface tooltip without altering drag/capture or button state.

## 4. Validation and evidence

- [x] 4.1 Add/run focused UI protocol and host regressions for delay, stationary pointer, target change, geometry/clipping, weak lifetime and non-interactivity.
- [x] 4.2 Add/run provider regressions for reflection metadata, asset details, no asset load, picker filtering/rebind and toolbar migration.
- [x] 4.3 Add/run shell regressions for main/modal/floating roots, independent asset viewers, popup blocking, input reset and surface retirement.
- [x] 4.4 Build the affected real UI/editor libraries, SandboxModule and SandboxEditor with the existing host cache; run relevant existing suites and source-ownership checks if build organization changes.
- [ ] 4.5 Perform a bounded editor visual check for reflected/asset/picker/toolbar tooltips, edge wrapping and an independent floating surface; retain only key screenshots and record unavailable cases explicitly.
- [x] 4.6 Check scoped formatting, includes, forbidden patterns, whitespace and strict OpenSpec validation; update verification and tasks with actual results without commit/archive.
- [ ] 4.7 Run Cocoa native mouse-leave/window-identity/retained-view regressions and a real focused-window leave check; record Windows/Linux as unexecuted when those native environments are unavailable.

Actual results and remaining visual/platform checks: [verification.md](verification.md).

## 5. Review remediation
- [x] 5.1 Reuse UI hit-testing and prevent tooltip discovery through foreground siblings.
- [x] 5.2 Remove paint-time discovery and cover geometry/lifetime invalidation.
- [x] 5.3 Run affected UI and editor regressions and update verification.
