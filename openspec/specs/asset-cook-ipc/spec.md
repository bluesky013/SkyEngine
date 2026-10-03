# asset-cook-ipc Specification

## Purpose

The out-of-process asset cook path: the length-prefixed frame protocol, the
in-process/out-of-process `ICookRunner` seam, the `AssetTool` worker host, response
correlation, post-cook index visibility, and worker lifecycle.

## Requirements

### Requirement: Length-prefixed frame protocol

The out-of-process cook worker SHALL exchange messages as length-prefixed frames over the child's standard output, where each frame is a little-endian unsigned 32-bit payload length followed by a UTF-8 JSON payload. The reader SHALL reassemble frames across partial reads and SHALL reject a declared length above a configured maximum as a desynchronized channel.

#### Scenario: Reassemble a split frame

- **WHEN** a frame's length header and payload arrive across multiple reads
- **THEN** the reader SHALL deliver exactly one complete message with the original payload bytes

#### Scenario: Oversized frame rejected

- **WHEN** a frame declares a payload length above the maximum
- **THEN** the channel SHALL fail with a desynchronization error instead of allocating unbounded memory

### Requirement: Log separation

Worker logs SHALL be written to standard error, and standard output SHALL carry only protocol frames so logs can never corrupt the protocol stream.

#### Scenario: Logs do not corrupt the protocol

- **WHEN** the worker writes log lines while serving requests
- **THEN** those lines SHALL be read from standard error and forwarded to the engine logger, and standard output SHALL contain only valid frames

#### Scenario: Malformed stdout fails the session

- **WHEN** a non-frame byte sequence appears on standard output
- **THEN** the reader SHALL treat the channel as failed and fail the in-flight requests rather than misinterpreting the stream

#### Scenario: Default logger cannot corrupt frames

- **WHEN** the worker runs engine logging during a cook, even though the engine logger's default stream is standard output
- **THEN** the worker SHALL direct the logger to standard error before engine initialization, and SHALL also preserve a private descriptor for frames and redirect process standard output to standard error, so standard output carries only frames even if a non-logger component writes to standard output directly

### Requirement: Protocol version handshake

On startup the worker SHALL perform a version handshake before accepting cook requests, and a protocol version mismatch SHALL fail the session.

#### Scenario: Handshake succeeds

- **WHEN** the worker starts and the parent sends a hello with a supported protocol version
- **THEN** the worker SHALL reply that it is ready together with its platform target

#### Scenario: Version mismatch rejected

- **WHEN** the parent's protocol version is not supported by the worker
- **THEN** the session SHALL fail and no cook request SHALL be sent

### Requirement: Mode-agnostic cook completion

The loading layer SHALL depend only on an `ICookRunner` abstraction and the build-finished event, so that an in-process cook and an out-of-process cook raise the same completion event and are indistinguishable to the loader. The cook mode SHALL be selectable by configuration, defaulting to in-process.

#### Scenario: Same event for both modes

- **WHEN** a cook requested through the out-of-process runner completes
- **THEN** it SHALL raise the same build-finished event, carrying uuid, target, return code, and error, as an in-process cook

#### Scenario: In-process default

- **WHEN** no cook mode is configured
- **THEN** cooks SHALL run in-process on the cook pool

#### Scenario: Out-of-process selected

- **WHEN** the configuration selects the out-of-process mode
- **THEN** cooks SHALL be dispatched to the worker process instead of running in the editor process

### Requirement: Request and response correlation

Each cook request SHALL carry a unique identifier and the asset uuid and target; the worker SHALL echo the identifier, uuid, target, return code, and any error in its response. A result SHALL be matched to its in-flight request by identifier, and the uuid and target SHALL be preserved for the completion event.

#### Scenario: Result matched to request

- **WHEN** the worker returns a result carrying the identifier of a queued request
- **THEN** the runner SHALL complete exactly that request with the returned return code and error

#### Scenario: Unknown response dropped

- **WHEN** a result arrives whose identifier does not match any in-flight request
- **THEN** the runner SHALL discard it and record a warning

### Requirement: Worker shares the editor context

The worker SHALL start with the same mount namespace and platform target as the editor and SHALL report its platform target in the handshake, so that path resolution and product placement match the editor.

#### Scenario: Same mounts and target

- **WHEN** the worker resolves a source path and writes a product
- **THEN** it SHALL use the same mounts and platform target as the editor

#### Scenario: Identity is stable

- **WHEN** the worker cooks an asset
- **THEN** the produced product SHALL keep the source uuid and the target bundle's product index SHALL map the logical path to that uuid

### Requirement: Worker capability parity

The worker SHALL provide the same asset-building capabilities as the editor by loading the same builder modules before serving requests; linking the framework alone SHALL NOT be assumed to provide builders. On a platform where the required builders are not built, a cook request SHALL fail observably rather than report success.

#### Scenario: Worker has builders

- **WHEN** the worker starts
- **THEN** it SHALL register the same builders as the editor (loaded from the builder modules) before accepting cook requests

#### Scenario: Missing builders fail the cook

- **WHEN** a cook is requested on a worker that has no builder for the asset
- **THEN** the request SHALL complete with a failure result and the pending load SHALL become failed

### Requirement: Post-cook index visibility

When an out-of-process cook completes successfully, the editor SHALL resolve the cooked asset by its logical path, which requires refreshing the editor's in-memory product mapping; the editor SHALL NOT rely on a mapping read only at bundle registration.

#### Scenario: Path resolves after out-of-process cook

- **WHEN** an out-of-process cook writes a product index entry for a path
- **THEN** a subsequent load of that path in the editor SHALL resolve to the cooked uuid

#### Scenario: Single writer

- **WHEN** the editor uses out-of-process cook
- **THEN** only the worker SHALL write the product index and manifests for that bundle, and the editor SHALL NOT run concurrent in-process cooks into the same bundle

### Requirement: Worker lifecycle and failure handling

The runner SHALL maintain a persistent worker per session, and on a per-request timeout, unexpected worker exit, or protocol failure it SHALL fail every in-flight request and permit a fresh worker to be started on the next request. A failed cook SHALL resolve the pending load to a failed state and SHALL NOT leave it stranded in the loading state.

#### Scenario: Timeout fails the request

- **WHEN** a cook request does not complete within its timeout
- **THEN** the request SHALL fail, the worker SHALL be terminated, and the pending load SHALL become failed

#### Scenario: Crash fails in-flight requests and restarts

- **WHEN** the worker exits unexpectedly while requests are in flight
- **THEN** all in-flight requests SHALL fail and a subsequent request SHALL start a fresh worker

#### Scenario: Clean shutdown on drain

- **WHEN** the runner is drained
- **THEN** the worker SHALL be asked to shut down and SHALL be terminated if it does not exit, leaving no orphan process

#### Scenario: Blocked reader threads unblock and join

- **WHEN** the worker is killed or drained while the parent's stdout/stderr reader threads are blocked on a read
- **THEN** those reads SHALL be unblocked, the threads SHALL join before the channel is destroyed, and shutdown SHALL complete without hanging or a use-after-free
