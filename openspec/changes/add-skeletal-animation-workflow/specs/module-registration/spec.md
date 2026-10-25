## MODIFIED Requirements

### Requirement: Shared metadata module consumers

Aurora.Metadata, Content.Metadata and Animation.GraphAssets SHALL own their shared reflection and persistent asset handlers. Runtime, builder and editor modules SHALL declare dependencies on the matching owner. Content.Module SHALL publish the Clip source service through Init/Shutdown and SHALL NOT call ContentReflection. Builder registration SHALL have one process definition owner and retain its callback code; runtime sessions SHALL NOT repeat registration.

#### Scenario: Content runtime and cook coexist
- **WHEN** Content.Module and Content.Builder.Module load in one process
- **THEN** Content.Metadata registers the shared Clip schema and handler once, while the builder module registers its builder once through RegisterMeta
