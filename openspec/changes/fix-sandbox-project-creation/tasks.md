## Implementation

- [x] Replace synchronous shell invocation with an owned asynchronous process task.
- [x] Add bounded pipe draining, timeout and shutdown cancellation.
- [x] Add actual stage reporting and elapsed time to a dedicated hub status row.
- [x] Disable conflicting actions while busy and validate successful results before registration.
- [x] Add headless regression coverage through the real module flow.

## Verification

- [x] Build SandboxModule, SandboxEditor, SandboxProjectTest and EditorShellTest.
- [x] Run project-creation and shell regressions.
- [x] Create and retain a real project under a temporary directory; inspect descriptor and generated files.
- [x] Run bounded editor startup and record any environment limitations.
- [x] Validate the OpenSpec change and review the final diff.

## Results (2026-10-15)

- SandboxProjectTest: 7/7 passed, including stage/elapsed updates, shutdown, timeout, missing Python, both output pipes, invalid descriptors, Chinese directories and real creation with spaces/`&`.
- EditorShellTest: 33/33 passed. Python export-schema tests: 2/2 passed.
- A `Sandbox Verify & Space` project was created and retained under the user temporary directory. Its descriptor, generated module source, platform launchers, assets/configs/cache and isolated recent-project registry were verified.
- `SandboxEditor --frames 12`: Vulkan hub initialized, module loaded, process exited with code 0. Native mouse-click/visual acceptance remains a manual check; creation was exercised through the real module callback in the headless harness.
- OpenSpec strict validation and `git diff --check` passed. No commit or archive performed.
