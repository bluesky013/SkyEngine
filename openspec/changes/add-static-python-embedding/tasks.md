## 1. Static core build (all platforms)

- [x] 1.1 Windows: build `pythoncore` with `Py_NO_ENABLE_SHARED;Py_BUILD_CORE` (patched `StaticLibrary` project) into `python3XX_static.lib`
- [x] 1.2 Windows: install the static library and headers into `3RD_PATH/cpython/libs/<config>`
- [x] 1.3 Prove static linking with a minimal embed smoke (initialize + run a string) before wiring the plugin
- [x] 1.4 Windows: build a Debug static core and modules, matching `/MD`/`/MDd` (validated with a Debug static spike)
- [ ] 1.5 Linux/macOS: build a static core (`./configure --disable-shared`) and install `libpython3.XX.a`
- [ ] 1.6 Android/iOS: add a static core cross-build (details coordinated with `add-android-python-runtime`)

## 2. Builtin extension modules (all platforms)

- [x] 2.1 Compile the Tier 1 extension set as builtins on Windows (`build_cpython.py`; `_ctypes` excluded on Windows static) (`unicodedata`, `_decimal`, `_uuid`, `_zoneinfo`, `_elementtree`, `pyexpat`, `_bz2`, `_lzma`, `_socket`, `select`, `_overlapped`, `_queue`)
- [ ] 2.2 Add a Tier 2 build option and, when enabled, build/register `_ssl`, `_hashlib`, `_sqlite3`, `_asyncio`, `_multiprocessing` (resolve OpenSSL)
- [ ] 2.3 Add the Tier 1 builtin set for Linux/macOS (equivalent module names)
- [x] 2.4 Add an import smoke for every Tier 1 module on Windows; document the excluded set (`_ctypes`)

## 3. CMake discovery and linking

- [x] 3.1 Static mode in `cmake/thirdparty/Findcpython.cmake`: link the static library and add `Py_NO_ENABLE_SHARED` on Windows
- [x] 3.2 Add platform system libraries (Windows: `ws2_32 crypt32 rpcrt4 advapi32 user32 shell32 ole32 oleaut32 version pathcch bcrypt`; Unix: `pthread dl m`) and require `/LTCG` on Windows
- [x] 3.3 Stop deploying a Python shared library

## 4. Plugin and runtime

- [x] 4.1 Compile `plugins/python` with `Py_NO_ENABLE_SHARED` (Windows) and link the static core on all platforms
- [x] 4.2 Deploy the standard library next to the output; set `PyConfig.site_import = 0` on Windows and keep `ResolveHome`
- [x] 4.3 Confirm `PythonModule` stays shared so exactly one interpreter exists

## 5. Tests

- [x] 5.1 Update `PythonRuntimeTest` to run against the static core without a Python shared-library dependency
- [x] 5.2 Add a Tier 1 import smoke test
- [x] 5.3 Keep the existing reflection-binding tests green
- [x] 5.4 Add an embedded text-script test run through `PythonModule` (imports builtins, JSON/re/zlib/collections, comprehensions)

## 6. Verification

- [x] 6.1 Bootstrap the static cpython package and configure/build with `-DSKY_BUILD_PYTHON=ON`
- [x] 6.2 Run the tests and confirm no Python shared library is loaded (module list / dependency dump)
- [x] 6.3 Verify Debug/Release configuration parity (both static images import the Tier 1 set)
- [x] 6.4 Style checks: ASCII-only, no new long lines, no new warnings
