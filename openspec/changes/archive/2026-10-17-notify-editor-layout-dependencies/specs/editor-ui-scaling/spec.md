## ADDED Requirements

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
