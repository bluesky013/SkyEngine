## ADDED Requirements

### Requirement: Coherent domain-aware material preparation

Scene preparation SHALL derive V from the actual draw's input description, receive Pipeline values and Material/Batch Shader-local values, then use the common selector. Input mode and palette contracts SHALL be validated. Resource parameters and variant values SHALL be separate. Prepared versions SHALL distinguish full selections and relevant revisions, retaining previous in-flight resources until retirement.

#### Scenario: Shared Material across inputs
- **WHEN** two draws share a Material but have different valid V/P/S selections
- **THEN** each uses its own prepared execution and compatible bindings

### Requirement: Generated schema material migration

Material construction/serialization SHALL preserve explicit stage program references, authored values, render state and generated semantic key metadata. Existing properties/aliases SHALL migrate through explicit mappings without losing dormant values. Changed formats SHALL reject incompatible payloads with migration/recook diagnostics. Generic reflected editor integration SHALL consume the generated schema rather than per-field custom controls.

#### Scenario: Ambiguous legacy alias
- **WHEN** old techniques map one stored property to incompatible semantic targets
- **THEN** migration names the conflict and does not silently discard or guess a value

## MODIFIED Requirements

### Requirement: CreateMaterialFromAsset / CreateMaterialInstanceFromAsset

The existing creation helpers SHALL retain checked tagged programs and typed properties rather than the removed single ShaderRef technique shape. Texture UUIDs SHALL resolve through AssetManager with explicit failure for invalid resources; missing Device or invalid data SHALL return an empty result. Production SceneAssetSource SHALL additionally resolve complete logical Material schema from source or product metadata and freeze named values, explicit selected variants and exact resource dependencies before GPU preparation. Asset conversion SHALL NOT prepack a target's uniform bytes.

#### Scenario: Valid material conversion
- **WHEN** valid typed data is converted with a compatible device
- **THEN** the Material retains its explicit technique programs and property values

#### Scenario: Invalid creation input
- **WHEN** Device is absent or a required asset/value cannot be resolved
- **THEN** conversion returns failure without publishing a partially valid material

### Requirement: MaterialInstance 资产（父材质 + 覆盖 + 版本）

MaterialInstanceAssetData SHALL persist its supported version, parent Material UUID and typed sparse value/texture/sampler overrides. Unoverridden fields SHALL inherit the parent; explicit equal-to-default writes SHALL still count as overrides. Instance preparation SHALL validate canonical names and types against the parent schema and SHALL NOT allow static Shader-local option changes. Unsupported versions or invalid overrides SHALL fail without publishing a partial instance.

#### Scenario: Sparse override
- **WHEN** an instance overrides one canonical value and texture
- **THEN** those values are retained and all other fields inherit the parent

#### Scenario: Static option override
- **WHEN** an instance attempts to override a Shader-local static option
- **THEN** preparation rejects it explicitly

### Requirement: Material 资产（typed 属性 + technique + 版本）

MaterialAssetData SHALL persist versioned explicit tagged ShaderTechnique stage/source/entry references, material render state and typed canonical Key/Value properties. Values SHALL use the checked semantic variant, textures SHALL use asset UUIDs and samplers SHALL use typed descriptors; serialization SHALL NOT copy C++ object padding. Complete Material keys SHALL be reconstructed from declaration metadata and Shader-local options. The current payload is version 4; supported legacy version 3 aliases/options SHALL migrate through exact declarations, while unsupported versions and ambiguous mappings SHALL fail with recook/migration diagnostics.

#### Scenario: Typed round trip
- **WHEN** a valid material with explicit programs, values, texture UUIDs and samplers is saved and loaded
- **THEN** tags, stage references, render state and semantic property values remain unchanged

#### Scenario: Legacy alias is ambiguous
- **WHEN** an old key cannot map uniquely to the current complete schema
- **THEN** preparation rejects the migration without guessing a destination
