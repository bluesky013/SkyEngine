## 1. Third-party bootstrap: cpython from source

- [x] 1.1 Add a `custom_only` package flag to `python/third_party.py` that runs the custom step and skips the CMake configure/build
- [x] 1.2 Add a `cpython` package to `cmake/thirdparty.json` (`custom` script, `custom_need_platform`, `custom_only`, desktop platforms)
- [x] 1.3 Add the CPython custom build script that builds and installs `include/`, `lib/`, `bin/`, and the stdlib into `<3RD_PATH>/cpython`
- [x] 1.4 Support the Windows path (`PCbuild\build.bat`) and the Unix path (`./configure --enable-shared && make install`)

## 2. CMake discovery

- [x] 2.1 Rework `cmake/thirdparty/Findcpython.cmake`: `CPYTHON_VERSION`, composed artifact names, existence checks
- [x] 2.2 `FATAL_ERROR` with the expected path when `SKY_BUILD_CPYTHON=ON` and the package is missing
- [x] 2.3 Confirm `plugins/python/CMakeLists.txt` links `3rdParty::cpython` and that `sky_find_3rd` is gated by `SKY_BUILD_CPYTHON`

## 3. PythonEngine portability and lifecycle

- [x] 3.1 Remove the hardcoded interpreter home; resolve from `SKY_PYTHON_HOME` or the install location (UTF-8)
- [x] 3.2 Configure module search paths / base executable; set site/isolation options explicitly
- [x] 3.3 Guard `Init` (idempotent) and `Shutdown` (finalize only if initialized); track and clear state
- [x] 3.4 Add `RunFile`/`RunString` with GIL handling and Python error reporting

## 4. Module integration

- [x] 4.1 Run an optional startup script from `StartArguments` in `PythonModule::Init`; propagate failure
- [x] 4.2 Keep the default runtime/editor manifests clean of the opt-in module; enabling the plugin requires an explicit manifest entry
- [x] 4.3 Make `plugins/python/plugin.json` dependencies reflect real targets (drop placeholders that do not resolve)
- [x] 4.4 Enforce the host-environment attach contract in `PythonEngine::Init` via `Environment::IsAttached()`

## 5. Tests

- [x] 5.1 Add a test that initializes the interpreter, runs a string, reads a result, and shuts down
- [x] 5.2 Skip the test with a clear message when cpython is not available

## 6. Verification

- [x] 6.1 Run the third-party bootstrap for cpython on the host desktop platform
- [x] 6.2 Configure and build with `-DSKY_BUILD_PYTHON=ON`; run the new test
- [x] 6.3 Style checks: ASCII-only, no new long lines, no new warnings
