# Enforce CMake source ownership

## Why

Jolt tests compile adaptor, render, builder and shared codec implementation sources directly. Animation editor, Sandbox project tests and Enet repeat the same pattern. This bypasses implementation targets and allows tests to compile a different copy of production code with different dependencies and flags. Functional test success did not expose the build-boundary violation.

## What Changes

- Give existing implementations one STATIC target each; module entries and tests link them. Keep existing directories and the Jolt adaptor/render/builder split without adding a core layer.
- Compile the shared physics asset codec once. Give the SDK-independent Android input implementation one owner for native and host-test use.
- Automatically reject duplicate production source compilation, cross-directory source borrowing, production sources in tests and source propagation through interface targets during CMake configuration.
- Preserve shared test fixtures and explicitly checked static-factory compile variants.
- Exercise the guard through SDK-independent CMake regression tests, CTest and a standalone CI job; document the rule in AGENTS.md.

## Capabilities

### New Capabilities
- `cmake-source-ownership`: enforce production translation-unit ownership and test reuse at configuration time.

## Impact

CMake target composition changes in Jolt, Animation editor, Enet, Sandbox and Framework Android input; no C++ behavior or source locations change. Aurora retains its existing targets and module entry: its static factory test gains an exact compile-variant declaration. The common static registration generator marks its output explicitly, including for existing game templates. Existing physics asset/streaming work and the deferred AssetTool configuration change remain separate.
