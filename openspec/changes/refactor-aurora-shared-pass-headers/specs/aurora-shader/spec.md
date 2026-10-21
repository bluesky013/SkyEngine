## REMOVED Requirements

### Requirement: UBO 类型按 tier 约定（global/pass static，batch dynamic）
**Reason**: Static/dynamic policy is explicit and cannot be inferred from the resource tier or a removed code generator.
**Migration**: Use the existing explicit binding-type selection and validate it against the compiled layout.

### Requirement: 反射驱动 codegen
**Reason**: C++ Pass declarations must not require running a shader compiler to generate their types.
**Migration**: Replace Slang declaration files and generated mirrors with Pass-owned shared `.h` sources. Retain independent shader compilation and target reflection validation.

## ADDED Requirements

### Requirement: UBO 类型按显式绑定声明
Shared declarations SHALL describe the base uniform type. The consuming Pass/material SHALL explicitly select static or dynamic binding and keep shader/layout type, allocation and binding offsets consistent; set numbers SHALL NOT determine this policy.

#### Scenario: Explicit dynamic binding
- **WHEN** a consumer selects a dynamic uniform binding
- **THEN** its shader layout, allocation and supplied offsets agree; a policy mismatch returns an explicit error before drawing

### Requirement: Pass-owned shared header source
Each Global/Pass/Batch declaration SHALL have one editable shared `.h` source consumed directly by C++ and Slang. Field/resource lists SHALL define native C++ types, typed binding slots, expected schema and shader declarations without duplicating fields or binding numbers. Global/Pass/Batch SHALL remain sets 0/1/2; View SHALL remain optional and owned by the consuming Pass.

#### Scenario: Build C++ without shader code generation
- **WHEN** Pipeline C++ consumers compile
- **THEN** they include source headers directly and have no ShaderHeaders, ShaderHeaderTool or Slang execution prerequisite; CMake may embed source text for runtime shader includes

#### Scenario: Shared source changes
- **WHEN** a shared declaration or common shader helper changes
- **THEN** C++ consumers rebuild and CMake refreshes the embedded source; no mirrored ABI header in the source or build tree is consumed

#### Scenario: Uniform layout across targets
- **WHEN** a shared flat uniform block is compiled for SPIR-V, DXIL or MSL
- **THEN** it uses explicit logical set/binding ConstantBuffer declarations and column-major matrices; expected C++ sizeof/offsetof and reflected target layout must agree

## MODIFIED Requirements

### Requirement: 虚拟 include
`ShaderCompilerSlang` SHALL resolve quoted and angle includes through `ShaderFileSystem`, including packaged shared headers and their common helper, without requiring disk copies or native host header generators.

#### Scenario: Packaged scene shaders
- **WHEN** only the built-in scene virtual files are registered, without filesystem search paths
- **THEN** SceneForward, SceneLit, SceneShadow and SceneToneMap compile for each supported target with their shared declaration includes

#### Scenario: Shared helper cache invalidation
- **WHEN** a shared helper reached through an angle include changes
- **THEN** the source closure hash changes and old cache entries are rejected; quoted includes search the including directory before the logical root

### Requirement: 类型映射
Shared declaration helpers SHALL map float/int/uint, float2/float4 and column-major mat4 fields to native C++ types and Slang fields, and preserve bounded float4 array counts/strides. BOOL/FLOAT3 uniform fields (use UINT/FLOAT4), nested structures, storage-buffer declarations, descriptor arrays and unsupported uniform arrays SHALL fail explicitly instead of omitting fields. Resource-only declarations SHALL preserve resource shape and typed slots; describing a resource SHALL NOT imply backend binding support.

#### Scenario: Bounded uniform arrays
- **WHEN** a Pass declares a float4 array
- **THEN** native sizeof/offsetof, array count and stride agree with each target's reflection

#### Scenario: Resource-only declaration
- **WHEN** a shared declaration contains textures or samplers without constants
- **THEN** it exposes resource schema and typed slots without inventing a placeholder uniform block

### Requirement: 三方一致性校验
Shared C++ types and their expected schema SHALL use native sizeof/offsetof. Target shader reflection SHALL validate resource shapes and actual uniform members, offsets, sizes and strides before binding; the legacy RgBlockDesc bridge SHALL remain derived from the same field list. Tests SHALL verify shared declarations against SPIR-V, DXIL and MSL independently. MSL source/reflection tests SHALL NOT replace native Metal execution validation.

#### Scenario: Layout drift is rejected
- **WHEN** compiled shader layout disagrees with the shared C++ schema
- **THEN** binding returns an explicit error before modifying the draw or executing it on the GPU

#### Scenario: Cross compilation
- **WHEN** a target platform C++ build consumes a Pass header
- **THEN** no host shader generator is required to obtain its types; target layout and execution still require corresponding shader/platform validation
