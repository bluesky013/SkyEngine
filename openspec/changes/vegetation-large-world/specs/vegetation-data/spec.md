## ADDED Requirements

### Requirement: Vegetation asset with biomes, palettes, and density

The vegetation asset SHALL carry a biome set (biome definitions + placement rules), species/palette definitions (mesh/billboard reference, scale/rotation jitter, wind response, density), and per-biome density (distribution) maps, with addressable access by biome.

#### Scenario: Address a biome

- **WHEN** a biome is requested by id from a vegetation asset that contains it
- **THEN** the asset SHALL return that biome's rules, palette, and density data

#### Scenario: Missing biome

- **WHEN** a biome is requested by an id absent from the asset
- **THEN** the asset SHALL report that the biome is unavailable

### Requirement: Density map payload

Density maps SHALL be stored as addressable payloads sampled in world space, so placement can read density at a world position independently of the cell partition.

#### Scenario: World-space sampling

- **WHEN** a density value is read for a world position
- **THEN** it SHALL be derived from the density payload by world-space addressing

### Requirement: Vegetation asset serialization round-trip

The vegetation asset SHALL support binary serialization such that saving and reloading preserves biomes, palettes, density payloads, and rules without loss.

#### Scenario: Round-trip vegetation asset

- **WHEN** a vegetation asset is serialized and then deserialized
- **THEN** the reloaded biomes, palettes, density payloads, and rules SHALL match the original

### Requirement: Logic-only vegetation component

The vegetation component SHALL hold only plain data (vegetation asset/source identifier, biome set id, seed, density/LOD parameters, streaming parameters) and SHALL NOT declare or reference any render resource type.

#### Scenario: Reflect vegetation component

- **WHEN** the vegetation component is serialized and restored
- **THEN** all of its configuration SHALL round-trip as plain data and asset identifiers

### Requirement: Instances

The vegetation asset SHALL carry an instance list (position, rotation, scale, biome, species); cells whose area contains instances SHALL use them in place of procedural placement, while cells without instances continue procedural placement.

#### Scenario: Instances override procedural placement

- **WHEN** a cell contains instances
- **THEN** that cell SHALL emit the instances instead of procedurally generated ones

#### Scenario: Procedural placement for cells without instances

- **WHEN** a cell contains no instances
- **THEN** that cell SHALL emit procedurally generated instances

#### Scenario: Instances round-trip

- **WHEN** a vegetation asset with instances is serialized and reloaded
- **THEN** the instances SHALL be preserved
