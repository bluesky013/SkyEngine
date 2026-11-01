## MODIFIED Requirements

### Requirement: Content 模块与 adaptor / builder 分层

Content SHALL retain its existing adaptor, offline builder and runtime targets. Content.Metadata SHALL own ContentReflection and persistent handlers for existing Content asset schemas once per process definition. Content.Builder.Module SHALL depend on metadata and register its builders through RegisterMeta; it SHALL NOT require the runtime service for reflection. Content.Module and Content.Views SHALL depend on metadata and SHALL NOT invoke ContentReflection. New asset schemas, providers or ownership transfers SHALL be delivered with their feature implementation.

#### Scenario: Offline cook without runtime service
- **WHEN** Content.Metadata and Content.Builder.Module load without Content.Module
- **THEN** the existing asset schemas, handlers and builders are available without starting Content runtime services

#### Scenario: Runtime session restart
- **WHEN** Content.Module shuts down and initializes again
- **THEN** the same shared metadata definition remains registered without re-running ContentReflection
