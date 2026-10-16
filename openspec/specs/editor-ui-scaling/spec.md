# editor-ui-scaling Specification

## Purpose
TBD - created by archiving change editor-command-registry. Update Purpose after archive.
## Requirements
### Requirement: Single UI style source
The editor UI dimensions and typography SHALL come from a single theme (`UiTheme` with `UiMetrics` / `UiFonts`);
views SHALL derive sizes from the theme rather than hard-coding pixel layouts.

#### Scenario: Derived size
- **WHEN** a view needs a row height / padding / field height
- **THEN** it SHALL read it from the theme metrics (or an expression of them), not a literal

### Requirement: Uniform scaling
The theme SHALL provide a scale; applying it SHALL multiply every metric and font size together, so widget
sizes and text scale uniformly.

#### Scenario: Scale doubles
- **WHEN** the theme is built at 2× scale
- **THEN** every metric (e.g. row height) and font size SHALL be doubled

### Requirement: Chrome scales with DPI
The menu bar, toolbar, and status bar heights SHALL be theme metrics so the editor chrome scales with DPI like
the rest of the UI.

#### Scenario: High-DPI chrome
- **WHEN** the effective scale is above 1 (e.g. a 150% display)
- **THEN** the menu bar, toolbar, and status bar SHALL grow with the theme scale, not stay at fixed pixels

### Requirement: Effective scale from DPI and preference
The effective scale SHALL be the system DPI scale times a user preference (`editor.uiScale`), with an
environment override for development, and SHALL apply to the editor and the Project Manager hub.

#### Scenario: User preference multiplies DPI
- **WHEN** the display DPI scale is 1.5 and the user sets UI Scale to 1.25
- **THEN** the effective scale SHALL be 1.875

### Requirement: Reset Layout restores the built-out default
Resetting the layout SHALL restore the full default arrangement that was built (splits + panels), not a single
initial panel.

#### Scenario: Restore arrangement
- **WHEN** the default layout has been built and captured, the user rearranges/closes panels, and chooses
  Reset Layout
- **THEN** the full default arrangement SHALL be restored

### Requirement: Default theme revisions invalidate complete surface layout
The shared default UiTheme SHALL retain its stable address and expose a process-shared revision that advances whenever SetDefaultUiTheme is called, including same-scale updates. Editor surfaces SHALL select their effective DPI theme before layout synchronization and SHALL apply a newly observed revision by invalidating layout/measurement state for every live element in that surface before incremental reuse. Invalidating only the root SHALL NOT be considered sufficient. Main/floating surfaces SHALL independently track the revision applied to their layout. Independently mutable custom themes require owner notification or conservative volatility.

#### Scenario: Another DLL updates the theme
- **WHEN** an editor module sets the default theme through its linked EditorUi copy
- **THEN** other editor modules SHALL observe the same theme value/address and advanced revision

#### Scenario: Metrics change at the same scale
- **WHEN** the default theme's metrics or fonts change at the same scale after a clean layout
- **THEN** the next surface layout SHALL invalidate nested cached sizes/arrangements and update affected geometry even if outer rectangles remain unchanged

#### Scenario: Main and floating surfaces use different DPI
- **WHEN** main and floating surfaces select different effective DPI themes during successive paints
- **THEN** each surface SHALL apply the active theme revision before its own layout and SHALL NOT reuse nested geometry prepared under an incompatible theme

#### Scenario: Theme is unchanged between ordinary frames
- **WHEN** a surface receives the same theme revision and allocation inputs without a dependency mutation
- **THEN** theme handling SHALL NOT itself invalidate otherwise reusable layout

#### Scenario: Hidden view missed a live theme update
- **WHEN** a preserved view was detached during a theme change and is adopted back into a surface whose revision is already current
- **THEN** its entire subtree SHALL be synchronized before reuse so unchanged nested allocations cannot retain old-theme measurement results

#### Scenario: Main menu bar metrics change without rebuilding chrome
- **WHEN** the active theme changes menu bar height after main-window chrome construction
- **THEN** main layout SHALL update the menu bar's stored height before allocation so its paint, input, and popup origin use the active metric

### Requirement: Every editor surface multiplies device DPI by user scale once
Main and floating editor windows SHALL retain separate device-DPI and user-preference inputs. SetUiScale SHALL set the main device input, SetSurfaceScale the floating device input, and SetUserUiScale the user multiplier; effective scale queries SHALL use the same normalized product clamped to 0.5–4. The supported user preference range SHALL remain 0.5–2. A floating device override SHALL NOT bypass or duplicate the user multiplier. Startup/hub preference load and later Apply SHALL share this policy, retaining the developer environment override at the device-input boundary.

#### Scenario: Floating surface has a different DPI
- **WHEN** main/floating device scales differ and the same user multiplier is applied
- **THEN** each surface SHALL compute its own device×user result once rather than reuse an already-multiplied main value

#### Scenario: User multiplier changes after construction
- **WHEN** UI scale is applied from Preferences
- **THEN** the next main/floating layout SHALL use the updated effective scale and existing theme-revision invalidation

#### Scenario: Scale input is invalid or out of range
- **WHEN** a supplied scale is nonfinite or outside its supported range
- **THEN** normalization/clamping SHALL keep effective theme geometry finite and within the declared effective range

### Requirement: Surface presentation and input establish the target theme
The existing host GUI dispatch SHALL preserve its supplied surface identity through Shell presentation. Shell pointer, key and text entry points SHALL select that target surface's effective theme and prepare its layout before routing input, with existing owner/attachment lifetime guards. A geometry query without a supplied window context SHALL NOT be assumed to infer the target surface; the presentation/input host establishes that context.

#### Scenario: Main modal receives input after a floating paint
- **WHEN** a different-DPI floating surface paints and the main window next receives pointer, key or text input
- **THEN** input preparation SHALL select the main window's theme/layout before routing, including modal Cancel and field hit targets

#### Scenario: Host presents a floating surface
- **WHEN** the GUI callback is invoked for a floating surface
- **THEN** the supplied identity SHALL reach EditorShell::PaintSurface rather than presenting the main surface in that window
