## MODIFIED Requirements

### Requirement: aurora cook 模块与 builder 注册

AuroraCook.Static SHALL retain the offline builder implementation and Aurora.Cook its module entry. Aurora.Cook SHALL depend on Aurora.Metadata and SHALL NOT invoke AuroraReflection. Its RegisterMeta SHALL register image cook settings and image/mesh/material/material-instance/LOD/skeleton builders once per process definition. Init/Shutdown SHALL NOT repeat persistent registration. Existing tool/editor build and module manifests SHALL load the declared metadata dependency before its consumers.

#### Scenario: Cook and runtime share Aurora metadata
- **WHEN** Aurora.Cook and Aurora.Render coexist or restart in the same process
- **THEN** shared asset handlers remain owned by Aurora.Metadata and builders remain registered once by Aurora.Cook
