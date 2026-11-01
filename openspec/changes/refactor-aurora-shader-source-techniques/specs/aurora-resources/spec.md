## MODIFIED Requirements

### Requirement: Generated Material parameter keys

Material logical keys SHALL be generated from the stable Group 2 declaration union of its techniques and Shader-local option schemas. Keys SHALL distinguish fields, textures, samplers and options, retain semantic types and avoid persisting target byte offsets as authoring schema. Cross-technique shared keys SHALL agree on semantics/type/shape. Display names SHALL NOT create implicit aliases. Existing aliases SHALL migrate values explicitly and incompatible mappings SHALL fail diagnostically.

#### Scenario: Technique subset
- **WHEN** one technique uses only part of the Material's logical key table
- **THEN** other known keys remain stored without requiring active bindings for that technique

### Requirement: One logical Material cbuffer

A linked program family SHALL declare zero or one logical Group 2 cbuffer after merging stages. Repeated references to the same block SHALL count once; distinct blocks SHALL fail even if only one is active in a particular choice. Target stage ABI for the shared block SHALL be compatible. Pass/Batch groups SHALL retain their own resource contracts.

#### Scenario: Two material blocks
- **WHEN** Group 2 declares two distinct buffers
- **THEN** schema validation rejects the program before publication

### Requirement: Inactive Material values remain valid

Known inactive keys SHALL retain values and skip only current target writes. Unknown keys, semantic type errors and missing required active values/resources SHALL fail. Required resources SHALL be determined by the actual target contract, not inferred from a false option. Texture residency SHALL NOT implicitly change static options. MaterialInstance SHALL keep value/texture/sampler overrides without new static-option overrides.

#### Scenario: Disabled texture option
- **WHEN** a texture option is false but the native artifact still requires the texture
- **THEN** binding supplies a valid resource or explicit default, or reports missing-resource

#### Scenario: Inactive wrong type
- **WHEN** an inactive known field receives the wrong semantic type
- **THEN** validation still rejects it

### Requirement: Material 内部按 technique 维护

Material SHALL own tagged MaterialTechnique records containing an explicit ShaderTechnique program (stage/source/entry), checked legacy aliases and material render state. Scene queues SHALL select matching tags; fullscreen and compute SHALL select programs explicitly without requiring Material. Empty or duplicate Material technique tags SHALL fail. MaterialRenderState SHALL contain depth/stencil, raster, blend and alpha-to-coverage state, excluding mesh vertex layout/topology and Pass sample count. Shader resolution SHALL retain the selected program rather than substitute an implicit built-in shader.

#### Scenario: Tagged stage programs
- **WHEN** opaque and shadow techniques reference different explicit stage programs
- **THEN** each queue uses its matching program and material state

#### Scenario: Invalid tag
- **WHEN** a technique has an empty or duplicate tag
- **THEN** adding it fails without changing the existing techniques
