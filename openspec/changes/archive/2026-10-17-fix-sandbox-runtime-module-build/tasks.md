## 1. Configure the runtime module build dependencies

- [x] 1.1 Read configs/modules_editor.json after engine/plugin targets are defined and add SandboxEditor build dependencies for each existing configured module target.
- [x] 1.2 Guard absent SandboxEditor/module targets and track the checked-in manifest for reconfiguration without enabling plugins or changing loader/link behavior.
- [x] 1.3 Inspect the generated host dependency graph and selected Debug configuration, including enabled BulletPhysicsModule and RecastNavigation.

## 2. Rebuild and verify the reported startup

- [x] 2.1 Rebuild the stale runtime modules and SandboxEditor from the same Debug tree/configuration; record resulting binary/path identity evidence.
- [x] 2.2 Execute the user's actual Debug startup in a bounded session and record exit/crash/log evidence for the former PostTick/RTTI fault.
- [x] 2.3 Separate module/startup success from any remaining GPU/display/native-platform limitation; retain unrelated platform acceptance gates.

## 3. Close the scoped change

- [x] 3.1 Run strict OpenSpec validation and a build-only scope review.
- [x] 3.2 Update canonical §20 with actual root-cause, build and runtime results without rewriting historical checkpoints.
- [x] 3.3 Complete tracked/new-change whitespace checks and freeze the scoped documents.

Implementation and Debug startup verification passed (2026-10-17). Xcode Debug built only SandboxEditor and automatically rebuilt Bullet/Recast, reporting BUILD SUCCEEDED. Xcode direct dependencies and Ninja link-order-only dependency queries each verified all 7 manifest module targets; normal Ninja configuration also passed.

An unsandboxed LLDB `--frames 120` run loaded all seven modules, initialized Metal on Intel Iris Plus 655 and FreeType, entered hub mode, and its process 10584 exited 0. LLDB's outer command returned 1 only because a later backtrace command had no live process. A direct `--frames 600` run also exited 0, with seven successful module loads and Metal/FreeType initialization. No new SandboxEditor crash report appeared. The unbounded Debug hub was then left running in session 51508 and remained alive after approximately one minute.

The generated Ninja RERUN_CMAKE inputs include configs/modules_editor.json, confirming manifest change tracking. New-change strict validation, tracked whitespace and the final unified no-index whitespace check of all 237 untracked files passed. The 262-file protected audit retained 261 byte-identical files plus the prior renderer CMake folder-label-only exception; this repair changes no renderer/RHI/RDG implementation. ModuleManager failed-Init metadata/library ownership and duplicate Cocoa class warnings remain separate follow-ups in canonical §20.4. Windows/full display/cross-DPI/GPU acceptance remains open. No commit or OpenSpec archive is authorized.
