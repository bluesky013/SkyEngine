## ADDED Requirements

### Requirement: ClipmapConfig structure
The system SHALL define a `ClipmapConfig` POD structure containing: `blockSize` (uint32, default 64), `numLevels` (uint32, default 8), `resolution` (float, default 1.0), `heightScale` (float, default 256.0), `heightOffset` (float, default 0.0), `heightFormat` (enum: R16_UNORM or R32_SFLOAT, default R16_UNORM).

#### Scenario: Default configuration
- **WHEN** a `ClipmapConfig` is default-constructed
- **THEN** blockSize=64, numLevels=8, resolution=1.0, heightScale=256.0, heightOffset=0.0, heightFormat=R16_UNORM

#### Scenario: Serialization round-trip
- **WHEN** a `ClipmapConfig` is serialized to JSON and deserialized
- **THEN** all fields retain their values exactly

### Requirement: TerrainData with tiled storage
`TerrainData` SHALL contain: `ClipmapConfig config`, `Uuid material`, `uint32_t tileCountX`, `uint32_t tileCountY`, `std::vector<Uuid> heightmapTiles` (size = tileCountX × tileCountY), `std::vector<Uuid> splatmapTiles`, and `std::vector<LayerInfo> layers`.

#### Scenario: 8×8 terrain grid
- **WHEN** tileCountX=8 and tileCountY=8
- **THEN** heightmapTiles contains exactly 64 Uuid entries, one per tile

### Requirement: LayerInfo definition
Each material layer SHALL be described by a `LayerInfo` structure containing: `Name name`, `Uuid albedo`, `Uuid normal`, `Uuid roughness`.

#### Scenario: Layer with full PBR textures
- **WHEN** a layer is defined with name="Grass", albedo=Uuid_A, normal=Uuid_N, roughness=Uuid_R
- **THEN** the shader system can bind all three textures for that layer's contribution

### Requirement: Heightmap tile format
Each heightmap tile SHALL be stored as a 2D texture asset with pixel format matching `ClipmapConfig.heightFormat`. Tile dimensions SHALL be `blockSize × blockSize` texels. Adjacent tiles SHALL overlap by 1 texel row/column at shared edges to enable seamless bilinear sampling.

#### Scenario: R16_UNORM heightmap tile
- **WHEN** heightFormat is R16_UNORM and blockSize is 128
- **THEN** each tile is a 128×128 R16_UNORM texture, with world height = texel × heightScale + heightOffset

#### Scenario: Tile edge overlap
- **WHEN** tile (0,0) and tile (1,0) are adjacent
- **THEN** the rightmost column of tile (0,0) contains the same height values as the leftmost column of tile (1,0)

### Requirement: Splatmap tile format
Each splatmap tile SHALL be stored as an RGBA8 texture, packing 4 material layer weights per texel. Multiple splatmap textures per tile are used when more than 4 layers exist (layers 0-3 in texture 0, layers 4-7 in texture 1, etc.). Splatmap tiles use the same tile grid as heightmap tiles.

#### Scenario: 4-layer terrain
- **WHEN** the terrain has 4 material layers
- **THEN** each tile has exactly 1 splatmap texture (RGBA8), with R=layer0, G=layer1, B=layer2, A=layer3

#### Scenario: 6-layer terrain
- **WHEN** the terrain has 6 material layers
- **THEN** each tile has 2 splatmap textures: texture 0 packs layers 0-3, texture 1 packs layers 4-5 in RG channels

### Requirement: UE5-compatible height encoding
The height scale/offset system SHALL support lossless import of UE5 Landscape heightmaps. For UE5 data exported as .r16 (uint16, zero-point at 32768, 1/128 cm per unit, Scale.Z multiplier): `heightScale = ScaleZ / 128.0 × 0.01`, `heightOffset = -32768 / 128.0 × ScaleZ × 0.01`.

#### Scenario: UE5 default scale import
- **WHEN** importing a UE5 heightmap with Scale.Z=100 (default)
- **THEN** heightScale = 100/128×0.01 ≈ 0.0078125, heightOffset = -32768/128×100×0.01 ≈ -256.0, producing world heights in meters from -256m to +256m

### Requirement: UE5-compatible tile size alignment
`ClipmapConfig.blockSize` values of 64, 128, and 256 SHALL correspond to UE5 `ComponentSizeQuads` of 63, 127, and 255 respectively (vertices = quads + 1).

#### Scenario: UE5 127-quad component import
- **WHEN** importing a UE5 Landscape with ComponentSizeQuads=127
- **THEN** the importer sets blockSize=128, and each UE5 component maps to exactly one terrain tile

### Requirement: TerrainComponent as pure logic layer
`TerrainComponent` SHALL derive from `ComponentAdaptor<TerrainData>` and hold only `TerrainData` (POD with `Uuid` references). It SHALL communicate with `TerrainFeatureProcessor` via `SetConfig(ClipmapConfig)`, `SetMaterial(Uuid)`, and `SetTileData(tileCountX, tileCountY, heightmapTiles, splatmapTiles, layers)`. It SHALL NOT perform asset loading, texture creation, or material instantiation.

#### Scenario: Component attach lifecycle
- **WHEN** `TerrainComponent::OnAttachToWorld()` is called
- **THEN** it obtains `TerrainFeatureProcessor` via `GetFeatureProcessor<TerrainFeatureProcessor>(actor)` and calls `SetConfig`, `SetMaterial`, and `SetTileData` with data from `TerrainData`

#### Scenario: Component detach lifecycle
- **WHEN** `TerrainComponent::OnDetachFromWorld()` is called
- **THEN** it calls `TerrainFeatureProcessor::Reset()` and sets its FP pointer to null

### Requirement: Serialization reflection registration
All new POD types (`ClipmapConfig`, `LayerInfo`, updated `TerrainData`) SHALL be registered in `TerrainComponent::Reflect()` with the engine's `SerializationContext`, including `ASSET_TYPE` property hints for `Uuid` fields referencing textures or materials.

#### Scenario: Editor property panel
- **WHEN** a `TerrainComponent` is selected in the editor
- **THEN** all `ClipmapConfig` fields, material reference, tile counts, and layer list are editable in the property panel
