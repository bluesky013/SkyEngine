## ADDED Requirements

### Requirement: OpenSSL is provisioned statically per platform

The third-party bootstrap SHALL build OpenSSL (version matching the pinned CPython) as static libraries and install
it under `3RD_PATH/<platform>/openssl` for the supported platforms.

#### Scenario: Static OpenSSL is available

- **WHEN** the OpenSSL package is built for a supported platform
- **THEN** `3RD_PATH/<platform>/openssl` SHALL contain the OpenSSL headers and static libraries (no shared library)

#### Scenario: Missing prerequisites fail clearly

- **WHEN** a required build prerequisite (Perl, NASM, or a POSIX shell/`make`) is missing
- **THEN** the build SHALL fail with a message naming the missing prerequisite

### Requirement: `ssl` and `_hashlib` are built into the static interpreter

The embedded interpreter SHALL expose `ssl` and `_hashlib` without loading an external OpenSSL shared library.

#### Scenario: Import succeeds

- **WHEN** a script imports `ssl` and `_hashlib`
- **THEN** both imports SHALL succeed against the statically linked core

#### Scenario: No external OpenSSL shared library

- **WHEN** the interpreter is loaded at runtime
- **THEN** it SHALL NOT require a shared OpenSSL (`libssl`/`libcrypto`) library to be present

### Requirement: TLS verification works with the bundled CA bundle

The interpreter SHALL use a bundled CA certificate bundle for TLS verification by default.

#### Scenario: HTTPS verification

- **WHEN** a script performs an HTTPS request with the bundled CA bundle available
- **THEN** certificate verification SHALL succeed without a system certificate store

### Requirement: The feature is optional

The SSL/OpenSSL integration SHALL be gated behind a build option so builds without TLS do not carry the dependency.

#### Scenario: Disabled build

- **WHEN** the SSL build option is disabled
- **THEN** OpenSSL SHALL NOT be required and `ssl` SHALL be unavailable as documented
