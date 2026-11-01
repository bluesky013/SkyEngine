## RENAMED Requirements

- FROM: ### Requirement: Metadata owns callback code lifetime
- TO: ### Requirement: Loader owns process-resident callback code

## MODIFIED Requirements

### Requirement: Loader owns process-resident callback code

The loader SHALL keep successfully loaded DLLs resident until process exit, including runtime-only modules. Metadata definitions and callback registries SHALL NOT require per-object code tokens. Runtime shutdown SHALL withdraw services and drain accepted work before destroying their data and provider dependencies. Binary replacement or removal SHALL take effect after process restart; code residency SHALL NOT establish initialized-session readiness.

#### Scenario: Metadata survives session reload

- **WHEN** cook and editor runtime sessions are unloaded and recreated
- **THEN** existing reflected objects and builders SHALL remain usable with stable definition identity
- **AND** the underlying shared Environment SHALL remain attached

#### Scenario: Runtime-only compiler session stops

- **WHEN** a shader compiler module is shut down and its final external compile request is released
- **THEN** its compiler instance SHALL be released when its last real owner exits
- **AND** the DLL SHALL remain resident until process exit without a code lease
