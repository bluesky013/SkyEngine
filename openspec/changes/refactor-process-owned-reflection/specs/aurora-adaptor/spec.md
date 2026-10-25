## MODIFIED Requirements

### Requirement: AuroraReflection 入口

AuroraReflection SHALL register Aurora schema, asset handlers and components exactly once from Aurora.Metadata::RegisterMeta during process definition creation. Runtime, cook and editor modules SHALL depend on that owner; views SHALL only bind registered data. The function SHALL NOT be idempotent through Once, once_flag, static flags, existing-type checks or scratch nodes. A second registration owner SHALL remain a programming error caught by strict UUID/name checks.

#### Scenario: Runtime cook and editor share one definition
- **WHEN** these consumers load, restart or open several views in one process
- **THEN** AuroraReflection has one production call site and each schema is registered once

#### Scenario: Duplicate registration is rejected
- **WHEN** another owner attempts to register the same type UUID or name
- **THEN** the programming invariant fails rather than silently accepting or skipping the duplicate
