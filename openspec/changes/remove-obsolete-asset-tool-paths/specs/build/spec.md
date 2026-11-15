## ADDED Requirements

### Requirement: Portable Ninja discovery for CPython
CPython bootstrap SHALL discover Ninja from an explicit NINJA executable override or PATH, without a developer-specific installation path.

#### Scenario: Explicit toolchain location
- **WHEN** NINJA identifies an available executable outside PATH
- **THEN** bootstrap selects that executable

#### Scenario: Invalid explicit override
- **WHEN** NINJA names an unavailable executable
- **THEN** discovery reports unavailable rather than selecting an unrelated installation

### Requirement: Supported asset tooling has one source entry
The repository SHALL retain AssetTool as the supported cook host and SHALL NOT retain the inactive AssetBuilder entrypoint or its otherwise unreferenced ToolApplicationBase.

#### Scenario: Framework and tools configure
- **WHEN** the supported Framework and AssetTool targets are configured
- **THEN** no removed ToolApplicationBase or AssetBuilder translation unit is compiled
