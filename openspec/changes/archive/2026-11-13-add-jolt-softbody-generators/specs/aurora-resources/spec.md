## ADDED Requirements

### Requirement: Four influence deformation preserves shading and bounds
Generated deformation bindings SHALL preserve the existing maximum of four positive influences per vertex. Positions, shadow positions, normal contributions and bounds SHALL consume compatible complete versions at the same presentation time. Translation-only bone positions SHALL not be accepted as sufficient evidence of correct deformed normals.

#### Scenario: Affine deformation of a bound surface
- **WHEN** a generated cage undergoes a known affine compression or rotation
- **THEN** displayed positions and normal directions match the declared embedding and shading model, and dynamic bounds contain the result

#### Scenario: Render a textured generated mesh
- **WHEN** a generated mesh contains asset UV channels and its material consumes canonical shader TEXCOORD inputs
- **THEN** the render bridge preserves the channel mapping without changing source serialization and rejects ambiguous channel indices before publishing render resources

### Requirement: Generated mesh crowds use explicit bounded renderer capacity
Renderer hosts SHALL be able to configure per-frame resource descriptor, sampler descriptor and descriptor-set budgets through the existing renderer initialization interface. Defaults SHALL remain 1024, 128 and 256 respectively, with supported values in `[1, 65536]`. The existing launcher module SHALL accept separate decimal arguments `--aurora-resource-descriptors`, `--aurora-sampler-descriptors` and `--aurora-descriptor-sets`; malformed, duplicate, missing or out-of-range values SHALL fail initialization without publishing a renderer. Backend default budgets, mesh primitives and shadows SHALL not be changed implicitly to accommodate a crowd.

#### Scenario: A crowd exceeds the default descriptor-set budget
- **WHEN** a project launcher explicitly configures sufficient capacity for its complete main and shadow draw workload
- **THEN** the renderer applies those limits to its existing frame context and recycles descriptors only on a new retired-slot lease, allowing repeated frames without cumulative descriptor consumption

#### Scenario: A launcher supplies an invalid capacity
- **WHEN** a known descriptor-capacity flag is repeated, has no separate value, uses `--flag=value`, or supplies a value outside the supported decimal range
- **THEN** initialization returns an explicit error before renderer creation and does not silently use a default budget
