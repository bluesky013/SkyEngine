## MODIFIED Requirements

### Requirement: 编译器抽象与可选模块

IShaderCompiler and its shared factory SHALL keep the compiler optional for PRODUCTS consumption. DEVELOPMENT SHALL require the actual producer identity and valid sources, compiling only a cache miss. Runtime-only compiler registration SHALL be revoked on shutdown; accepted asynchronous work SHALL retain its compiler and cache endpoint owners until completion; defining DLL code SHALL remain process resident. Missing compiler ownership for an async miss SHALL fail explicitly.

#### Scenario: Products without compiler
- **WHEN** no compiler module is loaded and PRODUCTS has a compatible published selection
- **THEN** the artifact loads without reconstructing a compiler identity

#### Scenario: Async task survives unregister
- **WHEN** a compiler is unregistered after a task was accepted
- **THEN** that task retains its owned compiler and cache endpoints until completion without a per-object code token
