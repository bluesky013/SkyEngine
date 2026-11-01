## MODIFIED Requirements

### Requirement: Metal push constant 槽位约定

Metal push-constant support SHALL require an explicitly supported target mapping. The target-artifact DirectArguments path SHALL reject push constants whose native slots cannot be represented; it SHALL NOT infer a slot from the highest resource binding or declaration order. Retained low-level staging helpers SHALL NOT be treated as proof that production artifact creation supports push constants. Any supported partial-range path SHALL validate range and stage, accumulate CPU staging and preserve prior bytes.

#### Scenario: Native slot metadata is unsupported
- **WHEN** a target artifact requests push constants without a supported explicit Metal mapping
- **THEN** shader/plan creation fails before execution instead of guessing a buffer index

### Requirement: Metal shader function 按 entry 名加载

Metal shader creation SHALL validate the complete stage artifact and its explicit MSL_SOURCE or METALLIB format, then load artifact.identity.nativeEntry. Missing entries, wrong function stages and unsupported target environments SHALL fail explicitly. Handwritten fixtures SHALL provide the same complete artifact metadata; an empty entry SHALL NOT trigger VSMain/FSMain/CSMain guessing.

#### Scenario: Explicit target entry
- **WHEN** a valid MSL artifact records a native entry and matching stage
- **THEN** Metal loads that exact function and verifies its native stage

#### Scenario: Missing entry
- **WHEN** the recorded entry is absent or has the wrong stage
- **THEN** shader creation fails without selecting a conventional name

### Requirement: Metal ResourceGroup 直接绑定模型

Metal DirectArguments SHALL preserve logical set/binding identities independently of stage/category/native index. Shader-derived BindingPlan SHALL validate the target mapping; resource order SHALL NOT determine native slots. Name-first updates SHALL validate and publish immutable complete versions through the shared transaction. Binding SHALL apply only the recorded active stages and addresses. Dynamic offsets SHALL be checked for count, order, alignment and range; invalid inputs SHALL fail instead of falling back to a base offset. This path SHALL explicitly reject unsupported descriptor arrays and argument-buffer models.

#### Scenario: Different stage addresses
- **WHEN** one logical uniform maps to VS buffer(2) and FS buffer(5)
- **THEN** each stage receives the recorded address and the draw's validated offset

#### Scenario: Fourth group
- **WHEN** Batch set 3 contains a palette or UI image/sampler
- **THEN** its logical identity survives mapping without truncation to three sets

#### Scenario: Invalid dynamic offsets
- **WHEN** supplied offsets do not satisfy the binding plan
- **THEN** binding is rejected without silently reading the base range

#### Scenario: Unsupported descriptor array
- **WHEN** a DirectArguments request requires an unsupported descriptor array
- **THEN** it returns Unsupported rather than using binding plus arrayElement as a logical key
