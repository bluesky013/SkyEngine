# native-process Specification

## Purpose

Child-process spawn with piped stdio and lifecycle control (wait/timeout/kill) on desktop
platforms, used by the out-of-process asset cook worker. Unsupported on mobile.

## Requirements

### Requirement: Child process launch

The platform layer SHALL provide a way to start a child process with an explicit argument vector, environment, and working directory, without going through a shell.

#### Scenario: Start a child with arguments

- **WHEN** the caller starts a process with an argument vector, environment, and working directory
- **THEN** the child SHALL run with exactly those arguments, environment, and working directory, and the caller SHALL receive a handle to its standard input, output, and error streams

#### Scenario: Missing executable fails fast

- **WHEN** the executable path does not exist or cannot be executed
- **THEN** the launch SHALL fail and return an error rather than start a partially initialized process

#### Scenario: Working directory set without unsafe fork work

- **WHEN** a working directory must be applied on a platform whose spawn API supports it
- **THEN** the launch SHALL set it through the spawn API; if a `fork`+`exec` fallback is used, only async-signal-safe operations SHALL run between `fork` and `exec` (pre-built argument/environment buffers, no allocation, no locks, no logging)

### Requirement: Standard stream pipes

The launched child's standard input, standard output, and standard error SHALL be connected to pipes owned by the parent, and the parent SHALL be able to write to stdin and read stdout and stderr without shell interpretation.

#### Scenario: Write stdin and read stdout

- **WHEN** the parent writes bytes to the child's standard input
- **THEN** the child SHALL receive them unmodified, and bytes the child writes to standard output SHALL be readable by the parent

#### Scenario: Separate error stream

- **WHEN** the child writes to standard error
- **THEN** those bytes SHALL be readable from the error stream independently of the standard output stream

#### Scenario: Partial reads and writes are handled

- **WHEN** a read or write transfers fewer bytes than requested
- **THEN** the operation SHALL resume from the correct offset without losing or duplicating bytes

### Requirement: Wait, timeout, and kill

The platform layer SHALL allow the parent to wait for the child to exit, optionally with a timeout, and to forcibly terminate a running child.

#### Scenario: Wait returns exit status

- **WHEN** the parent waits for a child that exits
- **THEN** the wait SHALL return the child's exit status

#### Scenario: Wait times out

- **WHEN** the parent waits with a timeout and the child has not exited within it
- **THEN** the wait SHALL report a timeout without blocking indefinitely

#### Scenario: Kill terminates a running child

- **WHEN** the parent kills a running child
- **THEN** the child SHALL be terminated and a subsequent wait SHALL complete

### Requirement: Platform support scope

Child-process execution SHALL be supported on desktop (Windows, macOS, Linux) and SHALL report an explicit unsupported error on mobile (Android, iOS). Windows and macOS are the primary targets; Linux is supported through a shared POSIX backend and is lower priority.

#### Scenario: Unsupported platform reported

- **WHEN** a process is requested on a platform that has no process backend
- **THEN** the launch SHALL fail with an unsupported error and SHALL NOT crash or hang

#### Scenario: One POSIX backend for macOS and Linux

- **WHEN** the POSIX backend is compiled for macOS or Linux
- **THEN** it SHALL use the same portable source and only the subset portable across POSIX systems (`posix_spawn`, `posix_spawnattr_*`, `pipe`+`fcntl`, `waitpid`, `kill`) and SHALL NOT require Linux-only calls such as `pipe2` or parent-death signals

#### Scenario: Teardown kills the whole child tree

- **WHEN** the parent kills a child that started subprocesses
- **THEN** the child process group SHALL be terminated so no descendant survives
