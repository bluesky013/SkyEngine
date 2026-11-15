## ADDED Requirements

### Requirement: Aurora persistent data has an independent implementation owner
Aurora asset schemas, codecs, pure material definition processing, asset handlers and persistent component schemas SHALL be compiled by one Aurora.Assets.Static target. Cook and Adaptor SHALL consume that implementation through links. Assets SHALL NOT depend on Adaptor, Pipeline, Editor, Cook or concrete RHI backend implementations. GPU resource creation and World component behavior SHALL remain outside Assets.

#### Scenario: Offline producer shares persistent formats
- **WHEN** Aurora Cook compiles meshes, images, materials or bindings
- **THEN** it uses the shared persistent implementation without linking Aurora.Adaptor.Static
- **AND** each production source has exactly one compilation owner

#### Scenario: Existing world and assets reopen
- **WHEN** existing Aurora products or persisted components are read after the ownership split
- **THEN** their serialized fields, UUID references and component identities remain compatible
- **AND** runtime conversion still creates resources through the existing runtime owner
