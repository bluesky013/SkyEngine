# asset-async-executor Specification

## Purpose
TBD - created by archiving change asset-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Asset async uses the engine thread pool

Asset asynchronous execution SHALL use `sky::ThreadPool` (`engine/core/include/core/async/ThreadPool.h`). `Framework` public headers MUST NOT expose taskflow types or include `<taskflow/taskflow.hpp>`.

#### Scenario: No taskflow in public headers

- **WHEN** a translation unit includes `framework/asset/Asset.h` or `framework/asset/AssetExecutor.h`
- **THEN** it SHALL compile without taskflow headers, and no `tf::` type SHALL appear in those public interfaces

#### Scenario: Load work runs on the thread pool

- **WHEN** an asset is loaded asynchronously
- **THEN** its deserialization SHALL execute on the engine `ThreadPool` rather than a taskflow executor

### Requirement: Dependency-ordered async loading is preserved

Asset loading SHALL still schedule dependencies before the dependent asset, and waiting on an asset SHALL block until its dependency chain completes.

#### Scenario: Dependencies complete first

- **WHEN** an asset whose header lists dependencies is loaded asynchronously
- **THEN** each dependency SHALL reach a loaded state before the dependent asset's deserialization runs

#### Scenario: Blocking wait observes completion

- **WHEN** a caller blocks on an asset that is loading
- **THEN** the wait SHALL return once the asset reaches LOADED or FAILED (after its dependencies have finished)

### Requirement: Pending asset tasks can be drained

The asset executor SHALL expose a wait-for-all that blocks until every queued asset task has finished, used before flushing persistent state.

#### Scenario: Drain before saving

- **WHEN** the asset database is saved
- **THEN** the executor SHALL drain all pending asset tasks before the state is written

