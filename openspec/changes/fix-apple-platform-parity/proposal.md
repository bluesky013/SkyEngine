## Why

The OHOS parity review found that Apple platform code compiles but its native test entry, architecture selection and export configuration do not consistently reach the real build or artifact. Cocoa also busy-polls when every window is inactive, while UIKit dialog behavior has no native regression coverage.

## What Changes

- Restore the UIKit production-target fixture with the pinned GoogleTest checkout and explicit SFMT/RapidJSON/LZ4 dependencies; make path assertions portable on macOS.
- Bind Apple dependency architecture and simulator SDK identity, validate real artifacts and tool inputs, and preserve truthful preflight failures.
- Carry Apple export variant and version fields through configuration, build, metadata and artifact collection without rewriting project-owned sources.
- Implement interruptible Cocoa inactive waiting and exercise native text/IME transitions.
- Add UIKit native dialog coverage and a real-device regression path, then complete a separate signed archive/export flow that stages resources before signing.
- Record actual host/device evidence separately from the existing P02 GPU and long-running acceptance matrix.

## Capabilities

### New Capabilities

None; the change extends existing canonical domains.

### Modified Capabilities

- `build`: architecture/SDK-bound Apple dependencies, real artifact/tool validation and reproducible production-target UIKit tests.
- `platform`: Cocoa inactive waits and native UIKit dialog termination/owner lifecycle regression contracts.
- `project-packaging`: selected Apple configuration/version metadata and separate signed archive/export/install flow.

## Impact

Existing Python tooling, CMake dependency helpers and iOS test fixture, project-owned Apple templates, Framework Cocoa/UIKit implementations and colocated tests. No target/module ownership changes or directory moves. No new DLL code leases. Existing unsigned collection remains available; no store upload or credential creation is implied. The user authorized implementation after reviewing the parity findings; commit and archive still require their own confirmation.
