## ADDED Requirements

### Requirement: Rounded shapes evaluate layout-space distance
The Aurora UI renderer's rounded-shape path SHALL evaluate its signed-distance function using interpolated original layout coordinates carried in the shape quad's UV, together with the authored layout-space center, half-size, and radius, rather than comparing framebuffer position with layout-space shape data. Transformed shape vertices SHALL retain consistent shape-local distance calculations. Ordinary textured quads SHALL retain their existing texture UV semantics. The actual shader source used by runtime compilation SHALL also be consumable by offline compilation validation without a separately copied test shader.

#### Scenario: Transformed rounded shape uses coherent coordinates
- **WHEN** a rounded shape is translated, rotated, or scaled through UI geometry
- **THEN** its shader SHALL evaluate the shape distance using the corresponding interpolated layout position and authored layout-space radius/extent

#### Scenario: Texture UVs remain texture coordinates
- **WHEN** an ordinary textured quad is emitted
- **THEN** its UV values SHALL continue to sample the requested texture region rather than be interpreted as a shape-layout position

#### Scenario: Offline validation compiles runtime source
- **WHEN** the available shader compiler validates the UI shader offline
- **THEN** it SHALL compile the actual source artifact consumed by the runtime renderer, including the rounded-shape entry point
