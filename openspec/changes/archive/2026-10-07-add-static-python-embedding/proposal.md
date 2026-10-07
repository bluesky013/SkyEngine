## Why

The Python plugin currently links CPython as a shared library (`python3XX.dll`/`libpython3XX.so`), which adds a
runtime library dependency and platform-specific packaging friction. The decision is to **unify on static**
everywhere: link the CPython core statically into `PythonModule` on every platform, so no Python core library is
loaded at runtime and the interpreter is self-contained. On a static core the C-API is not exported, so extension
modules must be compiled into the image as builtins rather than loaded as separate `.pyd`/`.so` files. The standard
library still ships as files (accepted).

## What Changes

- Build a **static libpython** from CPython source on every platform:
  - Win32: build `pythoncore` with `Py_NO_ENABLE_SHARED;Py_BUILD_CORE` and archive it (validated).
  - Linux/macOS: `./configure --disable-shared` to produce `libpython3.XX.a`.
  - Android/iOS: cross-build a static core (NDK/Xcode), deferring the platform packaging details.
- Build a **curated extension set as builtins** (Tier 1 mandatory, Tier 2 optional) on every platform, because a
  static core cannot serve dynamically loaded extension modules.
- Install a static layout under `3RD_PATH/cpython` (headers, static library, standard library).
- Rework `Findcpython.cmake` to link the static library, define `Py_NO_ENABLE_SHARED`, and add the platform system
  libraries; stop deploying a Python shared library.
- Compile `plugins/python` with `Py_NO_ENABLE_SHARED`; keep `PythonModule` shared so exactly one interpreter exists.
- Continue bundling the standard library and pointing the interpreter home at it; on Windows set
  `PyConfig.site_import = 0` (a static core has no `sys.winver`).
- Update tests to run against the statically linked core.

**Non-goals**: replacing the plugin loading mechanism and the Android/iOS packaging specifics (tracked by
`add-android-python-runtime`). The dynamic path is removed as the target model, not kept as a fallback.

## Capabilities

### New Capabilities
- `python-static-embedding`: static linkage of the CPython core (with a curated builtin extension set) on all
  platforms.

### Modified Capabilities
<!-- none -->

## Impact

- `cmake/thirdparty.json`, `cmake/thirdparty/Findcpython.cmake`, `python/build_cpython.py`, `plugins/python`, and the
  runtime deployment of the standard library.
- Per-platform build steps for the static core and the builtin extensions; Android/iOS cross-compilation.
- Depends on `python-runtime` (embedding/lifecycle) and the third-party bootstrap.
