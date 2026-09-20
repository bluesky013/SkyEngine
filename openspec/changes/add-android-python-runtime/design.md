## Context

The unified-static decision applies to Android as well: the CPython core is linked statically into `PythonModule`
and the required extension modules are builtins. Android adds packaging constraints:

- Native code lives in the APK under `lib/<abi>/`; the dynamic linker load-namespace restricts where a library may
  be loaded from.
- The app cannot read arbitrary loose `.py` files from the APK; the standard library must be extracted to the app's
  files directory or placed on `sys.path` as a zip.
- CPython 3.13 ships `Android/android.py` for cross-building (`configure-build`/`make-build`,
  `configure-host`/`make-host --host=aarch64-linux-android`).
- The engine bootstrap targets Android with Ninja, NDK `27.0.12077973`, `android-31`, `arm64-v8a`, `c++_static`.

## Goals / Non-Goals

**Goals**
- Cross-build a static CPython core for Android and install it into `3RD_PATH/Android/cpython`.
- Make the Tier 1 extension set available as builtins on Android.
- Make the standard library importable from the Android app at runtime.
- Package `PythonModule` and the standard library into the APK.

**Non-Goals**
- iOS specifics, additional ABIs (`armeabi-v7a`, `x86_64`) in the first iteration.

## Decisions

### D1. Cross-build a static core with `configure` + our own CMake
`Android/android.py` needs `make` and a full POSIX toolchain, so it cannot run on Windows. Instead: use CPython's
`configure` only to generate the platform configuration, then compile with **CMake + Ninja + the NDK toolchain**
(which works from Windows).

1. Run `configure --host=aarch64-linux-android --build=<posix-triplet> --disable-shared --without-ensurepip` with the
   NDK clang. `configure` is a shell script (no `make`), so it runs under Git Bash on Windows. It produces
   `pyconfig.h`, `Modules/config.c`, and `Makefile` (object lists).
2. A CMake project compiles the core sources (object lists parsed from `Makefile`) into `libpython3.XX.a`, reusing
   the host-built frozen module headers (`Python/frozen_modules/*.h`) and supplying the configured `PREFIX` /
   `VERSION` / `VPATH` / `PLATLIBDIR` defines.
3. Install `include/`, `libpython3.XX.a`, and the standard library into `3RD_PATH/Android/cpython`.

Validated prototype: `configure` + CMake + Ninja + NDK `28.1.13356709` produced an **AArch64** ELF
`libpython3.13.a` for `arm64-v8a` from Windows (no `make`, no `android.py`). The remaining work is packaging this as
a reproducible pipeline (parse the Makefile object lists, reuse frozen headers, builtin extensions, install layout).

Host requirement: a POSIX **shell** is still needed for `configure` (Git Bash on Windows is sufficient); `make` is
not required.

### D2. Static core and builtin extensions
Link the static core into `PythonModule`; build the Tier 1 extension set as builtins (same curated set as other
platforms, using the Android/Unix module names). No shared `libpython` and no external extension modules at
runtime.

### D3. Standard library packaging
Ship the standard library so it is importable from the APK: build a `python3XX.zip` and add it to
`module_search_paths` (default), with extraction to the app's files directory as a fallback for modules that need
on-disk files.

### D4. `Findcpython.cmake` Android handling
Resolve the static archive for Android with an existence check and link it, adding the Android system libraries and
`-llog` as needed. No shared-library deployment.

### D5. Runtime configuration on Android
Resolve the packaged standard library (zip or extracted directory) from the app location and add it to
`module_search_paths`. `site_import` may stay enabled (the Windows `sys.winver` issue does not occur). The static
core is inside `PythonModule.so`.

### D6. Toolchain and ABI constraints, resolved from the environment
Resolve the NDK from the environment instead of a hardcoded path: `ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT`
(explicit NDK path), else `ANDROID_NDK_VERSION` (a version under `<sdk>/ndk`), else auto-detect the newest NDK
installed under `<sdk>/ndk`. The SDK root comes from `ANDROID_HOME` / `ANDROID_SDK_ROOT` / `ANDROID_SDK` (its value
is normalized). Preferred default version `27.0.12077973`; `android-31`, `arm64-v8a`, `c++_static`. Configuration
SHALL fail clearly when no NDK can be resolved. Additional ABIs are a follow-up.

### D7. APK packaging
Ensure the Android launcher packaging includes `PythonModule.so` and the standard library zip in the APK, and that
`PythonModule` is present in the Android runtime module list.

## Risks / Trade-offs

- [Static cross-build for Android is not a first-class CPython target] -> rely on `android.py` plus `--disable-shared`; isolate platform flags.
- [Standard library zip vs on-disk modules] -> start with zip; extraction fallback.
- [APK size growth] -> exclude tests and unused modules from the zip.
- [Builtin module compilation on Android] -> same curated table as other platforms; smoke-test each Tier 1 module on device.
- [Pinned NDK not installed locally] -> the build cannot be verified until NDK `27.0.12077973` is installed.

## Open Questions

- Zip-on-`sys.path` versus extraction as the default.
- Which additional ABIs are required.
- Whether to trim the packaged standard library.
- Whether Android needs different Tier 1 module names (`select`, `_socket` sources differ by platform).
