## MODIFIED Requirements

### Requirement: Shader 使用收集（离线预编）

Usage collection SHALL be explicitly enabled by the host and record source/entry/stage/target environment plus complete typed selection. Persistent identity SHALL NOT rely only on a 128-bit compatibility key or hash. Flush SHALL union compatible records across sessions with checked format/version handling. Offline cook SHALL consume explicit requested selections, project each stage's dependencies and publish the allowed complete selections; it SHALL NOT infer an unrestricted Cartesian product or replace nondefault values with defaults.

#### Scenario: Complete selection survives collection
- **WHEN** V/P/S inputs include nondefault bool, signed and unsigned values
- **THEN** usage/cook retain all typed values and their stage-specific meaning

#### Scenario: Collection disabled
- **WHEN** the host did not enable usage collection
- **THEN** resolution does not implicitly publish a usage file

#### Scenario: Session union
- **WHEN** two enabled sessions collect distinct valid selections
- **THEN** the published usage union retains both without duplicate identities

### Requirement: 编译器抽象与可选模块

IShaderCompiler and its shared factory SHALL keep the compiler optional for PRODUCTS consumption. DEVELOPMENT SHALL require the actual producer identity and valid sources, compiling only a cache miss. Runtime-only compiler registration SHALL be revoked on shutdown; accepted asynchronous work SHALL retain its compiler/module owner until completion. Missing compiler ownership for an async miss SHALL fail explicitly.

#### Scenario: Products without compiler
- **WHEN** no compiler module is loaded and PRODUCTS has a compatible published selection
- **THEN** the artifact loads without reconstructing a compiler identity

#### Scenario: Async task survives unregister
- **WHEN** a compiler is unregistered after a task was accepted
- **THEN** that task retains its owned compiler and code lease until completion

### Requirement: 单写者 IO 与原子落盘

Cache publication SHALL serialize index changes and atomically replace temporary files only after successful encoding and write. ShaderCacheWriter SHALL provide queued single-writer operation; synchronous commits SHALL use the existing guarded store. An unsupported or failed rename SHALL return failure rather than fall back to exposing a partial direct write. Async jobs SHALL retain owned compiler and cache endpoints until completion; errors SHALL remain observable through commit/flush results.

#### Scenario: Concurrent submissions
- **WHEN** several jobs publish different artifacts through the writer
- **THEN** successful flush retains all committed records

#### Scenario: Rename fails
- **WHEN** final atomic replacement fails
- **THEN** publication reports failure and the prior published index remains readable

### Requirement: 编译键与 entry 级产物

Compile identity SHALL include normalized source/dependency closure, entry/stage, code-affecting selection, schema/interface, target environment and actual producer identity. Complete logical selection identity SHALL remain distinct from stage code and native executable identity. Compatible native-specialization selections MAY share code, but SHALL retain independent stage-specific values and executable selection. Artifact identity SHALL bind exact code length and target reflection; reflection output SHALL NOT become a circular compile input.

#### Scenario: Native selections share code
- **WHEN** two complete selections differ only in a supported native specialization value
- **THEN** code may be reused while executable identity and stage values remain distinct

#### Scenario: Dependency changes
- **WHEN** an included export, companion, target option or compiler adapter changes
- **THEN** affected compile identities invalidate old development results

### Requirement: 内容寻址缓存（单 index + blobs）

Each target directory SHALL contain a versioned index and content-addressed stage artifacts. The current cache format SHALL preserve complete logical schema, allowed selections, code identities, target environment and producer metadata; code plus reflection SHALL be checked as one ArtifactID. Offline and local roots SHALL retain explicit read/write roles. Packaging SHALL combine selected owners' records with conflict validation instead of first-match mounting that hides another index. Incompatible formats SHALL require recook.

#### Scenario: Multiple shader owners
- **WHEN** two selected owners contribute distinct requests for the same target
- **THEN** the distributed index retains both; conflicting request identities fail

#### Scenario: Artifact corruption
- **WHEN** code or reflection differs from the recorded ArtifactID
- **THEN** loading rejects the artifact before native creation

### Requirement: 解析优先级 离线 > 源码 > 本地实时

Resolution SHALL explicitly distinguish DEVELOPMENT and PRODUCTS. DEVELOPMENT SHALL require the current source closure and actual producer identity, validate matching offline/local cache artifacts and compile only on a valid miss. PRODUCTS SHALL resolve complete published selections from its selected product root without source or compiler access. Missing, corrupt, incompatible or ambiguous products SHALL fail explicitly; neither mode SHALL accept a blob merely because its file exists.

#### Scenario: Development cache hit
- **WHEN** source, producer, target environment and normalized selection match a validated artifact
- **THEN** development loads that artifact without recompilation

#### Scenario: Products without compiler
- **WHEN** a relocated product distribution contains the requested complete selection and compatible runtime ABI
- **THEN** resolution succeeds without sources or the producer toolchain

#### Scenario: Unpublished selection
- **WHEN** PRODUCTS receives a selection absent from its manifest/index
- **THEN** it reports missing product without fallback compilation or substituting defaults

### Requirement: 三段解析（schema / 变体 / 二进制）

Preparation SHALL obtain complete logical schema, normalize typed V/P/S values, project stage dependencies and resolve matching stage artifacts. Schema fingerprint, ownership, ranges and defaults SHALL be validated before lookup. A supplied hash SHALL NOT replace full canonical selection validation. Immutable preparation SHALL retain program, values, source/product metadata and owners across asynchronous execution.

#### Scenario: Supplied fingerprint is stale
- **WHEN** request schema or selected values disagree with their metadata
- **THEN** preparation fails instead of resolving by a caller-supplied hash alone

### Requirement: Shader 身份为相对路径（非资产）

ShaderRef SHALL identify an explicit source path and entry without becoming a UUID asset. Paths SHALL be canonical domain-relative paths in validated owner inventories, with normalized separators and rejected traversal. Spelling SHALL be retained; case-only path collisions SHALL fail rather than being silently lowercased. Source and relocated product resolution SHALL preserve the same identity.

#### Scenario: Portable identity
- **WHEN** a module's physical root changes but its logical shader path and entry stay the same
- **THEN** requests retain the same logical identity

#### Scenario: Case-only collision
- **WHEN** two owners expose paths differing only by case
- **THEN** collection rejects the conflict before publishing the inventory
