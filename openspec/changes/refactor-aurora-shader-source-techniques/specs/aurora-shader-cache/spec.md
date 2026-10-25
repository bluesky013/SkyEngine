## ADDED Requirements

### Requirement: Complete selection and target execution identities

Caching SHALL distinguish complete normalized SelectionKey, per-stage StageCodeKey and device-scoped Executable/PSOKey. Complete selections SHALL include every effective typed V/P/S value. Stage code identity SHALL include source closure, stage/entry, target environment, schema/toolchain/lowering versions and code-affecting values. Native values MAY share code but SHALL distinguish executions. Full identities SHALL be checked on hash hits; input order and explicit defaults SHALL not create different selections.

#### Scenario: Native code sharing
- **WHEN** two selections differ only in a native value
- **THEN** eligible code is shared and executable identities differ

#### Scenario: Vertex layout changes
- **WHEN** stride changes without changing Shader features
- **THEN** code remains reusable while the vertex-input PSO remains correctly distinguished

### Requirement: Generated selection and target metadata products

Versioned products SHALL contain logical schemas, complete allowed selection records, stage code references, native mappings and target reflection. Shipping SHALL normalize and resolve without sources/compilers, reject absent selections even when generic code exists and validate complete environment/schema identity. The baseline v9 protocol SHALL be upgraded when these encodings change; incompatible records SHALL require recook.

#### Scenario: Uncooked native choice
- **WHEN** reusable code exists but the full requested choice is absent from the published catalog
- **THEN** runtime reports missing-selection rather than accepting unvalidated values

### Requirement: Common immutable preparation

All consumers SHALL resolve normalized immutable choices before graph build/recording. Ready, pending and error SHALL remain explicit, and code/interface/bindings SHALL publish as a coherent generation. Existing task/snapshot/module/GPU lifetimes SHALL be preserved. Material UUID or Tag alone SHALL NOT identify one reusable program across different V/P/S choices.

#### Scenario: Stale completion
- **WHEN** an older preparation completes after a newer request
- **THEN** it cannot replace the newer generation or relabel old bindings

## MODIFIED Requirements

### Requirement: 变体 key 组装

Production selection SHALL use canonical typed V/P/S keys. Mesh capabilities supply Vertex values; pipeline supplies Pipeline values; Material/custom Batch/fullscreen/compute supply Shader-local values. Owner, type, range, duplicate and cross-stage consistency SHALL be validated; explicit and omitted defaults SHALL normalize identically. Full selection SHALL NOT be limited by the retained 128-bit compatibility helper, and business values SHALL NOT choose macro/native IDs or policy.

#### Scenario: Cross-domain override
- **WHEN** Material tries to set a Vertex skinning key or Pipeline shadow key
- **THEN** selection fails without changing the previous prepared material

#### Scenario: Fullscreen without mesh
- **WHEN** fullscreen supplies P/S inputs and no Vertex inputs
- **THEN** it uses the same normalization contract and requires no dummy Mesh or Material
