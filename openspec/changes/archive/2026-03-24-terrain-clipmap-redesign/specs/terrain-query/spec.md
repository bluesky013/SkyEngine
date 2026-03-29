## ADDED Requirements

### Requirement: CPU height query
`TerrainFeatureProcessor` SHALL expose `float QueryHeight(float worldX, float worldZ)` that returns the interpolated terrain height at the given world XZ coordinate by bilinearly sampling the appropriate heightmap tile from memory.

#### Scenario: Query within a single tile
- **WHEN** `QueryHeight(50.0, 50.0)` is called and the point falls within tile (0,0)
- **THEN** the system bilinearly interpolates the 4 nearest heightmap texels in tile (0,0) and returns worldHeight = interpolatedTexel × heightScale + heightOffset

#### Scenario: Query at tile boundary
- **WHEN** `QueryHeight(x, z)` falls exactly on the boundary between two tiles
- **THEN** the 1-texel overlap between adjacent tiles ensures correct bilinear interpolation without boundary artifacts

#### Scenario: Query outside terrain bounds
- **WHEN** `QueryHeight(x, z)` is called with coordinates outside the terrain extent
- **THEN** the system returns 0.0 (or a configurable default height)

### Requirement: CPU normal query
`TerrainFeatureProcessor` SHALL expose `Vector3 QueryNormal(float worldX, float worldZ)` that computes the terrain surface normal at the given position using finite-difference sampling of adjacent height values.

#### Scenario: Flat terrain normal
- **WHEN** the terrain is perfectly flat at the queried position
- **THEN** `QueryNormal` returns Vector3(0, 1, 0) (up direction)

#### Scenario: Sloped terrain normal
- **WHEN** the terrain has a gradient in the X direction at the queried position
- **THEN** `QueryNormal` returns a normalized vector tilted away from Y toward the downhill direction

### Requirement: CPU splatmap weight query
`TerrainFeatureProcessor` SHALL expose `Vector4 QuerySplatWeights(float worldX, float worldZ)` that returns the material layer weights (first 4 layers) at the given position by bilinearly sampling the splatmap tile.

#### Scenario: Query splatmap at grass-dominant area
- **WHEN** the splatmap at (x, z) has RGBA = (200, 30, 25, 0)
- **THEN** `QuerySplatWeights` returns approximately Vector4(0.784, 0.118, 0.098, 0.0) (normalized to 0-1)

### Requirement: CPU raycast against terrain
`TerrainFeatureProcessor` SHALL expose `bool Raycast(const Ray& ray, float maxDist, TerrainHitResult& out)` that performs a ray-terrain intersection test using ray marching along the heightfield.

#### Scenario: Ray hits terrain
- **WHEN** a downward ray from (100, 500, 100) intersects the terrain surface
- **THEN** `Raycast` returns true and `out.position` contains the intersection point, `out.normal` contains the surface normal

#### Scenario: Ray misses terrain
- **WHEN** a ray is directed upward away from the terrain
- **THEN** `Raycast` returns false

### Requirement: GPU heightmap atlas handle
`TerrainFeatureProcessor` SHALL expose `RDTexture2DPtr GetHeightmapAtlas()` returning the GPU texture handle of the heightmap atlas. This SHALL return nullptr until a heightmap atlas is built (Phase 2 streaming).

#### Scenario: Phase 1 without atlas
- **WHEN** `GetHeightmapAtlas()` is called before streaming is implemented
- **THEN** it returns nullptr

#### Scenario: Phase 2 with atlas
- **WHEN** the heightmap atlas has been built from loaded tiles
- **THEN** `GetHeightmapAtlas()` returns a valid GPU texture handle that compute shaders can sample

### Requirement: GPU splatmap atlas handle
`TerrainFeatureProcessor` SHALL expose `RDTexture2DPtr GetSplatmapAtlas()` returning the GPU texture handle of the splatmap atlas. Same phased availability as heightmap atlas.

#### Scenario: Atlas not yet available
- **WHEN** `GetSplatmapAtlas()` is called before atlas construction
- **THEN** it returns nullptr

### Requirement: Clipmap info struct for GPU consumers
`TerrainFeatureProcessor` SHALL expose `const ClipmapInfo& GetClipmapInfo()` returning a structure with: `float2 centerOffset[MAX_LEVELS]`, `float levelScale[MAX_LEVELS]`, `float heightScale`, `float heightOffset`, `uint32_t numLevels`, `uint32_t blockSize`. This data is sufficient for external GPU systems (e.g., vegetation compute shaders) to sample the terrain.

#### Scenario: Vegetation system reads clipmap info
- **WHEN** a vegetation feature processor calls `terrainFP->GetClipmapInfo()`
- **THEN** it receives current per-level offsets and scale parameters needed to correctly sample the heightmap atlas

### Requirement: One-way dependency direction
External systems (vegetation, physics, AI) SHALL depend on terrain query API. Terrain SHALL NOT have compile-time or runtime dependencies on any consumer system.

#### Scenario: Vegetation plugin depends on terrain
- **WHEN** the vegetation plugin's `plugin.json` lists "Terrain" in dependencies
- **THEN** terrain's `plugin.json` does NOT list the vegetation plugin

#### Scenario: Terrain works without consumers
- **WHEN** no vegetation or physics plugins are loaded
- **THEN** the terrain system initializes and renders normally
