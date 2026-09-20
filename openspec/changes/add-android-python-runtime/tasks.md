## 1. Static core cross-build (configure + CMake)

- [x] 1.1 Generate the Android platform configuration by running CPython `configure --host=aarch64-linux-android --disable-shared` under a POSIX shell (Git Bash on Windows) with the NDK clang (`pyconfig.h`, `Modules/config.c`, `Makefile`)
- [x] 1.2 Prototype: CMake + Ninja + NDK compile the core into an AArch64 `libpython3.XX.a` (no `make`, no `android.py`)
- [x] 1.3 Parse the `Makefile` object lists to drive the CMake source list
- [x] 1.4 Reuse the host-built frozen module headers (`Python/frozen_modules/*.h`) in the Android compile
- [x] 1.5 Install `include/`, `libpython3.XX.a`, and the standard library into `3RD_PATH/Android/cpython`
- [x] 1.6 Wire the pipeline into `python/build_cpython.py` / the bootstrap (`cmake/thirdparty.json` already lists Android)
- [x] 1.7 Add Android to the `cpython` package platforms and dispatch the platform in the custom script

## 2. Builtin extensions on Android

- [x] 2.1 Build the Tier 1 extension set as builtins for Android via `Modules/Setup.local` (`*static*`): array, _csv, _json, _random, _struct, binascii, cmath, math, zlib, select, socket, unicodedata, _zoneinfo plus `_decimal` (vendored libmpdec); `pyexpat`/`_elementtree` (vendored expat) and `_lzma`/`_bz2`/`_uuid`/`_ctypes` deferred
- [ ] 2.2 Add a Tier 2 option (reuse the cross-platform Tier 2 gate)

## 3. CMake discovery

- [x] 3.1 Resolve and link the static archive for Android in `cmake/thirdparty/Findcpython.cmake` with an existence check (non-MSVC branch)
- [x] 3.2 Add the Android system libraries (`log`) and surface a clear error when the package is missing

## 4. Standard library packaging

- [x] 4.1 Build a `python3XX.zip` for Android (549 entries, 2.6 MB), excluding `test`/`idlelib`/`tkinter`/`turtledemo`/`lib2to3`/`ensurepip`
- [x] 4.2 Add the packaged standard library to `module_search_paths` (via `SKY_PYTHON_ZIP`) in the interpreter configuration

## 5. Plugin and app packaging

- [x] 5.0 No separate GameActivity provisioning needed: AGP `prefab true` + `androidx.games:games-activity` provides `find_package(game-activity CONFIG)` when the engine is built through the launcher's Gradle `externalNativeBuild`
- [ ] 5.1 Build `PythonModule` for Android via the launcher Gradle/AGP `externalNativeBuild` with `-DSKY_BUILD_PYTHON=ON` (prerequisite: full Android 3rd-party in `3RD_PATH`), linking `libpython3.13.a`
- [ ] 5.2 Include `PythonModule.so` and the standard library in the Android APK
- [ ] 5.3 Add the Python module to the Android runtime module list

## 6. Tests and verification

- [ ] 6.1 Add an on-device smoke test (via `adb`) that initializes the interpreter and imports a standard module
- [ ] 6.2 Verify the reflection bindings (`sky`) work on Android
- [x] 6.3 Resolve the NDK from the environment (`ANDROID_NDK_HOME`/`ANDROID_NDK_ROOT` -> `ANDROID_NDK_VERSION` -> auto-detect newest); verified locally against `D:/Android/ndk/28.1.13356709`
