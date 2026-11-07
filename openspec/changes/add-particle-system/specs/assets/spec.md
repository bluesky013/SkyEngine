## ADDED Requirements

### Requirement: Particle effect asset type

The asset pipeline SHALL register a particle effect asset type with a reflected `.particle` source schema and a cooked product containing the compiled program, renderer descriptors, shader variant keys and source hash.

#### Scenario: Source asset roundtrip

- **WHEN** a `.particle` source is saved and loaded
- **THEN** the system, emitter, module, parameter and renderer data SHALL roundtrip without loss

#### Scenario: Cooked product

- **WHEN** a `.particle` source is cooked
- **THEN** the product SHALL contain a runnable compiled program and SHALL be loadable by the runtime through the same product loader as other assets

#### Scenario: Runtime does not read source

- **WHEN** the runtime loads a particle effect
- **THEN** it SHALL load only the cooked product and SHALL NOT read the `.particle` source for asset data

#### Scenario: Invalid source fails cook

- **WHEN** a `.particle` source fails compiler validation
- **THEN** the cook SHALL fail with diagnostics and SHALL NOT publish a partial product

#### Scenario: Hot reload hash

- **WHEN** a cooked particle product is replaced with a different source hash
- **THEN** new effect instances SHALL use the new program and existing instances SHALL keep their resolved program until restarted


#### Scenario: Malformed source preserves valid output

- **WHEN** source JSON has a wrong member type, invalid enum, non-finite value or unsupported module parameter
- **THEN** parsing SHALL return a diagnostic without asserting or replacing an existing valid effect

#### Scenario: Complete semantic roundtrip

- **WHEN** a supported effect is written and read again
- **THEN** simulation space, bounds, warmup, module names and typed value fields SHALL be preserved
- **AND** compiled program identity SHALL include all fields that affect simulation and rendering

#### Scenario: Versioned compiled products

- **WHEN** a version-two product is read by the version-three loader
- **THEN** the loader SHALL supply WORLD simulation space and canonicalize the complete program hash
- **AND** malformed or unsupported program fields SHALL fail without replacing valid output
