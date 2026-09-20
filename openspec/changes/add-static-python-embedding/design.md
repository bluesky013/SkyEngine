## Context

The decision is to unify on a **statically linked CPython core** on every platform. Static linkage is anticipated
by the CPython source on Windows: `PC/pyconfig.h` only defines `Py_ENABLE_SHARED`/`MS_COREDLL` when
`Py_NO_ENABLE_SHARED` is absent, and `Include/exports.h` emits `dllimport`/`dllexport` only when
`Py_ENABLE_SHARED` is defined. On Unix, `./configure --disable-shared` produces `libpython3.XX.a`.

The critical consequence: a static core does not export its C-API, so extension modules cannot be loaded as
separate `.pyd`/`.so` files. They must be compiled into the image as builtins. (On Linux/macOS dynamic `.so`
extensions can sometimes bind to a static core when the host exports symbols, but the unified-static model treats
extensions as builtins everywhere.)

Before `Py_NO_ENABLE_SHARED` is set, the Windows static-core spike proved the concept end to end: a
`python313_static.lib` archive links into a minimal embed that runs `import json, re, zlib, math` with no
`python313.dll` present. Build-time facts confirmed: extra Windows libraries `pathcch` and `bcrypt` are required,
and `/LTCG` is needed because `PCbuild` objects use `/GL`.

## Goals / Non-Goals

**Goals**
- Link the CPython core statically into `PythonModule` on all platforms; no Python core shared library at runtime.
- Provide a curated builtin extension set (Tier 1 mandatory, Tier 2 optional) on all platforms.
- Keep exactly one interpreter instance and the existing plugin loading mechanism.
- Keep shipping the standard library as files and resolve it at runtime.

**Non-Goals**
- Replacing module loading or the `Environment` sharing already in place.
- Android/iOS application packaging specifics (owned by `add-android-python-runtime`).
- `_tkinter`, `_wmi`, and test modules.

## Decisions

### D1. Per-platform static core build
- **Win32**: take a disposable copy of `PCbuild/pythoncore.vcxproj`, set `<ConfigurationType>StaticLibrary</ConfigurationType>`
  and prepend `Py_NO_ENABLE_SHARED;` to every `<PreprocessorDefinitions>`; build with MSBuild
  (`/p:TargetName=python3XX_static`). CPython's frozen-module steps still run. (Validated.)
- **Linux/macOS**: `./configure --disable-shared --with-ensurepip=no && make`; install `libpython3.XX.a`.
- **Android/iOS**: cross-build a static core (NDK/Xcode); Android packaging is detailed in
  `add-android-python-runtime`.

Rejected alternative: reusing the existing dynamic artifacts / renaming the import library (only stubs).

### D2. Extension modules are builtins on all platforms
Compile the curated extension sources with the appropriate core-module macros and register them (builtin table or
`PyImport_AppendInittab`) before initialization.
- Tier 1: `unicodedata`, `_decimal`, `_uuid`, `_zoneinfo`, `_elementtree`, `pyexpat`, `_bz2`, `_lzma`,
  `_socket`, `select`, `_overlapped`, `_queue` (Windows-specific names map to their Unix equivalents).
  On Windows static, `_ctypes` is excluded: it needs a static `libffi` (the externals ship only an import
  library) and `ctypes` reads `sys.dllhandle`, which `Py_NO_ENABLE_SHARED` removes. All 12 remaining Tier 1 modules
  were validated importing in a fully static image with no DLLs.
- Tier 2 (optional): `_ssl`, `_hashlib`, `_sqlite3`, `_asyncio`, `_multiprocessing`.
Vendored dependencies (expat, bz2, liblzma, libmpdec, libffi, sqlite) ship in the CPython tree; only Tier 2
`_ssl`/`_hashlib` add an external OpenSSL dependency.

Validated Windows mechanism: for each module, take a disposable copy of `PCbuild/<module>.vcxproj`, set
`<ConfigurationType>StaticLibrary</ConfigurationType>`, replace `<TargetExt>$(PyStdlibPydExt)</TargetExt>` with
`.lib`, and inject `<PreprocessorDefinitions>Py_NO_ENABLE_SHARED;%(PreprocessorDefinitions)</PreprocessorDefinitions>`
into an `ItemDefinitionGroup`; build it with MSBuild (do NOT pass `/p:TargetName`, which leaks into the project's
`ProjectReference` dependencies). Register the module with `PyImport_AppendInittab("<name>", PyInit_<name>)` before
`Py_InitializeFromConfig`. This was proven with `unicodedata` in a fully static image. Note: injecting
`Py_NO_ENABLE_SHARED` into the generated `pyconfig.h` does not work because the build regenerates that header; the
define must be on the compile line/project.

### D3. `PythonModule` stays a shared plugin
The static core is linked only into `PythonModule` (shared), so one interpreter exists and the existing
`DynamicModule`/`StartModule` path is unchanged. Because extensions are built in, no C-API export surface is needed.

### D4. CRT, configuration, and symbol parity
The static core and `PythonModule` must use matching configuration and runtime on each platform (Windows `/MD`
release, `/MDd` debug; Unix a compatible C runtime). A Debug static core is built alongside Release. The static
core is linked only into `PythonModule` to avoid duplicate symbols and multiple interpreters.

### D5. CMake discovery and linking
`Findcpython.cmake` gains a static mode: link the static library, add `Py_NO_ENABLE_SHARED` to the interface, add
platform system libraries (Windows: `ws2_32 crypt32 rpcrt4 advapi32 user32 shell32 ole32 oleaut32 version pathcch
bcrypt iphlpapi`; Unix: the usual `-lpthread -ldl -lm`), and drop the Python shared-library deployment. The build
requires `/LTCG` on Windows because the objects use `/GL`. The consumer also links the Tier 1 module archives and
`liblzma`. A static-core consequence to note: `_ctypes` needs a static `libffi` and is excluded on Windows.

### D6. Standard library stays bundled
`Lib/` (or `lib/python3.XX`) is shipped and the interpreter home points at it. On Windows the embedding sets
`PyConfig.site_import = 0` because a static core has no `sys.winver`; on other platforms `site` is unaffected.

## Risks / Trade-offs

- [Builtin extension compilation is manual and version-specific] -> keep a single module table and an import smoke
  test per Tier 1 module; expect per-platform source-flag differences.
- [Windows static libpython is not an official target] -> the validated `pythoncore` archive approach; document the
  `/GL` + `/LTCG` and system-library requirements.
- [OpenSSL for Tier 2] -> Tier 2 is optional and gated; reuse existing engine OpenSSL if present.
- [CRT/ABI mismatch] -> enforce configuration/runtime parity per platform.
- [Symbol collisions between static CPython and engine libraries] -> link the static core only into `PythonModule`.
- [Cross-compilation complexity on Android/iOS] -> isolate into the platform changes.

## Open Questions

- Confirm the Tier 1 / Tier 2 module set (especially HTTPS `_ssl`/`_hashlib`, `_sqlite3`, `_asyncio`,
  `_multiprocessing`).
- Whether Debug static linking is required for every platform.
- Ordering: complete the Windows static core + builtins first, then Linux/macOS, then Android/iOS.
