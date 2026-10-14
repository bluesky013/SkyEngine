## Execution and ownership

The directory picker closes and starts one `std::async` task. That task owns `IProcess`, argv, the expected descriptor path and shared atomic cancellation/stage state. It never captures the module or touches UI and registry objects. The module polls its future without waiting in `Tick()` and commits a valid result on the main thread.

The existing framework process abstraction replaces shell command construction. This preserves argument boundaries for spaces and shell metacharacters. Native filesystem paths are converted to UTF-8 for process argv, including Chinese parent directories. Python uses unbuffered UTF-8 output. Both pipes are drained fairly; diagnostics and incomplete protocol lines have an 8 KiB limit. Creation has a 30-second timeout, with bounded output draining after exit. Shutdown and destruction cancel and join the worker before unloading code.

## Progress contract

`package_project.py create --progress` emits flushed stdout lines before each real stage:

| Marker | Display |
| --- | --- |
| `templates` | Copying game template |
| `launchers` | Creating platform launchers |
| `directories` | Creating asset directories |
| `descriptor` | Writing project descriptor |

Each marker uses the `[sky-create] ` prefix. The parser handles split reads and CRLF. No percentage is implied: elapsed time remains visible while a stage runs. The status occupies its own themed row, separate from the recent-project heading. Busy state disables Add/New/Open/Remove/Delete while preserving Quit. No new toolbar or settings form is introduced.

## Results and failure handling

Only exit code zero plus a readable, named, compatible descriptor updates the registry. Missing Python, timeout, invalid descriptor and nonzero exit restore hub actions and show an error, with a bounded output tail in the status and log. Existing target directories are rejected. Failed creation does not delete partially created files. Success refreshes recent projects and explicitly tells the user to select the project and click Open.

## Validation

`SandboxProjectTest` compiles the module implementation into a headless test executable. It drives the same picker-result callback and completion methods as the application, with a temporary working directory to isolate the recent-project registry. It exercises real subprocesses for progress, timeout, launch failure, pipe saturation, invalid results and successful template generation with spaces and `&` in the name. Setting `SKY_KEEP_PROJECT_TEST_OUTPUT=1` retains the successful temporary project for inspection.

The headless test does not initialize a GPU or synthesize native mouse clicks. A bounded `SandboxEditor --frames` run separately checks host/module startup when the local graphics environment permits it.
