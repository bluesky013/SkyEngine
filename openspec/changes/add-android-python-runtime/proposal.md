## Why

Per the unified-static decision, every platform links the CPython core statically and builds the required
extension modules as builtins. Android additionally has packaging constraints: native code lives in the APK under
`lib/<abi>/`, and the standard library cannot be read as loose files from the APK. CPython 3.13 ships an Android
cross-build helper (`Android/android.py`), and the engine bootstrap already targets Android (Ninja, NDK 27,
android-31).

## What Changes

- Cross-build a **static libpython** for Android (NDK, `arm64-v8a`) and install it under `3RD_PATH/Android/cpython`
  alongside headers and the standard library.
- Build the Tier 1 extension set as builtins for Android (same curated set as the other platforms).
- Package the standard library so the interpreter can import it from the APK (a zip on `sys.path`, or an extraction
  to the app's files directory on first run), and add it to the module search path.
- Rework `Findcpython.cmake` for Android to link the static archive with existence checks.
- Build `PythonModule` for Android and include it, plus the standard library, in the APK; list the module for the
  Android runtime.
- Enforce the toolchain constraints (NDK `28.1.13356709`, `android-31`, `arm64-v8a`) with clear failures.

**Non-goals**: iOS specifics, additional ABIs (`armeabi-v7a`, `x86_64`) in the first iteration.

## Capabilities

### New Capabilities
- `android-python-runtime`: cross-building, bundling, and packaging the statically linked CPython for the Android
  launcher.

### Modified Capabilities
<!-- none -->

## Impact

- `python/build_cpython.py` (Android static path), `cmake/thirdparty.json` (add Android to the cpython package),
  `cmake/thirdparty/Findcpython.cmake`, `plugins/python`, the Android launcher packaging, and runtime configuration.
- Depends on `add-static-python-embedding` (static core + builtin extension model) and the Android SDK/NDK.
- Requires the pinned NDK (`28.1.13356709`); configuration/build SHALL fail clearly when it is missing.
