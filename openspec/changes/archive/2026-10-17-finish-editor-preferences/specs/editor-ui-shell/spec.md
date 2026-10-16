## ADDED Requirements

### Requirement: T11 remaining-screen composition has a frozen four-view scope
FileBrowserDialog, NewWorldDialog, ProjectManagerView and WorldConfigPanel SHALL allocate their frozen outer regions/control slots through shared UI containers with theme inputs and explicit logical sizes. Compatibility geometry queries and pointer hit-testing SHALL consume arranged slots/visible viewports rather than independent dialog-level placement formulas. Paint SHALL consume prepared geometry. Small/degenerate allocations SHALL remain finite/nonnegative and long labels SHALL stay within their allotted clips. Specialized list-row/glyph painting and existing models/business operations MAY remain within their owning regions. This delivery SHALL NOT claim every editor screen is migrated.

#### Scenario: File browser regions are composed
- **WHEN** the browser first lays out or resizes
- **THEN** sidebar/navigation/list/footer/control queries SHALL follow shared-container allocation while navigation/filter/create/rename/accept/cancel and specialized popup behavior remain available

#### Scenario: New world fields and actions are composed
- **WHEN** New World lays out before first paint or after a theme/size change
- **THEN** name/location/Create/Cancel targets SHALL follow the arranged field/footer slots while existing text/path/create semantics remain intact

#### Scenario: Project Manager controls and recent viewport are composed
- **WHEN** the hub lays out or its geometry is queried before paint
- **THEN** banner/top actions/selection actions/header/list/status SHALL use arranged container slots, clipped rows SHALL not select through other regions, and existing recent/open/add/new/remove/confirmed-delete behavior SHALL remain

#### Scenario: World config keeps generic form behavior
- **WHEN** its composed list/form allocation changes or its document is rebound
- **THEN** the subsystem navigation and stable generic form SHALL use arranged regions while explicit document refresh/provider compatibility and subsystem enable/save behavior remain intact

### Requirement: T11 implementation and platform acceptance remain distinct
The closeout SHALL record the frozen inventory, actual settings-application matrix, narrow settings-only SandboxModule exception and executed portable/file/ASan checks. Windows/native module execution and real display/cross-DPI/GPU evidence SHALL remain explicit open gates when not run. Renderer initialization/tick/surface/refactor code, RHI/RDG/backends and Core Event SHALL remain outside the settings exception.

#### Scenario: Portable closeout succeeds without native GPU
- **WHEN** implementation and targeted portable checks complete on a host without a usable native device
- **THEN** T11 implementation closeout MAY be recorded with actual evidence while T11 platform acceptance remains unfinished
