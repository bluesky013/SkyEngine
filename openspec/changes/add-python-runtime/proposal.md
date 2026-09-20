## Why

The `plugins/python` scaffold cannot build or run: CPython is not provisioned by the third-party bootstrap,
`Findcpython.cmake` points at paths that may not exist, `PythonEngine` hardcodes an absolute interpreter home,
there is no script execution or lifecycle handling, and `PythonModule` is absent from the runtime manifests.
Without a portable, working embedded interpreter, the planned reflection-driven bindings have nothing to run on.

## What Changes

- Provision CPython from **source** through the third-party bootstrap: add a `cpython` package that builds and
  installs into `3RD_PATH/cpython` (headers, import library, shared library, and stdlib) in the layout
  `Findcpython.cmake` expects. Add a `custom_only` package flag so the CMake step is skipped for a package that a
  `custom` build script fully produces.
- Rework `Findcpython.cmake` into an existence-checked, version-parameterized imported target; fail configure with
  a clear message when `SKY_BUILD_CPYTHON=ON` but the package is missing.
- Make `PythonEngine` portable: resolve the interpreter home at runtime (install dir / env), configure module
  search paths and isolation/site options, guard init/finalize, and keep an explicit initialized state.
- Add a script execution surface (run file / run string, propagate errors) and wire `StartArguments`
  (optional startup script) through `PythonModule::Init`.
- Register `PythonModule` in `configs/modules_game.json` and `configs/modules_editor.json` so the built module is
  actually loaded, and make `plugins/python/plugin.json` dependencies reflect real targets.
- Add a `FrameworkTest`-style test that initializes the interpreter, runs a trivial script, and shuts down
  (skipped when cpython is unavailable).

**Non-goals**: C++<->Python bindings and auto-binding (deferred to `add-python-reflection-bindings`), Python
packaging/venv management, and Android/iOS CPython source builds (desktop platforms first).

## Capabilities

### New Capabilities
- `python-runtime`: provisioning, embedding, lifecycle, and script execution for the Python plugin.

### Modified Capabilities
<!-- none -->

## Impact

- `python/third_party.py` (`custom_only`), `cmake/thirdparty.json` (cpython package),
  `cmake/thirdparty/Findcpython.cmake`, `plugins/python/*`, `configs/modules_game.json`,
  `configs/modules_editor.json`, and a new test under `engine/test`.
- Depends on the third-party bootstrap and the plugin/module loading mechanism; no engine API changes.
