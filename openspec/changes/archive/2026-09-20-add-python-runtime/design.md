## Context

`plugins/python` already has a build target (`PythonModule`), an `IModule` implementation, and a `PythonEngine`
singleton, but it is disabled and non-functional: the bootstrap does not produce CPython, `Findcpython.cmake`
declares an imported target with possibly-missing paths, `PythonEngine.cpp` hardcodes `D:\Code\3rd\cpython`, there
is no script execution, and the module is missing from the runtime manifests. The bootstrap supports a `custom`
python step per package (`cmake/thirdparty.json`, e.g. `ispc_texcomp`) and a `source` subdir, but always runs a
CMake build unless a package is marked `is_tool`.

## Goals / Non-Goals

**Goals:**
- A clean-machine `python/third_party.py -p <desktop>` produces a usable CPython under `3RD_PATH/cpython`.
- `-DSKY_BUILD_PYTHON=ON` builds the plugin against that package on a different machine without edits.
- A guarded lifecycle and a minimal, testable script-execution API.

**Non-Goals:**
- C++<->Python bindings and auto-binding (change `add-python-reflection-bindings`).
- CPython source builds for Android/iOS (deferred; package lists desktop platforms).
- venv/pip/package management and embedding the full standard library everywhere.

## Decisions

### D1. Build CPython with a `custom` step and add `custom_only`
Add a `cpython` package to `cmake/thirdparty.json` with `custom: <script>` (mirroring `ispc_texcomp`) and a new
`custom_only: true` flag; extend `python/third_party.py` so `process_package` returns after the custom step and
never runs CMake on the CPython tree. `is_tool: true` already skips the CMake build, but it semantically means
"tool"; `custom_only` states the intent (a library fully produced by its custom step). Alternative rejected:
injecting a `CMakeLists.txt` via `cmake/patches/cpython.patch` to drive `PCbuild`/`configure` from CMake - more
fragile and harder to debug.

### D2. Platform build strategy
- **Win32**: `PCbuild\build.bat -p x64` for Debug and Release; install `Include/` + `PC/pyconfig.h` headers,
  `lib/Release/python3XX.lib`, `lib/Debug/python3XX_d.lib`, `bin/Release/python3XX.dll`, and the stdlib
  (`Lib/`, `DLLs/`).
- **MacOS-x86 / MacOS-arm / Linux**: `./configure --prefix=<stage> --enable-shared` + `make install`, then
  normalize into the same `include/`, `lib/`, `bin/`, stdlib layout.
- **Android / IOS**: excluded from the package `platforms` list for now.

The custom script derives the engine root from its own path and installs into `<engine>/build_3rd/<platform>/
cpython`; it receives `-p <platform>` from the bootstrap. A custom `-o` output root is not honored by this script
(recorded as a limitation).

### D3. `Findcpython.cmake` becomes checked and version-parameterized
Replace hardcoded `python313` paths with a `CPYTHON_VERSION` cache variable (default `3.13`) used to compose file
names, and `FATAL_ERROR` when `SKY_BUILD_CPYTHON` is on but the expected include/lib/runtime files are missing.
This turns a confusing link error into an actionable configure error.

### D4. Resolve the interpreter home at runtime
Drop the wide-string literal. Resolve the home as: `SKY_PYTHON_HOME` environment override, else a path derived
from the module/executable location pointing at the installed `cpython` tree; set it with
`PyConfig_SetBytesString` (UTF-8) and also set `module_search_paths`/`base_executable`.

### D5. Guarded lifecycle
`PythonEngine::Init` returns early if already initialized. `Shutdown` finalizes only when this engine performed a
successful initialization, then clears the flag. Keep the engine usable across multiple module Init/Shutdown
cycles in tests.

### D6. Script execution API
Add `RunFile(const std::string &path)` and `RunString(std::string_view source)` that acquire the GIL, execute via
`PyRun_File`/`PyRun_String` in the main module namespace, and on failure capture the Python exception, log it, and
return false. `PythonModule::Init` reads an optional startup script from `StartArguments` and runs it.

### D7. Runtime manifests stay in sync with the compile switch
Do not list `PythonModule` in the default manifests. The plugin is opt-in (`SKY_BUILD_PYTHON` defaults to off) and
`ModuleManager::LoadModules` reports a missing library as a load error, so listing an unconditionally absent module
would log a failure on every default run. Enabling the plugin for real use therefore requires a second, explicit
step: add the module entry to the active runtime manifest. Default manifests stay clean, matching the repository
rule that compile-time plugin selection and runtime module loading are separate systems.

### D8. Require the host environment before initialization
`Environment::IsAttached()` (set by `Environment::Attach`) is checked in `PythonEngine::Init`, which refuses to start
the interpreter without it. This guarantees the plugin shares the host's `SerializationContext` through
`Singleton`/`Environment` instead of silently creating a private one. Production (`ModuleManager::LoadModules` calls
`StartModule(Environment::Get())`) and the tests (`PythonAttachEnvironment`) both attach first.

## Risks / Trade-offs

- [CPython source builds are slow and platform-specific] -> desktop-only first; the custom script is the single
  place to adapt per platform; the package is cached by the bootstrap metadata.
- [Version drift between the built CPython and hardcoded file names] -> introduce `CPYTHON_VERSION` and check
  existence; keep `Findcpython` as the single place naming artifacts.
- [Windows build needs the CPython `PCbuild` prerequisites (MSVC + optional deps)] -> document prerequisites;
  `build.bat` fetches its own externals.
- [`custom` step does not receive the output root] -> derive from the engine root and document the `-o` limitation,
  or extend the bootstrap to pass the output path (follow-up).
- [Embedding the stdlib increases output size] -> acceptable for tooling/editor; can trim later.

## Open Questions

- Exact CPython tag to pin (target `3.13.x` to match `Findcpython`; confirm the latest 3.13 patch).
- Whether Debug CPython is required for Debug configs or Release is used for both.
- Where startup scripts live and how they are passed through `StartArguments`.
- Timeline for Android/iOS CPython builds.
