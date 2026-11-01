## ADDED Requirements

### Requirement: Compilation retains its source snapshot

Accepted compilation SHALL retain a stable owned source/dependency view, inventory and required provider/compiler/data ownership until completion or collected cancellation; defining DLL code SHALL remain process resident. Resource registration/retirement SHALL NOT mutate or destroy backing still used by a compile job. Failures SHALL release borrows through the same lifetime protocol.

#### Scenario: Source owner retires during compilation
- **WHEN** an owner stops accepting requests while a shader job is in flight
- **THEN** the job finishes against its retained snapshot or reports collected cancellation, and ownership is released only after that use ends

## MODIFIED Requirements

### Requirement: 编译器抽象与可选模块

IShaderCompiler and its shared factory SHALL keep the compiler optional for PRODUCTS consumption. DEVELOPMENT SHALL require the actual producer identity and valid sources, compiling only a cache miss. Runtime-only compiler registration SHALL be revoked on shutdown; accepted asynchronous work SHALL retain its compiler and cache endpoint owners until completion; defining DLL code SHALL remain process resident. Missing compiler ownership for an async miss SHALL fail explicitly.

#### Scenario: Products without compiler
- **WHEN** no compiler module is loaded and PRODUCTS has a compatible published selection
- **THEN** the artifact loads without reconstructing a compiler identity

#### Scenario: Async task survives unregister
- **WHEN** a compiler is unregistered after a task was accepted
- **THEN** that task retains its owned compiler and cache endpoints until completion without a per-object code token
